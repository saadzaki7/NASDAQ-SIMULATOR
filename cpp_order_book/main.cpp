#include "order_book.h"
#include "trading_strategy.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <set>

using json = nlohmann::json;
using namespace hft;

void write_market_data(const std::string& symbol, 
                       const std::pair<double, double>& best_prices, 
                       const std::pair<uint32_t, uint32_t>& volumes,
                       double imbalance,
                       uint64_t timestamp, 
                       std::ofstream& output_file) {
    // Create market data entry
    json market_data;
    market_data["symbol"] = symbol;
    market_data["bid_price"] = best_prices.first;
    market_data["ask_price"] = best_prices.second;
    market_data["bid_volume"] = volumes.first;
    market_data["ask_volume"] = volumes.second;
    market_data["imbalance"] = imbalance;
    market_data["timestamp"] = timestamp;
    market_data["mid_price"] = (best_prices.first + best_prices.second) / 2.0;
    market_data["spread"] = best_prices.second - best_prices.first;
    
    // Write to output file
    output_file << market_data.dump() << std::endl;
}

std::vector<json> load_json_data(const std::string& filename, size_t max_messages = 0) {
    std::vector<json> messages;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return messages;
    }
    
    // Check if the file starts with a '[' character
    char first_char = file.peek();
    bool is_json_array = (first_char == '[');
    
    if (is_json_array) {
        // Skip the opening bracket
        file.get();
    }
    
    std::string line;
    size_t count = 0;
    size_t line_number = 0;
    
    while (std::getline(file, line) && (max_messages == 0 || count < max_messages)) {
        line_number++;
        try {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));
            line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);
            
            // Skip empty lines
            if (line.empty()) {
                continue;
            }
            
            // Remove trailing comma if present
            if (line.back() == ',') {
                line.pop_back();
            }
            
            // Skip if this is the closing bracket of the JSON array
            if (line == "]" || line == "}]") {
                continue;
            }
            
            // Parse the JSON
            json j = json::parse(line);
            
            // Extract the message body
            if (j.contains("body")) {
                messages.push_back(j);
                count++;
                
                // Show progress
                if (count % 10000 == 0) {
                    std::cout << "Loaded " << count << " messages..." << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line " << line_number << " (skipping): " << e.what() << std::endl;
            // Print a part of the problematic line for debugging
            if (line.length() > 100) {
                std::cerr << "Line starts with: " << line.substr(0, 100) << "..." << std::endl;
            } else {
                std::cerr << "Line: " << line << std::endl;
            }
        }
    }
    
    std::cout << "Loaded " << messages.size() << " messages from " << filename << std::endl;
    return messages;
}

