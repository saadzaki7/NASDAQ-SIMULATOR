# NASDAQ Simulator

A multi-component system for simulating an order book, visualizing market data, and executing trading strategies.

## System Architecture

The NASDAQ Simulator consists of the following components:

1. **Order Book Simulator**: Processes NASDAQ ITCH 5.0 data to maintain order books for multiple securities.
2. **Trading Engine**: Executes trading strategies based on market data received from the Order Book Simulator.
3. **Market Dashboard**: Visualizes market data and trading performance.
4. **Asyncio Communication Layer**: Enables low-latency intra-process communication between components.

## Directory Structure

```
simulation_v2/
├── architecture.md        # Detailed architecture documentation
├── cpp_parser/            # C++ parser for ITCH 5.0 data
├── integrated_simulator.py # Integrated script combining order book simulator and trading engine
├── itch_raw_file/         # Raw ITCH 5.0 data files
├── market_dashboard/      # Dashboard for visualizing market data
├── order_book_simulator/  # Order book simulator component
│   ├── main.py            # Main script for processing ITCH data
│   └── order_book.py      # Order book implementation
├── output/                # Output files (CSV, plots, etc.)
└── trading_engine/        # Trading engine component
    ├── trading_engine.py  # Trading engine implementation
    └── requirements.txt   # Dependencies for the trading engine
```

## Getting Started

### Prerequisites

- Python 3.8+
- pandas, numpy, and other dependencies listed in requirements.txt files

### Installation

1. Clone the repository:
   ```
   git clone https://github.com/yourusername/simulation_v2.git
   cd simulation_v2
   ```

2. Install the required dependencies for each component:
   ```
   pip install -r order_book_simulator/requirements.txt
   pip install -r market_dashboard/requirements.txt
   pip install -r trading_engine/requirements.txt
   ```

### Running the System

#### Integrated Simulator

The easiest way to run the system is to use the integrated simulator, which combines the order book simulator and trading engine in a single process with asyncio-based communication:

```
python integrated_simulator.py --input-file itch_raw_file/01302019.NASDAQ_ITCH50.json --stocks AAPL MSFT AMZN --capital 1000000
```

This will:
1. Process the ITCH data and build order books for the specified stocks
2. Send market data updates to the trading engine via asyncio queues
3. Execute the HFT strategy in the trading engine
4. Generate reports and performance metrics

#### Running Components Separately

You can also run the components separately:

1. Run the Order Book Simulator:
   ```
   cd order_book_simulator
   python main.py ../itch_raw_file/01302019.NASDAQ_ITCH50.json --stocks AAPL MSFT AMZN
   ```

2. Start the Market Dashboard to visualize the results:
   ```
   cd market_dashboard
   python app.py --mega-csv ../output/all_price_updates.csv
   ```

## Components

### Order Book Simulator

The Order Book Simulator processes NASDAQ ITCH 5.0 data and maintains order books for multiple securities. It exports a "mega CSV" file with all price updates and provides market data to the Trading Engine via asyncio queues for ultra-low latency communication.

### Trading Engine

The Trading Engine implements an HFT strategy based on order book imbalance. It receives market data from the Order Book Simulator via asyncio queues, executes trades based on the strategy, and tracks P&L and trade history. The engine outputs performance summaries and trade logs.

### Market Dashboard

The Market Dashboard visualizes market data from the mega CSV file. It shows price charts, order book snapshots, and market overviews.

### Asyncio Communication Layer

The asyncio-based communication layer enables ultra-low latency (~1-10μs) intra-process communication between the Order Book Simulator and the Trading Engine. This approach eliminates network overhead and serialization costs, making it ideal for high-frequency trading simulations.

## HFT Strategy

The Trading Engine implements a realistic HFT strategy based on order book imbalance:

1. Monitor LOB imbalance (bid volume > ask volume by a factor of 2)
2. If imbalance persists for 5 consecutive ticks, place a buy order
3. Sell after a 0.1% gain or 0.05% loss
4. Cancel unfilled orders after 10 ticks

## Performance Benefits

The asyncio-based intra-process communication provides several advantages over network-based approaches:

1. **Ultra-low latency**: ~1-10μs vs. gRPC's ~10-100μs
2. **No serialization overhead**: Data is passed directly as Python objects
3. **No network stack**: Eliminates TCP/IP overhead and potential network issues
4. **Simplified development**: No need for protocol definitions or server setup
5. **Realistic HFT simulation**: Better represents the microsecond-level timing critical for HFT strategies

## Future Enhancements

1. Add more sophisticated HFT strategies (e.g., statistical arbitrage, market making)
2. Implement real-time visualization of trading performance
3. Add support for multi-asset strategies and portfolio optimization
4. Enhance the order book simulator with more realistic market microstructure features

## License

This project is licensed under the MIT License - see the LICENSE file for details.
