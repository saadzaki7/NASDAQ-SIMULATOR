#!/usr/bin/env python3
import argparse
import asyncio
import logging
import os
import signal
import sys
from pathlib import Path

# Configure logging
logging.basicConfig(
    level=logging.WARNING,  # Change to WARNING to reduce log output and improve performance
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler("integrated_simulator.log"),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger("integrated_simulator")
logger.setLevel(logging.INFO)  # Keep main simulator logs at INFO level

# Import order book simulator components
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'order_book_simulator'))
from order_book import OrderBook
from main import load_json_data, filter_messages_by_stocks, process_messages_async, generate_reports

# Import trading engine components
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'trading_engine'))
from trading_engine import TradingEngine

async def run_order_book_simulator(market_data_queue, args):
    """
    Run the order book simulator asynchronously.
    
    Args:
        market_data_queue: Queue to publish market data updates
        args: Command line arguments
    """
    logger.info("Starting order book simulator...")
    
    # Load data
    messages = load_json_data(args.input_file, args.num_messages)
    
    # Filter by stocks if specified
    if args.stocks:
        messages = filter_messages_by_stocks(messages, args.stocks)
    
    # Process messages asynchronously
    order_book = await process_messages_async(
        messages, 
        market_data_queue,
        update_interval=args.update_interval,
        tick_by_tick=args.tick_by_tick,
        verbose=args.verbose
    )
    
    # Generate reports
    generate_reports(order_book, args.output_dir, not args.no_plots)
    
    logger.info(f"Processed {order_book.message_counts['total']} messages")
    logger.info(f"Found {len(order_book.stocks)} unique stocks")
    logger.info(f"Output saved to {args.output_dir}")
    
    return order_book

async def run_trading_engine(market_data_queue, order_queue, args):
    """
    Run the trading engine asynchronously.
    
    Args:
        market_data_queue: Queue to receive market data updates
        order_queue: Queue to send orders
        args: Command line arguments
    """
    logger.info("Starting trading engine...")
    
    # Create the trading engine
    engine = TradingEngine(
        market_data_queue=market_data_queue,
        order_queue=order_queue,
        initial_capital=args.capital,
        output_dir=args.trading_output_dir
    )
    
    # Run the engine
    await engine.run()
    
    logger.info("Trading engine finished")

async def run_integrated_simulation(args):
    """
    Run the integrated simulation with order book simulator and trading engine.
    
    Args:
        args: Command line arguments
    """
    logger.info("Starting integrated simulation")
    
    # Create queues for communication between components
    market_data_queue = asyncio.Queue()
    order_queue = asyncio.Queue()
    
    logger.info(f"Created communication queues: market_data_queue={market_data_queue}, order_queue={order_queue}")
    
    # Create tasks
    logger.info("Creating order book simulator task")
    order_book_task = asyncio.create_task(
        run_order_book_simulator(market_data_queue, args)
    )
    
    logger.info("Creating trading engine task")
    trading_engine_task = asyncio.create_task(
        run_trading_engine(market_data_queue, order_queue, args)
    )
    
    # Wait for both tasks to complete
    logger.info("Waiting for simulation tasks to complete")
    try:
        await asyncio.gather(order_book_task, trading_engine_task)
        logger.info("Simulation completed successfully")
    except Exception as e:
        logger.error(f"Error during simulation: {e}")
        raise

def parse_arguments():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="Integrated Order Book Simulator and Trading Engine"
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
        help="Directory to save order book output files"
    )
    parser.add_argument(
        "--trading-output-dir", 
        type=str, 
        default="trading_output", 
        help="Directory to save trading output files"
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
        "--update-interval",
        type=float,
        default=0.0,
        help="Interval between market data updates in seconds (default: 0.0, 0=as fast as possible)"
    )
    parser.add_argument(
        "--tick-by-tick",
        action="store_true",
        help="Send market data updates for every tick"
    )
    parser.add_argument(
        "--capital",
        type=float,
        default=1000000.0,
        help="Initial capital for trading engine"
    )
    
    return parser.parse_args()

def main():
    """Main function to run the integrated simulation."""
    args = parse_arguments()
    
    # Create event loop
    loop = asyncio.get_event_loop()
    
    # Set up signal handlers for graceful shutdown
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, lambda: asyncio.create_task(shutdown(loop)))
    
    # Run the integrated simulation
    try:
        loop.run_until_complete(run_integrated_simulation(args))
    except KeyboardInterrupt:
        logger.info("Simulation stopped by user")
    except Exception as e:
        logger.error(f"Error running simulation: {e}")
    finally:
        loop.close()

async def shutdown(loop):
    """Gracefully shut down the simulation."""
    logger.info("Shutting down simulation...")
    
    # Cancel all running tasks
    tasks = [t for t in asyncio.all_tasks() if t is not asyncio.current_task()]
    for task in tasks:
        task.cancel()
    
    await asyncio.gather(*tasks, return_exceptions=True)
    loop.stop()

if __name__ == "__main__":
    main()
