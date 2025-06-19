cmake_minimum_required(VERSION 3.12)
project(cpp_order_book)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(nlohmann_json 3.9.0 REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Add executable
add_executable(order_book_processor
    main.cpp
    order_book.cpp
    trading_strategy.cpp
)

# Link libraries
target_link_libraries(order_book_processor PRIVATE nlohmann_json::nlohmann_json)

# Installation
install(TARGETS order_book_processor DESTINATION .)