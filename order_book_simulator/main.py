#!/usr/bin/env python3
import argparse
import json
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from typing import List, Dict, Any, Optional, Callable, Awaitable
from tqdm import tqdm
import sys
import os
import logging
import time
import asyncio
from queue import Queue
import signal

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler("order_book_simulator.log"),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger("main")

# Import the OrderBook class
from order_book import OrderBook


def parse_arguments():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="Process NASDAQ ITCH 5.0 JSON data and simulate an order book."
    )
    parser.add_argument(
        "input_file", 
        type=str, 
        help="Path to the JSON file containing ITCH 5.0 data"
    )
    parser.add_argument(
        "-n", "--num-messages", 
        type=int, 
        default=0, 
        help="Number of messages to process (0 for all messages)"
    )
    parser.add_argument(
        "-s", "--stocks", 
        type=str, 
        nargs="+", 
        help="Filter for specific stock symbols"
    )
    parser.add_argument(
        "-o", "--output-dir", 
        type=str, 
        default="output", 
        help="Directory to save output files"
    )
    parser.add_argument(
        "--no-plots", 
        action="store_true", 
        help="Disable generation of plots"
    )
    parser.add_argument(
        "--verbose", 
        action="store_true", 
        help="Enable verbose output"
    )
    parser.add_argument(
        "--trading-engine",
        action="store_true",
        help="Enable trading engine integration"
    )
    parser.add_argument(
        "--update-interval",
        type=float,
        default=0.01,
        help="Interval between market data updates in seconds (default: 0.01)"
    )
    parser.add_argument(
        "--tick-by-tick",
        action="store_true",
        help="Send market data updates for every tick"
    )
    
    return parser.parse_args()


def load_json_data(file_path: str, num_messages: int = 0) -> List[Dict[str, Any]]:
    """
    Load JSON data from file.
    
    Args:
        file_path: Path to the JSON file
        num_messages: Number of messages to load (0 for all)
        
    Returns:
        List of message dictionaries
    """
    logger.info(f"Loading data from {file_path}...")
    start_time = time.time()
    
    try:
        with open(file_path, 'r') as f:
            # Check first character to see if it's a JSON array
            first_char = f.read(1)
            f.seek(0)
            
            logger.debug(f"File format detection: first character is '{first_char}'")
            
            if first_char == '[':
                # Load as JSON array
                logger.info("Detected JSON array format")
                data = json.load(f)
                if num_messages > 0:
                    logger.info(f"Limiting to {num_messages} messages")
                    data = data[:num_messages]
            else:
                # Load line by line (JSON Lines format)
                logger.info("Detected JSON Lines format")
                data = []
                for i, line in enumerate(f):
                    if i % 1000 == 0:
                        logger.debug(f"Processed {i} lines...")
                    if num_messages > 0 and i >= num_messages:
                        logger.info(f"Reached limit of {num_messages} messages")
                        break
                    try:
                        data.append(json.loads(line))
                    except json.JSONDecodeError:
                        logger.warning(f"Could not parse line {i+1}")
        
        elapsed_time = time.time() - start_time
        logger.info(f"Loaded {len(data)} messages in {elapsed_time:.2f} seconds")
        
        # Log a sample of the data
        if data:
            logger.debug(f"Sample message: {data[0]}")
        
        return data
    
    except (json.JSONDecodeError, FileNotFoundError) as e:
        logger.error(f"Error loading JSON data: {e}")
        sys.exit(1)


def filter_messages_by_stocks(messages: List[Dict[str, Any]], stocks: List[str]) -> List[Dict[str, Any]]:
    """
    Filter messages to only include those for specific stocks.
    
    Args:
        messages: List of message dictionaries
        stocks: List of stock symbols to include
        
    Returns:
        Filtered list of message dictionaries
    """
    if not stocks:
        return messages
    
    # Normalize stock symbols (strip whitespace)
    stocks = [s.strip() for s in stocks]
    
    filtered_messages = []
    for msg in messages:
        body = msg.get("body", {})
        
        # Check various message types that might contain stock information
        stock = None
        
        for key in body:
            if isinstance(body[key], dict) and "stock" in body[key]:
                stock = body[key]["stock"].strip()
                break
        
        if stock in stocks:
            filtered_messages.append(msg)
    
    print(f"Filtered to {len(filtered_messages)} messages for stocks: {', '.join(stocks)}")
    return filtered_messages


def process_messages(messages: List[Dict[str, Any]], verbose: bool = False) -> OrderBook:
    """
    Process messages and build the order book.
    
    Args:
        messages: List of message dictionaries
        verbose: Whether to print verbose output
        
    Returns:
        Populated OrderBook instance
    """
    logger.info(f"Processing {len(messages)} messages...")
    
    # Create order book
    order_book = OrderBook()
    
    # Process messages with progress bar
    for i, message in enumerate(tqdm(messages, desc="Processing messages")):
        order_book.process_message(message)
        
        if verbose and i % 1000 == 0:
            logger.info(f"Processed {i+1}/{len(messages)} messages")
    
    logger.info(f"Processed {len(messages)} messages")
    logger.info(f"Found {len(order_book.stocks)} unique stocks: {', '.join(sorted(order_book.stocks))}")
    
    return order_book


