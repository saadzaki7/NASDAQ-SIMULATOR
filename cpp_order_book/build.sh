#!/bin/bash
# Build script for C++ order book processor

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build
make -j4

# Copy executable to main directory
cp order_book_processor ..

echo "C++ order book processor built successfully!"
echo "Run with: ./order_book_processor <input_file> [num_messages] [output_file] [stocks...]"
echo "Example: ./order_book_processor ../data/itch_data.json 250000 market_data.jsonl AAPL MSFT"
