#!/usr/bin/env python3
import argparse
import logging
import os
import time
import json
import pandas as pd
import numpy as np
from collections import defaultdict
from datetime import datetime
from pathlib import Path
import asyncio
import signal

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler("trading_engine.log"),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger("trading_engine")


class TradingEngine:
    """
    Trading engine that implements HFT strategies based on order book data.
    
    This engine uses asyncio for intra-process communication with the Order Book Simulator,
    receives market data via a queue, and executes trades based on the specified strategy.
    """
    
    def __init__(self, market_data_queue: asyncio.Queue, order_queue: asyncio.Queue = None,
                 initial_capital=1000000.0, output_dir='trading_output'):
        """
        Initialize the trading engine.
        
        Args:
            market_data_queue: Queue to receive market data updates
            order_queue: Queue to send orders (optional)
            initial_capital: Initial capital for trading
            output_dir: Directory to save trading output
        """
        self.market_data_queue = market_data_queue
        self.order_queue = order_queue
        self.initial_capital = initial_capital
        self.output_dir = Path(output_dir)
        
        # Create output directory if it doesn't exist
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Portfolio state
        self.cash = initial_capital
        self.positions = {}  # symbol -> {quantity, avg_price, realized_pnl}
        self.trades = []
        self.orders = {}  # order_id -> order
        self.next_order_id = 1
        self.next_trade_id = 1
        
        # Market data cache
        self.market_data = {}  # symbol -> latest market data
        self.order_books = {}  # symbol -> order book snapshot
        self.price_history = defaultdict(list)  # symbol -> list of price points
        
        # Strategy parameters
        self.strategy_params = {
            'lob_imbalance_threshold': 2.0,  # Bid volume > Ask volume by this factor
            'min_consecutive_ticks': 5,      # Number of consecutive ticks with imbalance
            'profit_target_pct': 0.1,        # Target profit percentage
            'stop_loss_pct': 0.05,           # Stop loss percentage
            'order_timeout_ticks': 10,       # Cancel unfilled orders after this many ticks
            'position_size_pct': 0.01,       # Position size as percentage of capital
            'max_positions': 10,             # Maximum number of concurrent positions
        }
        
        # Strategy state
        self.imbalance_counters = defaultdict(int)  # symbol -> consecutive ticks with imbalance
        self.order_age = {}  # order_id -> ticks since placement
        self.active_strategies = {}  # symbol -> strategy state
        
        # Performance metrics
        self.metrics = {
            'total_pnl': 0.0,
            'realized_pnl': 0.0,
            'unrealized_pnl': 0.0,
            'num_trades': 0,
            'winning_trades': 0,
            'losing_trades': 0,
            'start_time': time.time(),
            'end_time': None,
        }
        
        # Running flag
        self.running = True
        
        logger.info(f"Trading engine initialized with {initial_capital:.2f} capital")
    
    async def process_market_data(self):
        """
        Process market data updates from the queue.
        
        This method runs as an asyncio task and processes market data
        updates from the queue until the end of data is signaled.
        """
        try:
            logger.info("Starting market data processing")
            updates_received = 0
            last_log_time = time.time()
            
            while self.running:
                # Get the next market data update from the queue
                logger.debug(f"Waiting for market data update from queue (queue size: {self.market_data_queue.qsize()})")
                update = await self.market_data_queue.get()
                updates_received += 1
                
                # Log progress periodically
                current_time = time.time()
                if current_time - last_log_time >= 5.0:  # Log every 5 seconds
                    logger.info(f"Processed {updates_received} market data updates in the last 5 seconds")
                    updates_received = 0
                    last_log_time = current_time
                
                # Check for end of data signal
                if update is None:
                    logger.info("Received end of data signal")
                    self.running = False
                    break
                
                # Process the update
                symbol = update['symbol']
                
                # Process the update with our trading strategy
                # Note: process_market_update will update market_data and price_history
                await self.process_market_update(update)
                
                # Age all orders
                for order_id in list(self.order_age.keys()):
                    self.order_age[order_id] += 1
                    # Cancel old orders
                    if self.order_age[order_id] >= self.strategy_params['order_timeout_ticks']:
                        await self.cancel_order(order_id)
                
                # Mark task as done
                self.market_data_queue.task_done()
        
        except asyncio.CancelledError:
            logger.info("Market data processing task cancelled")
        except Exception as e:
            logger.error(f"Error processing market data: {e}")
        finally:
            logger.info("Market data processing ended")
            
            # Calculate final performance metrics
            self.calculate_performance_metrics()
            
            # Save performance summary
            await self.save_performance_summary()
    
    async def process_market_update(self, update):
        """
        Process a market data update and execute trading strategy.
        
        Args:
            update: Market data update dictionary
        """
        symbol = update['symbol']
        
        # Update market data cache - skip debug logging for better performance
        self.market_data[symbol] = update
        
        # Add to price history - use a more efficient approach to limit size
        if symbol not in self.price_history:
            self.price_history[symbol] = [update]
        else:
            self.price_history[symbol].append(update)
            # Limit history size for better memory usage and performance
            if len(self.price_history[symbol]) > 100:  # Only keep last 100 updates
                self.price_history[symbol] = self.price_history[symbol][-100:]
        
        # Check if we have enough history for this symbol
        if len(self.price_history[symbol]) < 10:
            return
        
        # Execute trading strategy
        await self.lob_imbalance_strategy(update)
    
    async def lob_imbalance_strategy(self, update):
        """
        Implement a LOB imbalance strategy.
        
        Strategy:
        1. Monitor LOB imbalance (bid volume > ask volume)
        2. If imbalance exceeds threshold for consecutive ticks, place a buy order
        3. Sell after a target profit or stop loss
        4. Cancel unfilled orders after a timeout
        
        Args:
            update: Market data update dictionary
        """
        symbol = update['symbol']
        timestamp = update['timestamp']
        bid_price = update['bid_price']
        ask_price = update['ask_price']
        bid_volume = update['bid_volume']
        ask_volume = update['ask_volume']
        
        logger.debug(f"LOB Strategy for {symbol} at {timestamp}: bid={bid_price}@{bid_volume}, ask={ask_price}@{ask_volume}")
        
        # Check if we already have a position in this symbol
        if symbol in self.positions and self.positions[symbol]['quantity'] != 0:
            # We have a position, check if we should exit
            position = self.positions[symbol]
            current_price = update['mid_price']
            entry_price = position['avg_price']
            
            if position['quantity'] > 0:  # Long position
                pnl_pct = (current_price - entry_price) / entry_price * 100
                
                # Check profit target or stop loss
                if pnl_pct >= self.strategy_params['profit_target_pct']:
                    logger.info(f"Profit target reached for {symbol}: {pnl_pct:.2f}%")
                    await self.place_order(symbol, "sell", position['quantity'], current_price)
                elif pnl_pct <= -self.strategy_params['stop_loss_pct']:
                    logger.info(f"Stop loss triggered for {symbol}: {pnl_pct:.2f}%")
                    await self.place_order(symbol, "sell", position['quantity'], current_price)
            
            # Reset imbalance counter
            self.imbalance_counters[symbol] = 0
            return
        
        # Check if we have too many positions
        if len([p for p in self.positions.values() if p['quantity'] != 0]) >= self.strategy_params['max_positions']:
            return
        
        # Check LOB imbalance
        if bid_volume > ask_volume * self.strategy_params['lob_imbalance_threshold']:
            # Increment counter
            self.imbalance_counters[symbol] += 1
            
            # Check if we have enough consecutive ticks with imbalance
            if self.imbalance_counters[symbol] >= self.strategy_params['min_consecutive_ticks']:
                # Calculate imbalance ratio for logging
                imbalance_ratio = bid_volume / ask_volume if ask_volume > 0 else float('inf')
                logger.info(f"LOB imbalance detected for {symbol}: {imbalance_ratio:.2f}, placing buy order")
                
                # Calculate position size
                position_value = self.cash * self.strategy_params['position_size_pct']
                quantity = int(position_value / update['ask_price'])
                
                if quantity > 0:
                    # Place a buy order
                    await self.place_order(symbol, "buy", quantity, update['ask_price'])
                
                # Reset counter
                self.imbalance_counters[symbol] = 0
        else:
            # Reset counter
            self.imbalance_counters[symbol] = 0
    
    async def place_order(self, symbol, side, quantity, price):
        """
        Place an order.
        
        Args:
            symbol: Security symbol
            side: "buy" or "sell"
            quantity: Number of shares
            price: Limit price
        
        Returns:
            Order ID
        """
        # Generate order ID
        order_id = f"order_{self.next_order_id}"
        self.next_order_id += 1
        
        # Create order
        order = {
            'order_id': order_id,
            'symbol': symbol,
            'side': side,
            'quantity': quantity,
            'price': price,
            'timestamp': int(time.time() * 1e9),
            'status': 'new'
        }
        
        # Store order
        self.orders[order_id] = order
        self.order_age[order_id] = 0
        
        logger.info(f"Placed {side} order for {quantity} {symbol} at {price}: {order_id}")
        
        # If we have an order queue, send the order
        if self.order_queue is not None:
            await self.order_queue.put(order)
        
        # Execute order immediately (in a real system, this would be sent to the exchange)
        await self.execute_order(order_id)
        
        return order_id
    
    async def cancel_order(self, order_id):
        """
        Cancel an order.
        
        Args:
            order_id: Order ID to cancel
        
        Returns:
            Success flag
        """
        if order_id not in self.orders:
            logger.warning(f"Order not found: {order_id}")
            return False
        
        order = self.orders[order_id]
        if order['status'] not in ['new', 'partially_filled']:
            logger.warning(f"Cannot cancel order with status {order['status']}: {order_id}")
            return False
        
        # Update order status
        order['status'] = 'canceled'
        
        # Remove from order age tracking
        if order_id in self.order_age:
            del self.order_age[order_id]
        
        # If we have an order queue, send the cancel request
        if self.order_queue is not None:
            cancel_request = {
                'type': 'cancel',
                'order_id': order_id
            }
            await self.order_queue.put(cancel_request)
        
        logger.info(f"Canceled order: {order_id}")
        
        return True
    
    async def execute_order(self, order_id):
        """
        Execute an order (simulate fill).
        
        Args:
            order_id: Order ID to execute
        
        Returns:
            Success flag
        """
        if order_id not in self.orders:
            logger.warning(f"Order not found: {order_id}")
            return False
        
        order = self.orders[order_id]
        if order['status'] != 'new':
            logger.warning(f"Cannot execute order with status {order['status']}: {order_id}")
            return False
        
        symbol = order['symbol']
        side = order['side']
        quantity = order['quantity']
        price = order['price']
        
        # Update order status
        order['status'] = 'filled'
        
        # Remove from order age tracking
        if order_id in self.order_age:
            del self.order_age[order_id]
        
        # Create trade
        trade_id = f"trade_{self.next_trade_id}"
        self.next_trade_id += 1
        
        trade = {
            'trade_id': trade_id,
            'order_id': order_id,
            'symbol': symbol,
            'side': side,
            'quantity': quantity,
            'price': price,
            'timestamp': int(time.time() * 1e9),
            'pnl': 0.0
        }
        
        # Update positions
        if symbol not in self.positions:
            self.positions[symbol] = {
                'quantity': 0,
                'avg_price': 0.0,
                'realized_pnl': 0.0
            }
        
        position = self.positions[symbol]
        
        if side == 'buy':
            # Calculate cost
            cost = quantity * price
            
            # Check if we have enough cash
            if cost > self.cash:
                logger.warning(f"Not enough cash to execute order: {order_id}")
                order['status'] = 'rejected'
                return False
            
            # Update cash
            self.cash -= cost
            
            # Update position
            if position['quantity'] == 0:
                # New position
                position['quantity'] = quantity
                position['avg_price'] = price
            else:
                # Add to existing position
                new_quantity = position['quantity'] + quantity
                new_avg_price = (position['quantity'] * position['avg_price'] + quantity * price) / new_quantity
                position['quantity'] = new_quantity
                position['avg_price'] = new_avg_price
        
        elif side == 'sell':
            # Check if we have enough shares
            if quantity > position['quantity']:
                logger.warning(f"Not enough shares to execute order: {order_id}")
                order['status'] = 'rejected'
                return False
            
            # Calculate proceeds
            proceeds = quantity * price
            
            # Calculate P&L
            pnl = (price - position['avg_price']) * quantity
            
            # Update trade P&L
            trade['pnl'] = pnl
            
            # Update position
            position['quantity'] -= quantity
            position['realized_pnl'] += pnl
            
            # Update cash
            self.cash += proceeds
            
            # Update metrics
            self.metrics['realized_pnl'] += pnl
            self.metrics['num_trades'] += 1
            if pnl > 0:
                self.metrics['winning_trades'] += 1
            elif pnl < 0:
                self.metrics['losing_trades'] += 1
        
        # Add trade to history
        self.trades.append(trade)
        
        # Log trade
        logger.info(f"Executed {side} trade for {quantity} {symbol} at {price}: {trade_id}, P&L: {trade['pnl']:.2f}")
        
        # Save trade to file
        await self.save_trade(trade)
        
        return True
    
    async def save_trade(self, trade):
        """
        Save a trade to file.
        
        Args:
            trade: Trade to save
        """
        # Create trades directory if it doesn't exist
        trades_dir = self.output_dir / "trades"
        trades_dir.mkdir(parents=True, exist_ok=True)
        
        # Convert timestamp to datetime
        dt = datetime.fromtimestamp(trade['timestamp'] / 1e9)
        date_str = dt.strftime("%Y%m%d")
        
        # Create file path
        file_path = trades_dir / f"trades_{date_str}.csv"
        
        # Check if file exists
        file_exists = file_path.exists()
        
        # Write to file asynchronously
        try:
            # Create a task to write to file
            loop = asyncio.get_running_loop()
            await loop.run_in_executor(None, self._write_trade_to_file, file_path, file_exists, trade)
        except Exception as e:
            logger.error(f"Error saving trade to file: {e}")
    
    def _write_trade_to_file(self, file_path, file_exists, trade):
        """
        Write a trade to file (synchronous helper method).
        
        Args:
            file_path: Path to the file
            file_exists: Whether the file exists
            trade: Trade to save
        """
        with open(file_path, 'a') as f:
            # Write header if file doesn't exist
            if not file_exists:
                f.write("trade_id,order_id,symbol,side,quantity,price,timestamp,pnl\n")
            
            # Write trade
            f.write(f"{trade['trade_id']},{trade['order_id']},{trade['symbol']},{trade['side']},"
                   f"{trade['quantity']},{trade['price']},{trade['timestamp']},{trade['pnl']}\n")
    
    def calculate_performance_metrics(self):
        """
        Calculate performance metrics.
        
        Returns:
            Dictionary of performance metrics
        """
        # Calculate unrealized P&L
        unrealized_pnl = 0.0
        for symbol, position in self.positions.items():
            if position['quantity'] > 0 and symbol in self.market_data:
                current_price = self.market_data[symbol].mid_price
                unrealized_pnl += (current_price - position['avg_price']) * position['quantity']
        
        # Update metrics
        self.metrics['unrealized_pnl'] = unrealized_pnl
        self.metrics['total_pnl'] = self.metrics['realized_pnl'] + unrealized_pnl
        
        # Calculate win rate
        if self.metrics['num_trades'] > 0:
            self.metrics['win_rate'] = self.metrics['winning_trades'] / self.metrics['num_trades'] * 100
        else:
            self.metrics['win_rate'] = 0.0
        
        # Calculate average profit and loss
        winning_trades = [t for t in self.trades if t['pnl'] > 0]
        losing_trades = [t for t in self.trades if t['pnl'] < 0]
        
        if winning_trades:
            self.metrics['avg_profit'] = sum(t['pnl'] for t in winning_trades) / len(winning_trades)
        else:
            self.metrics['avg_profit'] = 0.0
        
        if losing_trades:
            self.metrics['avg_loss'] = sum(t['pnl'] for t in losing_trades) / len(losing_trades)
        else:
            self.metrics['avg_loss'] = 0.0
        
        # Calculate profit factor
        gross_profit = sum(t['pnl'] for t in winning_trades)
        gross_loss = abs(sum(t['pnl'] for t in losing_trades))
        
        if gross_loss > 0:
            self.metrics['profit_factor'] = gross_profit / gross_loss
        else:
            self.metrics['profit_factor'] = float('inf') if gross_profit > 0 else 0.0
        
        # Calculate duration
        self.metrics['duration_seconds'] = time.time() - self.metrics['start_time']
        
        return self.metrics
    
    async def save_performance_summary(self):
        """
        Save performance summary to file.
        
        Returns:
            Path to the summary file
        """
        # Calculate metrics
        metrics = self.calculate_performance_metrics()
        
        # Create summary
        summary = {
            'timestamp': datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            'duration_seconds': metrics['duration_seconds'],
            'initial_capital': self.initial_capital,
            'final_capital': self.cash + sum(
                position['quantity'] * self.market_data[symbol]['mid_price']
                for symbol, position in self.positions.items()
                if position['quantity'] > 0 and symbol in self.market_data
            ),
            'total_pnl': metrics['total_pnl'],
            'realized_pnl': metrics['realized_pnl'],
            'unrealized_pnl': metrics['unrealized_pnl'],
            'num_trades': metrics['num_trades'],
            'winning_trades': metrics['winning_trades'],
            'losing_trades': metrics['losing_trades'],
            'win_rate': metrics['win_rate'],
            'avg_profit': metrics['avg_profit'],
            'avg_loss': metrics['avg_loss'],
            'profit_factor': metrics['profit_factor'],
            'positions': [
                {
                    'symbol': symbol,
                    'quantity': position['quantity'],
                    'avg_price': position['avg_price'],
                    'current_price': self.market_data[symbol]['mid_price'] if symbol in self.market_data else None,
                    'unrealized_pnl': (self.market_data[symbol]['mid_price'] - position['avg_price']) * position['quantity']
                    if symbol in self.market_data and position['quantity'] > 0 else 0.0,
                    'realized_pnl': position['realized_pnl']
                }
                for symbol, position in self.positions.items()
                if position['quantity'] > 0 or position['realized_pnl'] != 0.0
            ]
        }
        
        # Create summary file
        summary_path = self.output_dir / "performance_summary.json"
        
        # Write to file asynchronously
        try:
            loop = asyncio.get_running_loop()
            await loop.run_in_executor(None, self._write_summary_to_file, summary_path, summary)
        except Exception as e:
            logger.error(f"Error saving performance summary to file: {e}")
        
        # Also print to console
        logger.info("Performance Summary:")
        logger.info(f"Duration: {summary['duration_seconds']:.2f} seconds")
        logger.info(f"Initial Capital: ${self.initial_capital:.2f}")
        logger.info(f"Final Capital: ${summary['final_capital']:.2f}")
        logger.info(f"Total P&L: ${summary['total_pnl']:.2f}")
        logger.info(f"Realized P&L: ${summary['realized_pnl']:.2f}")
        logger.info(f"Unrealized P&L: ${summary['unrealized_pnl']:.2f}")
        logger.info(f"Number of Trades: {summary['num_trades']}")
        logger.info(f"Winning Trades: {summary['winning_trades']} ({summary['win_rate']:.2f}%)")
        logger.info(f"Losing Trades: {summary['losing_trades']}")
        logger.info(f"Average Profit: ${summary['avg_profit']:.2f}")
        logger.info(f"Average Loss: ${summary['avg_loss']:.2f}")
        logger.info(f"Profit Factor: {summary['profit_factor']:.2f}")
        
        return summary_path
        
    def _write_summary_to_file(self, summary_path, summary):
        """
        Write performance summary to file (synchronous helper method).
        
        Args:
            summary_path: Path to the summary file
            summary: Performance summary dictionary
        """
        with open(summary_path, 'w') as f:
            json.dump(summary, f, indent=2)
    
    async def run(self):
        """
        Run the trading engine.
        
        This method processes market data from the queue and executes
        trading strategies until the end of data is signaled.
        """
        try:
            logger.info("Trading engine run() method started")
            
            # Set up signal handlers for graceful shutdown
            loop = asyncio.get_running_loop()
            logger.debug("Setting up signal handlers for graceful shutdown")
            for sig in (signal.SIGINT, signal.SIGTERM):
                loop.add_signal_handler(sig, lambda: asyncio.create_task(self.shutdown()))
            
            # Start market data processing task
            logger.info("Creating market data processing task")
            market_data_task = asyncio.create_task(self.process_market_data())
            
            # Start periodic performance reporting task
            logger.info("Creating periodic performance reporting task")
            reporting_task = asyncio.create_task(self.periodic_reporting())
            
            # Wait for tasks to complete
            logger.info("Trading engine running, press Ctrl+C to stop")
            logger.debug("Awaiting asyncio.gather() for market data and reporting tasks")
            await asyncio.gather(market_data_task, reporting_task)
        
        except asyncio.CancelledError:
            logger.info("Trading engine tasks cancelled")
        except Exception as e:
            logger.error(f"Error running trading engine: {e}")
        finally:
            # Ensure we're stopped
            self.running = False
            
            logger.info("Trading engine stopped")
        
        return True
    
    async def periodic_reporting(self):
        """
        Periodically report performance metrics.
        """
        try:
            while self.running:
                # Calculate and log performance metrics
                metrics = self.calculate_performance_metrics()
                logger.info(f"Current P&L: ${metrics['total_pnl']:.2f}, Trades: {metrics['num_trades']}")
                
                # Sleep to avoid busy waiting, but use a shorter interval for faster simulation
                await asyncio.sleep(1)
        except asyncio.CancelledError:
            logger.info("Periodic reporting task cancelled")
    
    async def shutdown(self):
        """
        Gracefully shut down the trading engine.
        """
        logger.info("Shutting down trading engine...")
        self.running = False
        
        # Calculate final performance metrics
        self.calculate_performance_metrics()
        
        # Save performance summary
        await self.save_performance_summary()
        
        # Cancel all running tasks
        for task in asyncio.all_tasks():
            if task is not asyncio.current_task():
                task.cancel()
        
        logger.info("Trading engine shutdown complete")