async def process_messages_async(messages: List[Dict[str, Any]], 
                               market_data_queue: asyncio.Queue,
                               update_interval: float = 0.01,
                               tick_by_tick: bool = False,
                               verbose: bool = False) -> OrderBook:
    """
    Process messages asynchronously and build the order book, publishing updates to a queue.
    
    Args:
        messages: List of message dictionaries
        market_data_queue: Queue to publish market data updates
        update_interval: Interval between market data updates in seconds
        tick_by_tick: Whether to send updates for every tick
        verbose: Whether to print verbose output
        
    Returns:
        Populated OrderBook instance
    """
    logger.info(f"Processing {len(messages)} messages asynchronously...")
    logger.info(f"Configuration: update_interval={update_interval}s, tick_by_tick={tick_by_tick}, verbose={verbose}")
    logger.debug(f"Market data queue: {market_data_queue}, max size: {market_data_queue.maxsize}")
    
    # Track statistics for debugging
    updates_sent = 0
    last_stats_time = time.time()
    messages_processed = 0
    
    # Create order book
    order_book = OrderBook()
    
    # Track last update time
    last_update_time = time.time()
    
    # Process messages with progress bar
    for i, message in enumerate(tqdm(messages, desc="Processing messages")):
        # Process the message
        order_book.process_message(message)
        
        # Check if we should send an update - optimize by checking less frequently for better performance
        if i % 10 == 0:  # Only check every 10 messages
            current_time = time.time()
            should_update = tick_by_tick or (current_time - last_update_time >= update_interval)
            
            if should_update:
                # Create market data update for all stocks
                for stock in order_book.stocks:
                    # Get the latest data for this stock
                    price_history = order_book.get_price_history(stock)
                    ob_snapshot = order_book.get_order_book_snapshot(stock)
                    
                    if not price_history.empty and not ob_snapshot.empty:
                        # Get the latest price data
                        latest_price = price_history.iloc[-1]
                        
                        # Calculate volumes - use numpy for better performance
                        bid_mask = ob_snapshot['side'] == 'bid'
                        ask_mask = ob_snapshot['side'] == 'ask'
                        bid_volume = ob_snapshot.loc[bid_mask, 'volume'].sum()
                        ask_volume = ob_snapshot.loc[ask_mask, 'volume'].sum()
                        
                        # Calculate imbalance
                        total_volume = bid_volume + ask_volume
                        imbalance = 0.0
                        if total_volume > 0:
                            imbalance = (bid_volume - ask_volume) / total_volume
                        
                        # Create market data update - use direct values to avoid type conversions
                        market_data = {
                            'timestamp': int(latest_price['timestamp']),
                            'symbol': stock.strip(),
                            'bid_price': float(latest_price['bid']),
                            'ask_price': float(latest_price['ask']),
                            'mid_price': float(latest_price['mid']),
                            'bid_volume': int(bid_volume),
                            'ask_volume': int(ask_volume),
                            'imbalance': float(imbalance),
                            'spread': float(latest_price['ask'] - latest_price['bid']),
                            'spread_pct': float((latest_price['ask'] - latest_price['bid']) / latest_price['mid'] * 100)
                        }
                        
                        # Put the update in the queue - skip debug logging for better performance
                        await market_data_queue.put(market_data)
                        updates_sent += 1
                
                # Update last update time
                last_update_time = current_time
        
        # Yield control to allow other tasks to run, but less frequently to improve performance
        if i % 1000 == 0:
            await asyncio.sleep(0)
        
        # Update message processing count
        messages_processed += 1
        
        # Log statistics periodically but less frequently to reduce overhead
        if time.time() - last_stats_time >= 10.0:  # Log every 10 seconds instead of 5
            elapsed = time.time() - last_stats_time
            msg_rate = messages_processed / elapsed if elapsed > 0 else 0
            update_rate = updates_sent / elapsed if elapsed > 0 else 0
            logger.info(f"Progress: {i+1}/{len(messages)} messages ({msg_rate:.1f} msgs/sec), {updates_sent} updates sent ({update_rate:.1f} updates/sec)")
            messages_processed = 0
            updates_sent = 0
            last_stats_time = time.time()
        
        if verbose and i % 1000 == 0:
            logger.info(f"Processed {i+1}/{len(messages)} messages")
    
    # Send final update
    await market_data_queue.put(None)  # Signal end of data
    
    logger.info(f"Processed {len(messages)} messages asynchronously")
    logger.info(f"Found {len(order_book.stocks)} unique stocks: {', '.join(sorted(order_book.stocks))}")
    
    return order_book


