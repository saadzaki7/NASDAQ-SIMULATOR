#!/usr/bin/env python3
import argparse
import logging
import os
import time
import json
import pandas as pd
import numpy as np
from pathlib import Path
import grpc
from concurrent import futures
import threading
import sys

# Add the grpc directory to the Python path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'grpc', 'generated')))

# Import the generated gRPC code (will be imported after generation)
# import market_data_pb2
# import market_data_pb2_grpc

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler("market_data_server.log"),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger("market_data_server")

# Import the OrderBook class
from order_book import OrderBook


class MarketDataServicer:
    """
    Implementation of the MarketDataService gRPC service.
    
    This class handles gRPC requests from the Trading Engine and provides
    market data from the Order Book Simulator.
    """
    
    def __init__(self, order_book):
        """
        Initialize the market data servicer.
        
        Args:
            order_book: The OrderBook instance to get data from
        """
        self.order_book = order_book
        self.clients = {}  # client_id -> {stream, securities, interval}
        self.next_client_id = 1
        self.lock = threading.Lock()
        self.running = True
        
        # Start the update thread
        self.update_thread = threading.Thread(target=self.update_clients)
        self.update_thread.daemon = True
        self.update_thread.start()
    
    def stop(self):
        """Stop the servicer and its update thread."""
        self.running = False
        if self.update_thread.is_alive():
            self.update_thread.join(timeout=2.0)
    
    def update_clients(self):
        """Update all clients with the latest market data."""
        while self.running:
            try:
                # Get a copy of the clients dictionary to avoid modification during iteration
                with self.lock:
                    clients = self.clients.copy()
                
                # Update each client
                for client_id, client_info in clients.items():
                    try:
                        # Check if it's time to send an update
                        current_time = time.time() * 1000  # milliseconds
                        if current_time - client_info.get('last_update', 0) < client_info.get('interval', 0):
                            continue
                        
                        # Get the securities to update
                        securities = client_info.get('securities', [])
                        
                        # If no securities specified, use all available
                        if not securities:
                            securities = list(self.order_book.stocks)
                        
                        # Send updates for each security
                        for symbol in securities:
                            try:
                                # Get the latest data for this security
                                market_data = self.get_market_data_for_security(symbol)
                                
                                if market_data:
                                    # Send the update
                                    client_info['stream'].write(market_data)
                            except Exception as e:
                                logger.error(f"Error sending update for {symbol} to client {client_id}: {e}")
                        
                        # Update the last update time
                        client_info['last_update'] = current_time
                    
                    except Exception as e:
                        logger.error(f"Error updating client {client_id}: {e}")
                        # Remove the client if there was an error
                        with self.lock:
                            if client_id in self.clients:
                                del self.clients[client_id]
                
                # Sleep to avoid busy waiting
                time.sleep(0.01)
            
            except Exception as e:
                logger.error(f"Error in update thread: {e}")
                time.sleep(1.0)  # Sleep longer on error
    
    def get_market_data_for_security(self, symbol):
        """
        Get the latest market data for a security.
        
        Args:
            symbol: Security symbol
        
        Returns:
            MarketDataUpdate protobuf message or None if data not available
        """
        try:
            # Get the order book snapshot
            ob_snapshot = self.order_book.get_order_book_snapshot(symbol)
            
            # Get the price history
            price_history = self.order_book.get_price_history(symbol)
            
            # Check if we have data
            if ob_snapshot.empty or price_history.empty:
                return None
            
            # Get the latest price data
            latest_price = price_history.iloc[-1]
            
            # Calculate volumes
            bid_volume = ob_snapshot[ob_snapshot['side'] == 'bid']['volume'].sum()
            ask_volume = ob_snapshot[ob_snapshot['side'] == 'ask']['volume'].sum()
            
            # Calculate imbalance
            total_volume = bid_volume + ask_volume
            imbalance = 0.0
            if total_volume > 0:
                imbalance = (bid_volume - ask_volume) / total_volume
            
            # Create the market data update
            market_data = market_data_pb2.MarketDataUpdate(
                timestamp=int(latest_price['timestamp']),
                symbol=symbol.strip(),
                bid_price=float(latest_price['bid']),
                ask_price=float(latest_price['ask']),
                mid_price=float(latest_price['mid']),
                bid_volume=int(bid_volume),
                ask_volume=int(ask_volume),
                imbalance=float(imbalance),
                spread=float(latest_price['ask'] - latest_price['bid']),
                spread_pct=float((latest_price['ask'] - latest_price['bid']) / latest_price['mid'] * 100)
            )
            
            return market_data
        
        except Exception as e:
            logger.error(f"Error getting market data for {symbol}: {e}")
            return None
    
    def StreamMarketData(self, request, context):
        """
        Implementation of the StreamMarketData gRPC method.
        
        Args:
            request: StreamRequest message
            context: gRPC context
        
        Returns:
            Generator of MarketDataUpdate messages
        """
        # Get request parameters
        securities = list(request.securities)
        interval = request.update_interval_ms
        
        # Log the request
        logger.info(f"Received StreamMarketData request: {len(securities)} securities, interval={interval}ms")
        
        # Generate a client ID
        client_id = self.next_client_id
        self.next_client_id += 1
        
        # Create a queue for this client
        with self.lock:
            self.clients[client_id] = {
                'stream': context,
                'securities': securities,
                'interval': interval,
                'last_update': 0
            }
        
        try:
            # Keep the stream open until the client disconnects
            while context.is_active():
                time.sleep(0.1)
        except Exception as e:
            logger.error(f"Error in StreamMarketData for client {client_id}: {e}")
        finally:
            # Remove the client
            with self.lock:
                if client_id in self.clients:
                    del self.clients[client_id]
            
            logger.info(f"Client {client_id} disconnected")
    
    def GetOrderBookSnapshot(self, request, context):
        """
        Implementation of the GetOrderBookSnapshot gRPC method.
        
        Args:
            request: SecurityRequest message
            context: gRPC context
        
        Returns:
            OrderBookSnapshot message
        """
        symbol = request.symbol
        
        logger.info(f"Received GetOrderBookSnapshot request for {symbol}")
        
        try:
            # Get the order book snapshot
            ob_snapshot = self.order_book.get_order_book_snapshot(symbol)
            
            # Check if we have data
            if ob_snapshot.empty:
                context.set_code(grpc.StatusCode.NOT_FOUND)
                context.set_details(f"Order book snapshot not found for {symbol}")
                return market_data_pb2.OrderBookSnapshot()
            
            # Get the latest timestamp
            price_history = self.order_book.get_price_history(symbol)
            timestamp = int(price_history.iloc[-1]['timestamp']) if not price_history.empty else 0
            
            # Create the order book snapshot
            snapshot = market_data_pb2.OrderBookSnapshot(
                symbol=symbol.strip(),
                timestamp=timestamp
            )
            
            # Add price levels
            for _, row in ob_snapshot.iterrows():
                level = snapshot.levels.add()
                level.price = float(row['price'])
                level.volume = int(row['volume'])
                level.side = str(row['side'])
            
            return snapshot
        
        except Exception as e:
            logger.error(f"Error in GetOrderBookSnapshot for {symbol}: {e}")
            context.set_code(grpc.StatusCode.INTERNAL)
            context.set_details(f"Internal error: {e}")
            return market_data_pb2.OrderBookSnapshot()
    
    def GetAllSecuritiesData(self, request, context):
        """
        Implementation of the GetAllSecuritiesData gRPC method.
        
        Args:
            request: Empty message
            context: gRPC context
        
        Returns:
            AllSecuritiesData message
        """
        logger.info("Received GetAllSecuritiesData request")
        
        try:
            # Create the response
            response = market_data_pb2.AllSecuritiesData(
                timestamp=int(time.time() * 1e9)
            )
            
            # Add data for each security
            for symbol in self.order_book.stocks:
                market_data = self.get_market_data_for_security(symbol)
                if market_data:
                    response.securities.append(market_data)
            
            return response
        
        except Exception as e:
            logger.error(f"Error in GetAllSecuritiesData: {e}")
            context.set_code(grpc.StatusCode.INTERNAL)
            context.set_details(f"Internal error: {e}")
            return market_data_pb2.AllSecuritiesData()


