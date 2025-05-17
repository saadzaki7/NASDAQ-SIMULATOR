# Order Book Simulator

A Python-based order book simulator that processes NASDAQ ITCH 5.0 data from JSON files and creates a model of the limit order book over time.

## Overview

This project implements an order book simulator that:

1. Processes NASDAQ ITCH 5.0 messages from JSON files
2. Reconstructs the limit order book for each security
3. Tracks bids, asks, and price movements over time
4. Generates statistics and visualizations of the order book data

The simulator is designed for quantitative finance applications, particularly for testing trading strategies that rely on order book data, such as high-frequency trading (HFT) strategies.

## Components

The project consists of the following components:

- `order_book.py`: Core implementation of the order book data structure
- `main.py`: CLI tool for processing JSON data and generating reports
- `visualize.py`: Tool for creating visualizations of order book data
- `requirements.txt`: Python dependencies

## Installation

```bash
# Clone the repository
git clone <repository-url>
cd order_book_simulator

# Install dependencies
pip install -r requirements.txt
```

## Usage

### Processing ITCH Data

```bash
python main.py /path/to/itch_data.json [options]
```

Options:
- `-n, --num-messages`: Number of messages to process (default: all)
- `-s, --stocks`: Filter for specific stock symbols (e.g., `-s AAPL MSFT`)
- `-o, --output-dir`: Directory to save output files (default: `output`)
- `--no-plots`: Disable generation of plots
- `--verbose`: Enable verbose output

Example:
```bash
python main.py /Users/saadzaki/Documents/simulation_v2/itch_raw_file/01302019.NASDAQ_ITCH50.json -n 1000000 -s AAPL MSFT -o results
```

### Visualizing Order Book Data

```bash
python visualize.py /path/to/output_dir [options]
```

Options:
- `-s, --stocks`: Filter for specific stock symbols
- `-o, --output-dir`: Directory to save visualizations (default: `input_dir/visualizations`)
- `--interactive`: Generate interactive Plotly visualizations

Example:
```bash
python visualize.py results -s AAPL --interactive
```

## Data Structure

The order book simulator maintains the following data structures:

1. **Orders**: A dictionary of all active orders indexed by reference ID
2. **Books**: A dictionary of order books for each stock, with separate collections for bids and asks
3. **Best Prices**: Tracks the current best bid and ask for each stock
4. **Price History**: Records the time series of bid, ask, and mid prices for each stock

## Message Types

The simulator processes the following NASDAQ ITCH 5.0 message types:

- **Add Order**: Adds a new order to the book
- **Delete Order**: Removes an order from the book
- **Order Executed**: Handles full or partial execution of an order
- **Order Cancelled**: Handles full or partial cancellation of an order
- **Replace Order**: Replaces an existing order with a new one

## Output

The simulator generates the following outputs:

1. **Summary Statistics**: Overall statistics for each stock (spread, imbalance, etc.)
2. **Order Book Snapshots**: Current state of the order book for each stock
3. **Price History**: Time series of bid, ask, and mid prices for each stock
4. **Visualizations**: Plots of order book data and price movements

## Example HFT Strategy

The order book data can be used to implement HFT strategies such as:

```python
def order_book_imbalance_strategy(order_book, stock, threshold=2.0, holding_period=5):
    """
    A simple HFT strategy based on order book imbalance.
    
    Args:
        order_book: OrderBook instance
        stock: Stock symbol
        threshold: Imbalance threshold (bid_volume / ask_volume)
        holding_period: Number of ticks to hold position
    
    Returns:
        Action: "buy", "sell", or "hold"
    """
    # Get current order book
    ob_snapshot = order_book.get_order_book_snapshot(stock)
    
    # Calculate imbalance
    bid_volume = ob_snapshot[ob_snapshot['side'] == 'bid']['volume'].sum()
    ask_volume = ob_snapshot[ob_snapshot['side'] == 'ask']['volume'].sum()
    
    if ask_volume == 0:
        return "hold"
    
    imbalance = bid_volume / ask_volume
    
    # Trading logic
    if imbalance > threshold:
        return "buy"  # Strong buying pressure
    elif imbalance < 1/threshold:
        return "sell"  # Strong selling pressure
    else:
        return "hold"
```

## License

MIT

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
