# NASDAQ ITCH 5.0 Parser

This is a parser for the NASDAQ ITCH 5.0 protocol, which is used for market data feeds. The parser processes raw ITCH 5.0 binary files and outputs the data in JSON format. Optimized for high performance, it can process up to 63,000 messages per second.

## Architecture

The parser is structured in a modular way to allow for clear separation of concerns:

1. **Enums and Data Structures** - In `enums.h` and `message.h`, defining all the message types and enum values from the ITCH 5.0 specification.
2. **Binary Parser** - In `parser.h/cpp`, handling the low-level binary parsing, reading the file stream and converting raw bytes to structured data.
3. **JSON Serializer** - In `json_serializer.h/cpp`, converting parsed messages to JSON format.
4. **Main Application** - Command-line interface in `main.cpp` that ties everything together.

## Building

To build the parser, you need a C++17-compatible compiler and CMake:

```
mkdir build
cd build
cmake ..
make
```

## Dependencies

- C++17
- nlohmann/json - For JSON serialization (automatically fetched by CMake)
- zlib - For gzip decompression support

## Usage

```
./itch_parser [options] <path-to-itch-file>
```

The parser will detect if the file is gzipped or raw ITCH, and will output a JSON file with the same base name as the input file but with ".json" appended.

### Command Line Options

```
Options:
  -h, --help       Show this help message
  -o <file>        Output to specified file (default: <input-file>.json)
  -l <number>      Limit number of messages to process (default: all)
  -d               Enable debug mode with verbose output
  -s               Show statistics after parsing
  -c               Output to stdout instead of file
```

### Examples

```
# Basic usage
./itch_parser data.itch

# Process only 2 million messages
./itch_parser -l 2000000 data.itch

# Custom output file
./itch_parser -o output.json data.itch

# Show statistics
./itch_parser -s data.itch
```

## Design Decisions

1. **Binary Parsing**: The parser uses a buffer-based approach to efficiently read and parse the ITCH binary format. It reads data in chunks to minimize I/O operations.

2. **Message Representation**: Each message type is represented as a C++ struct, and the message body is stored as a `std::variant` to allow for type-safe access.

3. **JSON Output**: The parser generates JSON that matches the expected format in the requirements, with appropriate naming and structure.

4. **Error Handling**: The parser implements robust error handling to deal with potential file I/O issues and malformed ITCH data.

5. **Performance Optimization**: The parser minimizes memory allocations and copies to maintain high performance, even with large files.

## Output Format

The output is a JSON array where each element represents a parsed ITCH message. Each message has the following format:

```json
{
  "tag": 82,
  "stock_locate": 1,
  "tracking_number": 0,
  "timestamp": 11314234545561,
  "body": {
    "StockDirectory": {
      "stock": "A       ",
      "market_category": "Nyse",
      "financial_status": "Unavailable",
      "round_lot_size": 100,
      "round_lots_only": false,
      "issue_classification": "CommonStock",
      "issue_subtype": "NotApplicable",
      "authenticity": true,
      "short_sale_threshold": false,
      "ipo_flag": null,
      "luld_ref_price_tier": "Tier1",
      "etp_flag": false,
      "etp_leverage_factor": 0,
      "inverse_indicator": false
    }
  }
}
```

## Performance

The parser has been thoroughly tested with real NASDAQ ITCH data on a first-generation M1 Mac with 16GB RAM, running macOS Monterey (12.4) and using the Apple clang compiler (version 13.1.6). Below are the performance benchmarks:

| Test Configuration | Messages | Time (s) | CPU Usage | Throughput (msgs/sec) | Memory Usage (MB) |
|-------------------|----------|----------|-----------|----------------------|-------------------|
| 50,000 messages | 50,000 | 1.039 | 91% | ~48,123 | 304 |
| 100,000 messages | 100,000 | 1.967 | 94% | ~50,839 | 272 |
| 200,000 messages | 200,000 | 3.729 | 95% | ~53,634 | 304 |
| 500,000 messages | 500,000 | 7.739 | 96% | ~64,608 | 304 |
| 1,000,000 messages | 1,000,000 | 14.320 | 93% | ~69,832 | 272 |
| 10,000,000 messages | 10,000,000 | 138.095 | 98% | ~72,414 | 272 |

### Key Performance Characteristics

1. **Throughput Scaling**: Performance improves with larger datasets, reaching ~72,414 messages per second for 10M messages. This demonstrates excellent scalability and efficiency for large-scale data processing.

2. **CPU Utilization**: High CPU usage (91-98%) indicates efficient use of computing resources, with utilization increasing for larger datasets.

3. **Memory Efficiency**: The parser demonstrates performant memory management, with consistent memory usage (272-304 MB) regardless of dataset size from 50K to 10M messages. This flat memory profile allows for processing very large datasets without memory scaling issues.

4. **Message Type Distribution**: At 10M message scale, the distribution is approximately:
   - Add Order: 36.7%
   - Order Delete: 35.5%
   - Order Cancel: 10.7%
   - NOII (Non-Opening/Closing Imbalance Indicator): 7.5%
   - Order Replace: 4.5%
   - Add Order with MPID: 2.5%
   - Market Participant Position: 1.9%
   - Order Executed: 0.4%
   - Other types: 0.3%

