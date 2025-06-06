import pandas as pd
import numpy as np
import os
from typing import Dict, List, Optional, Tuple, Any
from dataclasses import dataclass
from collections import defaultdict, deque
import logging

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger("order_book")


@dataclass
class Order:
    """Represents an order in the order book."""
    reference: int
    price: float
    shares: int
    side: str  # "Buy" or "Sell"
    stock: str
    timestamp: int


class OrderBook:
    """
    Implements an order book that tracks bids and asks for securities.
    
    This class processes NASDAQ ITCH 5.0 messages and maintains the state
    of the order book for each security.
    """
    
    def __init__(self):
        # Dictionary to store all active orders by reference ID
        self.orders: Dict[int, Order] = {}
        
        # Dictionary to store order books for each stock
        # Each order book has two sides: bids and asks
        # Each side is a dictionary with price levels as keys and total volume at that level as values
        self.books: Dict[str, Dict[str, Dict[float, int]]] = defaultdict(
            lambda: {"bids": {}, "asks": {}}
        )
        
        # Dictionary to store the current best bid and ask for each stock
        self.best_prices: Dict[str, Dict[str, Optional[float]]] = defaultdict(
            lambda: {"bid": None, "ask": None}
        )
        
        # Use deque for price history instead of list (faster appends, fixed size)
        self.price_history_max_size = 1000  # Limit history to 1000 entries per stock
        self.price_history: Dict[str, deque] = defaultdict(lambda: deque(maxlen=self.price_history_max_size))
        
        # Set of all stocks seen
        self.stocks = set()
        
        # Cache for order book snapshots
        self.snapshot_cache: Dict[str, Tuple[int, pd.DataFrame]] = {}
        
        # Message counters for statistics
        self.message_counts = {
            "add_order": 0,
            "delete_order": 0,
            "order_executed": 0,
            "order_cancelled": 0,
            "replace_order": 0,
            "other": 0,
            "total": 0
        }

    def process_message(self, message: dict) -> None:
        """
        Process a single NASDAQ ITCH 5.0 message and update the order book.
        
        Args:
            message: A dictionary containing the ITCH message data.
        """
        self.message_counts["total"] += 1
        
        # Extract common fields
        tag = message.get("tag")
        timestamp = message.get("timestamp")
        body = message.get("body", {})
        
        # Debug: Log message details
        if self.message_counts["total"] % 1000 == 0:
            logger.info(f"Processing message #{self.message_counts['total']}, tag: {tag}, timestamp: {timestamp}")
        
        # Process message based on its type
        if "AddOrder" in body:
            logger.debug(f"Processing AddOrder: {body['AddOrder']}")
            self._process_add_order(body["AddOrder"], timestamp)
            self.message_counts["add_order"] += 1
        
        elif "DeleteOrder" in body:
            logger.debug(f"Processing DeleteOrder: {body['DeleteOrder']}")
            self._process_delete_order(body["DeleteOrder"])
            self.message_counts["delete_order"] += 1
        
        elif "OrderExecuted" in body:
            logger.debug(f"Processing OrderExecuted: {body['OrderExecuted']}")
            self._process_order_executed(body["OrderExecuted"])
            self.message_counts["order_executed"] += 1
        
        elif "OrderCancelled" in body:
            logger.debug(f"Processing OrderCancelled: {body['OrderCancelled']}")
            self._process_order_cancelled(body["OrderCancelled"])
            self.message_counts["order_cancelled"] += 1
        
        elif "ReplaceOrder" in body:
            logger.debug(f"Processing ReplaceOrder: {body['ReplaceOrder']}")
            self._process_replace_order(body["ReplaceOrder"], timestamp)
            self.message_counts["replace_order"] += 1
        
        else:
            logger.debug(f"Unknown message type: {list(body.keys())[0] if body else 'No body'}")
            self.message_counts["other"] += 1

    def _process_add_order(self, data: dict, timestamp: int) -> None:
        """Process an Add Order message."""
        reference = data.get("reference")
        side = data.get("side")
        shares = data.get("shares")
        stock = data.get("stock", "").strip()
        price = float(data.get("price", "0.0"))
        
        logger.debug(f"AddOrder details - reference: {reference}, side: {side}, shares: {shares}, stock: {stock}, price: {price}")
        
        if not all([reference, side, shares, stock, price]):
            logger.warning(f"Incomplete AddOrder data: {data}")
            return
        
        # Add stock to the set of stocks
        self.stocks.add(stock)
        logger.debug(f"Added stock {stock} to tracked stocks. Total stocks: {len(self.stocks)}")
        
        # Create a new order
        order = Order(
            reference=reference,
            price=price,
            shares=shares,
            side=side,
            stock=stock,
            timestamp=timestamp
        )
        
        # Store the order
        self.orders[reference] = order
        logger.debug(f"Stored order {reference}. Total orders: {len(self.orders)}")
        
        # Update the order book
        book_side = "bids" if side == "Buy" else "asks"
        
        if price not in self.books[stock][book_side]:
            self.books[stock][book_side][price] = 0
            logger.debug(f"Created new price level {price} for {stock} {book_side}")
        
        self.books[stock][book_side][price] += shares
        logger.debug(f"Updated {stock} {book_side} at price {price}. New volume: {self.books[stock][book_side][price]}")
        
        # Update best prices
        old_best_bid = self.best_prices[stock]["bid"]
        old_best_ask = self.best_prices[stock]["ask"]
        
        self._update_best_prices(stock)
        
        # Log if best prices changed
        if old_best_bid != self.best_prices[stock]["bid"] or old_best_ask != self.best_prices[stock]["ask"]:
            logger.debug(f"Best prices for {stock} changed. Bid: {old_best_bid} -> {self.best_prices[stock]['bid']}, Ask: {old_best_ask} -> {self.best_prices[stock]['ask']}")
        
        # Update price history if best prices changed
        self._update_price_history(stock, timestamp)

    def _process_delete_order(self, data: dict) -> None:
        """Process a Delete Order message."""
        reference = data.get("reference")
        
        if reference in self.orders:
            order = self.orders[reference]
            
            # Update the order book
            book_side = "bids" if order.side == "Buy" else "asks"
            
            if order.price in self.books[order.stock][book_side]:
                self.books[order.stock][book_side][order.price] -= order.shares
                
                # Remove the price level if no more orders at this price
                if self.books[order.stock][book_side][order.price] <= 0:
                    del self.books[order.stock][book_side][order.price]
            
            # Remove the order
            del self.orders[reference]
            
            # Update best prices
            self._update_best_prices(order.stock)
            
            # Update price history if best prices changed
            self._update_price_history(order.stock, order.timestamp)

    def _process_order_executed(self, data: dict) -> None:
        """Process an Order Executed message."""
        reference = data.get("reference")
        shares_executed = data.get("shares", 0)
        
        if reference in self.orders and shares_executed > 0:
            order = self.orders[reference]
            
            # Update the order book
            book_side = "bids" if order.side == "Buy" else "asks"
            
            if order.price in self.books[order.stock][book_side]:
                self.books[order.stock][book_side][order.price] -= shares_executed
                
                # Remove the price level if no more orders at this price
                if self.books[order.stock][book_side][order.price] <= 0:
                    del self.books[order.stock][book_side][order.price]
            
            # Update the order
            order.shares -= shares_executed
            
            # Remove the order if fully executed
            if order.shares <= 0:
                del self.orders[reference]
            
            # Update best prices
            self._update_best_prices(order.stock)
            
            # Update price history if best prices changed
            self._update_price_history(order.stock, order.timestamp)

    def _process_order_cancelled(self, data: dict) -> None:
        """Process an Order Cancelled message."""
        reference = data.get("reference")
        shares_cancelled = data.get("shares", 0)
        
        if reference in self.orders and shares_cancelled > 0:
            order = self.orders[reference]
            
            # Update the order book
            book_side = "bids" if order.side == "Buy" else "asks"
            
            if order.price in self.books[order.stock][book_side]:
                self.books[order.stock][book_side][order.price] -= shares_cancelled
                
                # Remove the price level if no more orders at this price
                if self.books[order.stock][book_side][order.price] <= 0:
                    del self.books[order.stock][book_side][order.price]
            
            # Update the order
            order.shares -= shares_cancelled
            
            # Remove the order if fully cancelled
            if order.shares <= 0:
                del self.orders[reference]
            
            # Update best prices
            self._update_best_prices(order.stock)
            
            # Update price history if best prices changed
            self._update_price_history(order.stock, order.timestamp)

    def _process_replace_order(self, data: dict, timestamp: int) -> None:
        """Process a Replace Order message."""
        old_reference = data.get("reference")
        new_reference = data.get("new_reference")
        new_shares = data.get("shares")
        new_price = float(data.get("price", "0.0"))
        
        if old_reference in self.orders:
            old_order = self.orders[old_reference]
            
            # First, remove the old order from the book
            book_side = "bids" if old_order.side == "Buy" else "asks"
            
            if old_order.price in self.books[old_order.stock][book_side]:
                self.books[old_order.stock][book_side][old_order.price] -= old_order.shares
                
                # Remove the price level if no more orders at this price
                if self.books[old_order.stock][book_side][old_order.price] <= 0:
                    del self.books[old_order.stock][book_side][old_order.price]
            
            # Create a new order with the updated information
            new_order = Order(
                reference=new_reference,
                price=new_price,
                shares=new_shares,
                side=old_order.side,
                stock=old_order.stock,
                timestamp=timestamp
            )
            
            # Remove the old order and add the new one
            del self.orders[old_reference]
            self.orders[new_reference] = new_order
            
            # Update the order book with the new order
            if new_price not in self.books[new_order.stock][book_side]:
                self.books[new_order.stock][book_side][new_price] = 0
            
            self.books[new_order.stock][book_side][new_price] += new_shares
            
            # Update best prices
            self._update_best_prices(new_order.stock)
            
            # Update price history if best prices changed
            self._update_price_history(new_order.stock, timestamp)

    def _update_best_prices(self, stock: str) -> None:
        """Update the best bid and ask prices for a stock."""
        # Update best bid
        if self.books[stock]["bids"]:
            self.best_prices[stock]["bid"] = max(self.books[stock]["bids"].keys())
        else:
            self.best_prices[stock]["bid"] = None
        
        # Update best ask
        if self.books[stock]["asks"]:
            self.best_prices[stock]["ask"] = min(self.books[stock]["asks"].keys())
        else:
            self.best_prices[stock]["ask"] = None

    def _update_price_history(self, stock: str, timestamp: int) -> None:
        """Update the price history for a stock."""
        bid = self.best_prices[stock]["bid"]
        ask = self.best_prices[stock]["ask"]
        
        # Only add to history if both bid and ask are available
        if bid is not None and ask is not None:
            # Ensure stock is in tracked stocks list
            if stock not in self.stocks:
                self.stocks.append(stock)
                
            # Add to deque (automatically manages fixed size)
            self.price_history[stock].append((timestamp, bid, ask))

    def get_order_book_snapshot(self, stock: str) -> pd.DataFrame:
        """
        Get a snapshot of the current order book for a stock.
        
        Returns a DataFrame with price levels and volumes for both sides of the book.
        Uses caching to avoid redundant calculations.
        """
        if stock not in self.books:
            return pd.DataFrame()
            
        # Check cache first - use message count as version to determine if cache is fresh
        # Only use cache if less than 5 messages have been processed since the cache was created
        current_msg_count = self.message_counts["total"]
        if stock in self.snapshot_cache:
            cache_msg_count, snapshot = self.snapshot_cache[stock]
            if current_msg_count - cache_msg_count < 5:  # Cache is recent enough
                return snapshot
        
        # Use list comprehensions for better performance
        bid_data = [{"price": price, "volume": volume, "side": "bid"} 
                   for price, volume in sorted(self.books[stock]["bids"].items(), reverse=True)]
        
        ask_data = [{"price": price, "volume": volume, "side": "ask"} 
                   for price, volume in sorted(self.books[stock]["asks"].items())]
        
        # Combine and create DataFrame
        all_data = bid_data + ask_data
        if not all_data:
            return pd.DataFrame()
        
        snapshot = pd.DataFrame(all_data)
        
        # Update cache
        self.snapshot_cache[stock] = (current_msg_count, snapshot)
        
        return snapshot

    def get_price_history(self, stock: str) -> pd.DataFrame:
        """
        Get the price history for a stock.
        
        Returns a DataFrame with timestamps, bid prices, and ask prices.
        Optimized for performance with list conversion.
        """
        if stock not in self.price_history or not self.price_history[stock]:
            return pd.DataFrame()
        
        # Convert deque to list once for better performance
        price_data = list(self.price_history[stock])
        if not price_data:
            return pd.DataFrame()
            
        # Create DataFrame - use direct constructor for better performance
        df = pd.DataFrame(price_data, columns=["timestamp", "bid", "ask"])
        
        # Calculate mid price - use vectorized operations
        df["mid"] = (df["bid"] + df["ask"]) / 2
        
        return df

    def get_summary_statistics(self) -> pd.DataFrame:
        """
        Get summary statistics for all stocks in the order book.
        
        Returns a DataFrame with statistics for each stock.
        """
        stats = []
        
        for stock in self.stocks:
            # Get current best prices
            best_bid = self.best_prices[stock]["bid"]
            best_ask = self.best_prices[stock]["ask"]
            
            # Calculate spread
            spread = None
            spread_pct = None
            mid_price = None
            
            if best_bid is not None and best_ask is not None:
                spread = best_ask - best_bid
                mid_price = (best_bid + best_ask) / 2
                spread_pct = (spread / mid_price) * 100 if mid_price > 0 else None
            
            # Calculate total volume on each side
            bid_volume = sum(self.books[stock]["bids"].values())
            ask_volume = sum(self.books[stock]["asks"].values())
            
            # Calculate order book imbalance
            imbalance = None
            if bid_volume + ask_volume > 0:
                imbalance = (bid_volume - ask_volume) / (bid_volume + ask_volume)
            
            # Get price history stats if available
            price_volatility = None
            price_history = self.get_price_history(stock)
            
            if not price_history.empty and len(price_history) > 1:
                price_volatility = price_history["mid"].std()
            
            # Add to stats
            stats.append({
                "stock": stock,
                "best_bid": best_bid,
                "best_ask": best_ask,
                "spread": spread,
                "spread_pct": spread_pct,
                "mid_price": mid_price,
                "bid_volume": bid_volume,
                "ask_volume": ask_volume,
                "imbalance": imbalance,
                "price_volatility": price_volatility,
                "num_price_levels_bid": len(self.books[stock]["bids"]),
                "num_price_levels_ask": len(self.books[stock]["asks"])
            })
        
        return pd.DataFrame(stats)

    def get_message_statistics(self) -> pd.DataFrame:
        """Get statistics about processed messages."""
        data = [{
            "message_type": msg_type,
            "count": count,
            "percentage": (count / self.message_counts["total"]) * 100 if self.message_counts["total"] > 0 else 0
        } for msg_type, count in self.message_counts.items() if msg_type != "total"]
        
        return pd.DataFrame(data)
        
    def export_mega_csv(self, output_path: str) -> str:
        """
        Export a timestamp-sorted mega CSV with all price updates for all stocks.
        
        Args:
            output_path: Directory to save the mega CSV file
            
        Returns:
            Path to the created CSV file
        """
        logger.info(f"Exporting mega CSV with all price updates to {output_path}")
        
        # Create a list to store all price updates
        all_updates = []
        
        # Process each stock's price history
        for stock in self.stocks:
            price_history = self.get_price_history(stock)
            
            if not price_history.empty:
                # Add stock symbol to each row
                price_history['stock'] = stock
                
                # Calculate additional metrics
                price_history['spread'] = price_history['ask'] - price_history['bid']
                price_history['spread_pct'] = (price_history['spread'] / price_history['mid']) * 100
                
                # Get volume information from the current order book
                current_ob = self.get_order_book_snapshot(stock)
                bid_volume = 0
                ask_volume = 0
                
                if not current_ob.empty:
                    bid_volume = current_ob[current_ob['side'] == 'bid']['volume'].sum()
                    ask_volume = current_ob[current_ob['side'] == 'ask']['volume'].sum()
                
                # Add volume info to each row
                price_history['bid_volume'] = bid_volume
                price_history['ask_volume'] = ask_volume
                
                # Calculate imbalance
                total_volume = bid_volume + ask_volume
                imbalance = 0
                if total_volume > 0:
                    imbalance = (bid_volume - ask_volume) / total_volume
                price_history['imbalance'] = imbalance
                
                # Add to the list of all updates
                all_updates.append(price_history)
        
        # Combine all updates into a single DataFrame
        if not all_updates:
            logger.warning("No price updates found for any stock")
            return ""
        
        mega_df = pd.concat(all_updates, ignore_index=True)
        
        # Sort by timestamp
        mega_df = mega_df.sort_values('timestamp')
        
        # Convert timestamp to datetime for better readability
        mega_df['datetime'] = pd.to_datetime(mega_df['timestamp'] / 1e9, unit='s')
        
        # Save to CSV
        output_file = os.path.join(output_path, "all_price_updates.csv")
        mega_df.to_csv(output_file, index=False)
        
        logger.info(f"Exported {len(mega_df)} price updates for {len(self.stocks)} stocks to {output_file}")
        
        return output_file
