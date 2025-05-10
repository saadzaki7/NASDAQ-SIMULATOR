cmake_minimum_required(VERSION 3.10)
project(market_data_processor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find nlohmann_json package
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
    FetchContent_MakeAvailable(json)
endif()

# Source files
set(SOURCES
    src/market_data_processor.cpp
    src/order_book.cpp
    src/market_stats.cpp
    src/main.cpp
)

# Include directories
include_directories(include)

# Create executable
add_executable(market_data_processor ${SOURCES})

# Link libraries
if(nlohmann_json_FOUND)
    target_link_libraries(market_data_processor PRIVATE nlohmann_json::nlohmann_json)
else()
    target_link_libraries(market_data_processor PRIVATE nlohmann_json::nlohmann_json)
endif()

# Install target
install(TARGETS market_data_processor DESTINATION bin)