class MarketDataServer:
    """
    gRPC server for market data.
    
    This class runs a gRPC server that provides market data from the
    Order Book Simulator to the Trading Engine.
    """
    
    def __init__(self, order_book, port=50051, max_workers=10):
        """
        Initialize the market data server.
        
        Args:
            order_book: The OrderBook instance to get data from
            port: Port to listen on
            max_workers: Maximum number of worker threads
        """
        self.order_book = order_book
        self.port = port
        self.max_workers = max_workers
        self.server = None
        self.servicer = None
    
    def start(self):
        """Start the gRPC server."""
        try:
            # Create the server
            self.server = grpc.server(futures.ThreadPoolExecutor(max_workers=self.max_workers))
            
            # Create the servicer
            self.servicer = MarketDataServicer(self.order_book)
            
            # Add the servicer to the server
            market_data_pb2_grpc.add_MarketDataServiceServicer_to_server(
                self.servicer, self.server)
            
            # Add a port
            address = f"[::]:{self.port}"
            self.server.add_insecure_port(address)
            
            # Start the server
            self.server.start()
            
            logger.info(f"Market data server started, listening on {address}")
            
            return True
        
        except Exception as e:
            logger.error(f"Error starting market data server: {e}")
            return False
    
    def stop(self):
        """Stop the gRPC server."""
        if self.servicer:
            self.servicer.stop()
        
        if self.server:
            logger.info("Stopping market data server...")
            self.server.stop(grace=1.0)
            logger.info("Market data server stopped")
    
    def wait_for_termination(self):
        """Wait for the server to terminate."""
        if self.server:
            self.server.wait_for_termination()


def main():
    """Main function to run the market data server."""
    parser = argparse.ArgumentParser(
        description="Market Data Server for Order Book Simulator"
    )
    parser.add_argument(
        "--port", 
        type=int, 
        default=50051, 
        help="Port to listen on"
    )
    parser.add_argument(
        "--input-file", 
        type=str, 
        required=True,
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
    
    args = parser.parse_args()
    
    try:
        # Import the main module functions
        from main import load_json_data, filter_messages_by_stocks, process_messages
        
        # Load data
        messages = load_json_data(args.input_file, args.num_messages)
        
        # Filter by stocks if specified
        if args.stocks:
            messages = filter_messages_by_stocks(messages, args.stocks)
        
        # Process messages
        order_book = process_messages(messages, verbose=False)
        
        # Create and start the server
        server = MarketDataServer(order_book, port=args.port)
        if not server.start():
            sys.exit(1)
        
        # Wait for termination
        try:
            server.wait_for_termination()
        except KeyboardInterrupt:
            server.stop()
    
    except Exception as e:
        logger.error(f"Error running market data server: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
