#!/usr/bin/env python3
import argparse
import sys
import os
import logging
from pathlib import Path

# Add the parent directory to the path so we can import the order_book_simulator modules
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from order_book_simulator.order_book import OrderBook

# Import functions from main.py directly to avoid circular imports
def load_json_data(file_path, num_messages=0):
    """Load JSON data from file."""
    import json
    logger.info(f"Loading data from {file_path}...")
    
    try:
        with open(file_path, 'r') as f:
            # Check first character to see if it's a JSON array
            first_char = f.read(1)
            f.seek(0)
            
            if first_char == '[':
                # Load as JSON array
                data = json.load(f)
                if num_messages > 0:
                    data = data[:num_messages]
            else:
                # Load line by line (JSON Lines format)
                data = []
                for i, line in enumerate(f):
                    if num_messages > 0 and i >= num_messages:
                        break
                    try:
                        data.append(json.loads(line))
                    except json.JSONDecodeError:
                        logger.warning(f"Could not parse line {i+1}")
        
        logger.info(f"Loaded {len(data)} messages")
        return data
    
    except (json.JSONDecodeError, FileNotFoundError) as e:
        logger.error(f"Error loading JSON data: {e}")
        sys.exit(1)

def filter_messages_by_stocks(messages, stocks):
    """Filter messages to only include those for specific stocks."""
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
    
    logger.info(f"Filtered to {len(filtered_messages)} messages for stocks: {', '.join(stocks)}")
    return filtered_messages

def process_messages(messages, verbose=False):
    """Process messages and build the order book."""
    from tqdm import tqdm
    import time
    
    order_book = OrderBook()
    start_time = time.time()
    
    logger.info(f"Processing {len(messages)} messages...")
    
    for i, msg in enumerate(tqdm(messages)):
        # Process the message
        order_book.process_message(msg)
        
        # Log progress
        if verbose and (i+1) % 10000 == 0:
            logger.info(f"Processed {i+1}/{len(messages)} messages")
    
    elapsed_time = time.time() - start_time
    logger.info(f"Processed {len(messages)} messages in {elapsed_time:.2f} seconds")
    logger.info(f"Found {len(order_book.stocks)} unique stocks")
    
    return order_book

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger("generate_data")

def parse_arguments():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="Generate mega CSV file for the market dashboard."
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
        "--verbose", 
        action="store_true", 
        help="Enable verbose output"
    )
    
    return parser.parse_args()

def main():
    """Main function to generate the mega CSV file."""
    args = parse_arguments()
    
    # Create output directory if it doesn't exist
    output_path = Path(args.output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    # Load data
    logger.info(f"Loading data from {args.input_file}...")
    messages = load_json_data(args.input_file, args.num_messages)
    
    # Filter by stocks if specified
    if args.stocks:
        logger.info(f"Filtering for stocks: {', '.join(args.stocks)}")
        messages = filter_messages_by_stocks(messages, args.stocks)
    
    # Process messages
    logger.info(f"Processing {len(messages)} messages...")
    order_book = process_messages(messages, args.verbose)
    
    # Export mega CSV
    logger.info("Exporting mega CSV file...")
    mega_csv_path = order_book.export_mega_csv(args.output_dir)
    
    if mega_csv_path:
        logger.info(f"Successfully exported mega CSV to {mega_csv_path}")
        logger.info(f"Found {len(order_book.stocks)} unique stocks")
    else:
        logger.error("Failed to export mega CSV file")
    
if __name__ == "__main__":
    main()