std::vector<json> filter_messages_by_stocks(const std::vector<json>& messages, 
                                           const std::vector<std::string>& stocks) {
    if (stocks.empty()) {
        return messages;  // No filtering needed
    }
    
    std::vector<json> filtered;
    
    for (const auto& message : messages) {
        try {
            if (message.contains("body")) {
                const auto& body = message["body"];
                
                // Check for AddOrder and filter by stock
                if (body.contains("AddOrder") && body["AddOrder"].contains("stock")) {
                    std::string stock = body["AddOrder"]["stock"];
                    if (std::find(stocks.begin(), stocks.end(), stock) != stocks.end()) {
                        filtered.push_back(message);
                    }
                }
                // Include other message types for matched stocks
                else if ((body.contains("DeleteOrder") || 
                         body.contains("OrderExecuted") || 
                         body.contains("OrderCancelled") || 
                         body.contains("ReplaceOrder")) && 
                         body.contains("reference")) {
                    filtered.push_back(message);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error filtering message: " << e.what() << std::endl;
        }
    }
    
    std::cout << "Filtered to " << filtered.size() << " messages for specified stocks" << std::endl;
    return filtered;
}



int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file> [num_messages] [output_file] [trading_output_dir] [stocks...]" << std::endl;
        return 1;
    }
    
    // Parse command line arguments
    std::string input_file = argv[1];
    size_t num_messages = (argc > 2) ? std::stoi(argv[2]) : 0;
    std::string output_file = (argc > 3) ? argv[3] : "market_data.jsonl";
    std::string trading_output_dir = (argc > 4) ? argv[4] : "trading_output";
    
    // Collect stock filters
    std::vector<std::string> stocks;
    for (int i = 5; i < argc; i++) {
        stocks.push_back(argv[i]);
    }
    
    // Load messages
    auto messages = load_json_data(input_file, num_messages);
    
    // Filter messages if stocks specified
    if (!stocks.empty()) {
        messages = filter_messages_by_stocks(messages, stocks);
    }
    
    // Create order book
    hft::OrderBook order_book;
    
    // Create trading strategy
    hft::LiquidityReversionStrategy strategy(
        order_book,
        trading_output_dir,  // Output directory for trades and performance
        1000000.0,           // Initial capital
        1.5,                // Liquidity threshold (from the Liquidity Reversion strategy)
        0.67,               // Reverse threshold (from the Liquidity Reversion strategy)
        100,                // Position size (from the Liquidity Reversion strategy)
        20                  // Hold time ticks (from the Liquidity Reversion strategy)
    );
    
    // Open output file
    std::ofstream output_stream(output_file);
    if (!output_stream.is_open()) {
        std::cerr << "Failed to open output file: " << output_file << std::endl;
        return 1;
    }
    
    // Process messages and generate market data
    auto start_time = std::chrono::high_resolution_clock::now();
    std::set<std::string> unique_stocks;
    
    size_t count = 0;
    for (const auto& message : messages) {
        try {
            // Convert to string
            std::string message_json = message.dump();
            
            // Process message in order book
            order_book.process_message(message_json);
            
            // Extract stock from message
            std::string stock;
            if (message.contains("body")) {
                const auto& body = message["body"];
                
                if (body.contains("AddOrder") && body["AddOrder"].contains("stock")) {
                    stock = body["AddOrder"]["stock"];
                    // Trim whitespace from stock symbol
                    stock.erase(0, stock.find_first_not_of(" \t\n\r\f\v"));
                    stock.erase(stock.find_last_not_of(" \t\n\r\f\v") + 1);
                    unique_stocks.insert(stock);
                }
            }
            
            // For each known stock, update market data if this is an important message type
            if (!stock.empty()) {
                // Get market data for this stock
                auto best_prices = order_book.get_best_prices(stock);
                auto volumes = order_book.get_volumes(stock);
                double imbalance = order_book.get_imbalance(stock);
                uint64_t timestamp = 0;
                if (message.contains("timestamp")) {
                    // Handle the timestamp which can be either a string or a number
                    if (message["timestamp"].is_string()) {
                        timestamp = std::stoull(message["timestamp"].get<std::string>());
                    } else {
                        timestamp = message["timestamp"].get<uint64_t>();
                    }
                }
                
                // Write market data to output
                write_market_data(stock, best_prices, volumes, imbalance, timestamp, output_stream);
                
                // Execute trading strategy
                strategy.process_market_update(
                    stock, 
                    best_prices.first,   // bid price
                    best_prices.second,  // ask price
                    volumes.first,       // bid volume
                    volumes.second,      // ask volume
                    imbalance,           // order book imbalance
                    timestamp            // timestamp
                );
            }
            
            // Show progress
            count++;
            if (count % 10000 == 0) {
                auto current_time = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - start_time).count() / 1000.0;
                double rate = count / elapsed;
                
                std::cout << "Processed " << count << "/" << messages.size() 
                          << " messages (" << (rate) << " msgs/sec)" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing message: " << e.what() << std::endl;
        }
    }
    
    // Calculate final performance metrics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count() / 1000.0;
    double rate = count / elapsed;
    
    std::cout << "Processing complete!" << std::endl;
    std::cout << "Processed " << count << " messages in " << elapsed << " seconds" << std::endl;
    std::cout << "Rate: " << rate << " messages per second" << std::endl;
    std::cout << "Unique stocks processed: " << unique_stocks.size() << std::endl;
    
    // Print trading strategy performance
    strategy.print_performance();
    
    // Close output file
    output_stream.close();
    
    return 0;
}