async def run_trading_engine(market_data_queue, order_queue=None, initial_capital=1000000.0, output_dir="trading_output"):
    """Run the trading engine asynchronously."""
    # Create the trading engine
    engine = TradingEngine(
        market_data_queue=market_data_queue,
        order_queue=order_queue,
        initial_capital=initial_capital,
        output_dir=output_dir
    )
    
    # Run the engine
    await engine.run()

def main():
    """Main function to run the trading engine."""
    parser = argparse.ArgumentParser(description="Trading Engine")
    parser.add_argument(
        "--capital",
        type=float,
        default=1000000.0,
        help="Initial capital"
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default="trading_output",
        help="Output directory for trading results"
    )
    
    args = parser.parse_args()
    
    # Create event loop
    loop = asyncio.get_event_loop()
    
    # Create market data queue
    market_data_queue = asyncio.Queue()
    
    # Create order queue
    order_queue = asyncio.Queue()
    
    # Run the trading engine
    try:
        loop.run_until_complete(run_trading_engine(
            market_data_queue=market_data_queue,
            order_queue=order_queue,
            initial_capital=args.capital,
            output_dir=args.output_dir
        ))
    except KeyboardInterrupt:
        logger.info("Trading engine stopped by user")
    finally:
        loop.close()


if __name__ == "__main__":
    main()
