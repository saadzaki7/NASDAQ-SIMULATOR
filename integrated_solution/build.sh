#!/bin/bash
# Build script for integrated parser and processor

echo "Building integrated ITCH parser and order book processor..."

# Create build directory
mkdir -p build
cd build

# Create a CMakeLists.txt
cat > CMakeLists.txt << 'EOL'
cmake_minimum_required(VERSION 3.15)
project(nasdaq_integrated_processor LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Handle JSON dependency - use a header-only approach like in cpp_parser
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
    DOWNLOAD_NO_EXTRACT TRUE
    DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/include/nlohmann
)
FetchContent_MakeAvailable(json)

# Find other dependencies
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../cpp_parser/include)
include_directories(${CMAKE_BINARY_DIR}/include)

# Source files
set(SOURCES
    ../integrated_main.cpp
    ../../cpp_order_book/order_book.cpp
    ../../cpp_order_book/trading_strategy.cpp
    ../../cpp_parser/src/parser.cpp
    ../../cpp_parser/src/enums.cpp
    ../../cpp_parser/src/json_serializer.cpp
)

# Add executable
add_executable(integrated_processor ${SOURCES})

# Link libraries
target_link_libraries(integrated_processor PRIVATE 
    Threads::Threads
    ZLIB::ZLIB
)

# Install targets
install(TARGETS integrated_processor DESTINATION bin)
EOL

# Detect architecture (default: native, override with BUILD_ARCH=x86_64)
if [ "$BUILD_ARCH" == "x86_64" ]; then
    export CFLAGS="-arch x86_64"
    export CXXFLAGS="-arch x86_64"
    export LDFLAGS="-arch x86_64"
    cmake -DCMAKE_OSX_ARCHITECTURES=x86_64 .
else
    cmake .
fi

# Build
make -j4

# Copy executable to main directory
if [ -f integrated_processor ]; then
    cp integrated_processor ..
    echo "Integrated processor built successfully!"
    echo "Run with: ./integrated_processor ../itch_raw_file/01302019.NASDAQ_ITCH50 1000 trading_output_integrated 2 2 1"
    echo "This will process 1000 messages with debug mode enabled, using 2 threads for parsing and 2 for processing."
    echo "For a larger test: ./integrated_processor ../itch_raw_file/01302019.NASDAQ_ITCH50 250000 trading_output_integrated 4 4 0"
    echo "This will process 250,000 messages with debug mode disabled, using 4 threads for parsing and 4 for processing."
else
    echo "Build failed. Please check the error messages above."
fi
