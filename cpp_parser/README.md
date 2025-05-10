# NASDAQ ITCH 5.0 Parser

This is a parser for the NASDAQ ITCH 5.0 protocol, which is used for market data feeds. The parser processes raw ITCH 5.0 binary files and outputs the data in JSON format.

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
./itch_parser <path-to-itch-file>
```

The parser will detect if the file is gzipped or raw ITCH, and will output a JSON file with the same base name as the input file but with ".json" appended.

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

## License

This code is provided as-is with no warranties.
