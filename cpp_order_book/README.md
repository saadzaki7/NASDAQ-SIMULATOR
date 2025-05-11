# C++ Order Book Processor

A high-performance C++ implementation of an order book processor for NASDAQ ITCH 5.0 market data. This implementation is designed for maximum speed and efficiency, processing market data messages at rates significantly faster than the Python implementation.

## Features

- Efficient processing of market data messages with O(1) and O(log n) operations
- Direct processing of order book updates without Python overhead
- Minimal memory allocation and data structure optimizations
- Support for all order operations (add, execute, delete, cancel, replace)
- Real-time market data output for trading strategy consumption

## Dependencies

- CMake (3.12+)
- C++17 compatible compiler
- nlohmann_json library (for JSON parsing)

## Building

```bash
# Give execute permission to the build script if needed
chmod +x build.sh

# Run the build script
./build.sh
```

## Usage

```bash
./order_book_processor <input_file> [num_messages] [output_file] [stocks...]
```

### Parameters

- `input_file`: Path to the JSON file containing ITCH 5.0 data (required)
- `num_messages`: Number of messages to process (0 for all messages, default: 0)
- `output_file`: File to save market data output (default: market_data.jsonl)
- `stocks`: Optional list of stock symbols to filter (e.g., AAPL MSFT GOOG)

### Example

```bash
# Process 250,000 messages for AAPL and MSFT
./order_book_processor ../data/itch_data.json 250000 market_data.jsonl AAPL MSFT

# Process all messages for all stocks
./order_book_processor ../data/itch_data.json
```

## Performance

The C++ implementation typically processes hundreds of thousands to millions of messages per second, depending on hardware. This represents a significant performance improvement over the Python implementation.

## Integration with Trading Strategies

To use this with your trading strategies:

1. Run the C++ order book processor to generate market data as a JSON Lines file
2. Load and analyze the market data in your trading strategy implementation
3. Execute trades based on your strategy's analysis

## Design Notes

This implementation uses efficient data structures:
- `std::unordered_map` for O(1) order lookup by reference ID
- `std::map` for price levels (provides ordered access to prices)
- Pre-computed best prices and volumes for fast access

The order book maintains separate structures for buy and sell orders, allowing for efficient market data generation.
