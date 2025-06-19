
cmake_minimum_required(VERSION 3.15)
project(itch_parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define source files
set(SOURCES
    src/main.cpp
    src/parser.cpp
    src/enums.cpp
    src/json_serializer.cpp
)

# Create the executable
add_executable(itch_parser ${SOURCES})

# Add include directory
target_include_directories(itch_parser PRIVATE include)

# Handle JSON dependency - use a header-only approach
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
    DOWNLOAD_NO_EXTRACT TRUE
    DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/include/nlohmann
)
FetchContent_MakeAvailable(json)
target_include_directories(itch_parser PRIVATE ${CMAKE_BINARY_DIR}/include)

# Find ZLIB for gzip support
find_package(ZLIB REQUIRED)
target_link_libraries(itch_parser PRIVATE ZLIB::ZLIB)

# Install targets
install(TARGETS itch_parser DESTINATION bin)