def generate_reports(order_book: OrderBook, output_dir: str, generate_plots: bool = True):
    """
    Generate reports and plots from the order book data.
    
    Args:
        order_book: Populated OrderBook instance
        output_dir: Directory to save output files
        generate_plots: Whether to generate plots
    """
    # Create output directory if it doesn't exist
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    # Get summary statistics
    summary_stats = order_book.get_summary_statistics()
    if not summary_stats.empty:
        summary_stats.to_csv(output_path / "summary_statistics.csv", index=False)
        print(f"Saved summary statistics to {output_path / 'summary_statistics.csv'}")
        
        # Print summary to console
        pd.set_option('display.max_rows', None)
        pd.set_option('display.width', None)
        print("\nSummary Statistics:")
        print(summary_stats)
    
    # Get message statistics
    message_stats = order_book.get_message_statistics()
    if not message_stats.empty:
        message_stats.to_csv(output_path / "message_statistics.csv", index=False)
        print(f"Saved message statistics to {output_path / 'message_statistics.csv'}")
        
        # Print message stats to console
        print("\nMessage Statistics:")
        print(message_stats)
    
    # Generate reports for each stock
    for stock in order_book.stocks:
        # Get order book snapshot
        ob_snapshot = order_book.get_order_book_snapshot(stock)
        if not ob_snapshot.empty:
            ob_snapshot.to_csv(output_path / f"{stock}_order_book.csv", index=False)
            print(f"Saved order book snapshot for {stock} to {output_path / f'{stock}_order_book.csv'}")
        
        # Get price history
        price_history = order_book.get_price_history(stock)
        if not price_history.empty:
            price_history.to_csv(output_path / f"{stock}_price_history.csv", index=False)
            print(f"Saved price history for {stock} to {output_path / f'{stock}_price_history.csv'}")
        
        # Generate plots if enabled
        if generate_plots and not price_history.empty:
            # Set plot style
            sns.set(style="darkgrid")
            
            # Price history plot
            plt.figure(figsize=(12, 6))
            plt.plot(price_history['mid'], label='Mid Price')
            plt.plot(price_history['bid'], label='Bid Price')
            plt.plot(price_history['ask'], label='Ask Price')
            plt.title(f'Price History for {stock}')
            plt.xlabel('Message Index')
            plt.ylabel('Price')
            plt.legend()
            plt.tight_layout()
            plt.savefig(output_path / f"{stock}_price_history.png")
            plt.close()
            
            # Order book imbalance plot if we have enough data points
            if len(price_history) > 10:
                # Calculate rolling imbalance
                if not ob_snapshot.empty and 'side' in ob_snapshot.columns:
                    bid_volume = ob_snapshot[ob_snapshot['side'] == 'bid']['volume'].sum()
                    ask_volume = ob_snapshot[ob_snapshot['side'] == 'ask']['volume'].sum()
                    total_volume = bid_volume + ask_volume
                    
                    if total_volume > 0:
                        imbalance = (bid_volume - ask_volume) / total_volume
                        
                        plt.figure(figsize=(12, 6))
                        plt.bar(['Bid', 'Ask'], [bid_volume, ask_volume])
                        plt.title(f'Order Book Volume for {stock} (Imbalance: {imbalance:.2f})')
                        plt.ylabel('Volume')
                        plt.tight_layout()
                        plt.savefig(output_path / f"{stock}_volume.png")
                        plt.close()


async def run_order_book_simulator(market_data_queue: asyncio.Queue, args):
    """
    Run the order book simulator asynchronously.
    
    Args:
        market_data_queue: Queue to publish market data updates
        args: Command line arguments
    """
    # Load data
    messages = load_json_data(args.input_file, args.num_messages)
    
    # Filter by stocks if specified
    if args.stocks:
        messages = filter_messages_by_stocks(messages, args.stocks)
    
    # Process messages asynchronously if trading engine is enabled
    if args.trading_engine:
        order_book = await process_messages_async(
            messages, 
            market_data_queue,
            update_interval=args.update_interval,
            tick_by_tick=args.tick_by_tick,
            verbose=args.verbose
        )
    else:
        # Process messages synchronously
        order_book = process_messages(messages, args.verbose)
    
    # Generate reports
    generate_reports(order_book, args.output_dir, not args.no_plots)
    
    print(f"\nProcessed {order_book.message_counts['total']} messages")
    print(f"Found {len(order_book.stocks)} unique stocks")
    print(f"Output saved to {args.output_dir}")
    
    return order_book

def main():
    """Main function to run the order book simulator."""
    args = parse_arguments()
    
    # Check if trading engine is enabled
    if args.trading_engine:
        # Create event loop
        loop = asyncio.get_event_loop()
        
        # Create market data queue
        market_data_queue = asyncio.Queue()
        
        # Run the order book simulator
        try:
            loop.run_until_complete(run_order_book_simulator(market_data_queue, args))
        except KeyboardInterrupt:
            logger.info("Order book simulator stopped by user")
        finally:
            loop.close()
    else:
        # Load data
        messages = load_json_data(args.input_file, args.num_messages)
        
        # Filter by stocks if specified
        if args.stocks:
            messages = filter_messages_by_stocks(messages, args.stocks)
        
        # Process messages
        order_book = process_messages(messages, args.verbose)
        
        # Generate reports
        generate_reports(order_book, args.output_dir, not args.no_plots)
        
        print(f"\nProcessed {order_book.message_counts['total']} messages")
        print(f"Found {len(order_book.stocks)} unique stocks")
        print(f"Output saved to {args.output_dir}")


if __name__ == "__main__":
    main()
