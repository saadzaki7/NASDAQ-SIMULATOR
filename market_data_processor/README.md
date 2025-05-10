# Market Data Processor

This component processes NASDAQ ITCH 5.0 market data from JSON files and provides functionality for:

1. Maintaining order books for each symbol
2. Calculating market statistics
3. Simulating market events
4. Generating trading signals

## Architecture

The Market Data Processor consists of several key components:

### MarketDataProcessor

The main class that:
- Loads and processes JSON-formatted ITCH 5.0 messages
- Maintains order books for each symbol
- Tracks market statistics
- Provides a CLI interface for testing

### OrderBook

Maintains the state of the limit order book for a single symbol:
- Tracks all orders with their price, size, and side
- Provides methods to add, cancel, execute, and replace orders
- Calculates order book metrics like best bid/ask, spread, and depth

### MarketStats

Calculates and maintains market statistics:
- VWAP (Volume Weighted Average Price)
- High/Low/Open/Last prices
- Trading volume and trade count
- Price change and volatility
- Order and trade imbalance

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

The CLI interface provides several commands for testing:

```
Market Data Processor CLI
-------------------------
Commands:
  load <file_path>           - Load ITCH 5.0 JSON data from file
  process <count>            - Process specified number of messages
  process_all                - Process all remaining messages
  stats                      - Show processing statistics
  symbols                    - List all symbols in the data
  book <symbol>              - Show order book for a symbol
  top_active <count>         - Show most active symbols by volume
  top_gainers <count>        - Show top gaining symbols
  top_losers <count>         - Show top losing symbols
  reset                      - Reset processor state
  help                       - Show this help message
  exit                       - Exit the program
```

## Integration Points

This component is designed to be integrated with:

1. **Trading Engine** - Will connect via gRPC to provide market data for trading strategies
2. **Dashboard** - Will provide market statistics and order book visualization

## Trading Signals

The following market signals can be derived from the processed data:

1. **Order Imbalance** - Ratio of buy vs. sell orders, indicating potential price movement
2. **Price Momentum** - Rate of change in price over time
3. **Volatility** - Standard deviation of price changes
4. **Spread Analysis** - Changes in bid-ask spread indicating liquidity
5. **VWAP Crossovers** - Price crossing above/below VWAP
6. **Volume Spikes** - Unusual increases in trading volume
7. **Order Book Pressure** - Imbalance between buy and sell side depth
8. **Trade Size Analysis** - Patterns in trade sizes indicating institutional activity

## Future Enhancements

1. Real-time gRPC interface for trading engine integration
2. WebSocket API for dashboard visualization
3. Additional market statistics and signals
4. Performance optimizations for high-throughput processing
