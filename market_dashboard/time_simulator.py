#!/usr/bin/env python3
import pandas as pd
import numpy as np
import os
import time
import logging
import json
from pathlib import Path

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger("time_simulator")

class TimeSimulator:
    """
    Simulates the passage of time for order book data.
    This class takes the mega CSV file and creates time-sliced versions
    to simulate the order book evolving over time.
    """
    
    def __init__(self, mega_csv_path, output_dir, time_step_seconds=5):
        """
        Initialize the time simulator.
        
        Args:
            mega_csv_path: Path to the mega CSV file
            output_dir: Directory to save time-sliced data
            time_step_seconds: Time step in seconds for each slice
        """
        self.mega_csv_path = mega_csv_path
        self.output_dir = Path(output_dir)
        self.time_step_seconds = time_step_seconds
        self.current_time_ns = None
        self.simulation_state_path = self.output_dir / "simulation_state.json"
        
        # Create output directory if it doesn't exist
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Load the mega CSV file
        self.load_data()
        
        # Initialize or load simulation state
        self.load_simulation_state()
    
    def load_data(self):
        """Load data from the mega CSV file."""
        logger.info(f"Loading data from {self.mega_csv_path}")
        try:
            self.df = pd.read_csv(self.mega_csv_path)
            
            # Ensure timestamp is sorted
            self.df = self.df.sort_values('timestamp')
            
            # Get the min and max timestamps
            self.min_timestamp = self.df['timestamp'].min()
            self.max_timestamp = self.df['timestamp'].max()
            
            logger.info(f"Loaded {len(self.df)} price updates from {self.mega_csv_path}")
            logger.info(f"Time range: {pd.to_datetime(self.min_timestamp / 1e9, unit='s')} to {pd.to_datetime(self.max_timestamp / 1e9, unit='s')}")
            
            return True
        except Exception as e:
            logger.error(f"Error loading data: {e}")
            return False
    
    def load_simulation_state(self):
        """Load or initialize the simulation state."""
        if self.simulation_state_path.exists():
            try:
                with open(self.simulation_state_path, 'r') as f:
                    state = json.load(f)
                    self.current_time_ns = state.get('current_time_ns', self.min_timestamp)
                    logger.info(f"Loaded simulation state: current_time_ns={self.current_time_ns}")
            except Exception as e:
                logger.error(f"Error loading simulation state: {e}")
                self.current_time_ns = self.min_timestamp
        else:
            self.current_time_ns = self.min_timestamp
            logger.info(f"Initialized simulation state: current_time_ns={self.current_time_ns}")
    
    def save_simulation_state(self):
        """Save the current simulation state."""
        try:
            with open(self.simulation_state_path, 'w') as f:
                json.dump({'current_time_ns': self.current_time_ns}, f)
            logger.info(f"Saved simulation state: current_time_ns={self.current_time_ns}")
        except Exception as e:
            logger.error(f"Error saving simulation state: {e}")
    
    def advance_time(self):
        """Advance the simulation time by one step."""
        # Calculate the next timestamp
        time_step_ns = self.time_step_seconds * 1e9
        next_time_ns = self.current_time_ns + time_step_ns
        
        # Ensure we don't go beyond the max timestamp
        if next_time_ns > self.max_timestamp:
            logger.info("Reached the end of the simulation, resetting to the beginning")
            next_time_ns = self.min_timestamp
        
        # Update the current time
        self.current_time_ns = next_time_ns
        
        # Save the simulation state
        self.save_simulation_state()
        
        # Create a time-sliced version of the data
        self.create_time_slice()
        
        return self.current_time_ns
    
    def create_time_slice(self):
        """Create a time-sliced version of the data up to the current time."""
        # Filter data up to the current time
        time_slice = self.df[self.df['timestamp'] <= self.current_time_ns]
        
        # Get the latest data point for each stock
        latest_data = time_slice.groupby('stock').apply(lambda x: x.iloc[-1]).reset_index(drop=True)
        
        # Save to CSV
        output_path = self.output_dir / "current_slice.csv"
        latest_data.to_csv(output_path, index=False)
        
        # Also save a JSON file with the current time for the dashboard
        current_time_dt = pd.to_datetime(self.current_time_ns / 1e9, unit='s')
        with open(self.output_dir / "current_time.json", 'w') as f:
            json.dump({
                'current_time_ns': int(self.current_time_ns),
                'current_time_str': current_time_dt.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3],
                'stocks_count': len(latest_data),
                'min_price': float(latest_data['mid'].min()),
                'max_price': float(latest_data['mid'].max()),
                'avg_price': float(latest_data['mid'].mean())
            }, f)
        
        logger.info(f"Created time slice at {current_time_dt} with {len(latest_data)} stocks")
        
        return output_path

def main():
    """Main function to run the time simulator."""
    import argparse
    
    parser = argparse.ArgumentParser(description="Time simulator for order book data")
    parser.add_argument(
        "--mega-csv",
        type=str,
        default="../output/all_price_updates.csv",
        help="Path to the mega CSV file"
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default="../output/time_simulation",
        help="Directory to save time-sliced data"
    )
    parser.add_argument(
        "--time-step",
        type=float,
        default=5.0,
        help="Time step in seconds for each slice"
    )
    parser.add_argument(
        "--run-once",
        action="store_true",
        help="Run the simulation once and exit"
    )
    parser.add_argument(
        "--continuous",
        action="store_true",
        help="Run the simulation continuously"
    )
    parser.add_argument(
        "--sleep",
        type=float,
        default=1.0,
        help="Sleep time in seconds between steps in continuous mode"
    )
    
    args = parser.parse_args()
    
    # Create the time simulator
    simulator = TimeSimulator(
        mega_csv_path=args.mega_csv,
        output_dir=args.output_dir,
        time_step_seconds=args.time_step
    )
    
    if args.run_once:
        # Run the simulation once
        simulator.advance_time()
    elif args.continuous:
        # Run the simulation continuously
        try:
            while True:
                simulator.advance_time()
                time.sleep(args.sleep)
        except KeyboardInterrupt:
            logger.info("Simulation stopped by user")
    else:
        # Create the initial time slice
        simulator.create_time_slice()
        logger.info("Created initial time slice")

if __name__ == "__main__":
    main()
