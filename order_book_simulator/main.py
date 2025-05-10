#!/usr/bin/env python3
import argparse
import json
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from typing import List, Dict, Any
from tqdm import tqdm
import sys
import os
import logging
import time

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
    order_book = OrderBook()
    start_time = time.time()
    
    logger.info(f"Processing {len(messages)} messages...")
    
    # Track message types for debugging
    message_types = {}
    stock_counts = {}
    
    for i, msg in enumerate(tqdm(messages)):
        # Process the message
        order_book.process_message(msg)
        
        # Debug: Track message types
        if 'body' in msg and msg['body']:
            msg_type = list(msg['body'].keys())[0] if msg['body'] else 'Unknown'  
            message_types[msg_type] = message_types.get(msg_type, 0) + 1
            
            # Track stocks
            if msg_type in ['AddOrder', 'DeleteOrder', 'OrderExecuted']:
                body_content = msg['body'][msg_type]
                if 'stock' in body_content:
                    stock = body_content['stock'].strip()
                    stock_counts[stock] = stock_counts.get(stock, 0) + 1
        
        # Log progress
        if (i+1) % 1000 == 0:
            elapsed = time.time() - start_time
            rate = (i+1) / elapsed if elapsed > 0 else 0
            logger.info(f"Processed {i+1}/{len(messages)} messages ({(i+1)/len(messages)*100:.1f}%). Rate: {rate:.1f} msgs/sec")
            
            if verbose:
                # Log current order book stats
                logger.info(f"Current stats: {len(order_book.stocks)} stocks, {len(order_book.orders)} active orders")
                
                # Log message type distribution
                logger.info(f"Message types: {message_types}")
                
                # Log top 5 stocks by message count
                top_stocks = sorted(stock_counts.items(), key=lambda x: x[1], reverse=True)[:5]
                logger.info(f"Top stocks: {top_stocks}")
    
    elapsed_time = time.time() - start_time
    logger.info(f"Processed {len(messages)} messages in {elapsed_time:.2f} seconds ({len(messages)/elapsed_time:.1f} msgs/sec)")
    logger.info(f"Final message type distribution: {message_types}")
    logger.info(f"Found {len(order_book.stocks)} unique stocks")
    
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


def main():
    """Main function to run the order book simulator."""
    args = parse_arguments()
    
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
