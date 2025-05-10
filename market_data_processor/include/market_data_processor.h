#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

namespace market_data {

// Forward declarations
class OrderBook;
class MarketStats;

/**
 * MarketDataProcessor - Main class for processing ITCH 5.0 market data from JSON files
 * 
 * This component processes JSON-formatted ITCH 5.0 messages and:
 * 1. Maintains order books for each symbol
 * 2. Calculates market statistics
 * 3. Provides interfaces for querying market state
 * 4. Supports simulation of market events
 */
class MarketDataProcessor {
public:
    MarketDataProcessor();
    ~MarketDataProcessor();

    // Load and process market data from a JSON file
    bool loadDataFromFile(const std::string& filePath);
    
    // Process a single JSON message
    bool processMessage(const nlohmann::json& message);
    
    // Process a batch of messages (up to specified count)
    size_t processBatch(size_t messageCount);
    
    // Reset the processor state
    void reset();
    
    // Get statistics about processed data
    size_t getTotalMessagesProcessed() const;
    size_t getMessagesByType(const std::string& type) const;
    
    // Get list of all symbols in the data
    std::vector<std::string> getAllSymbols() const;
    
    // Get order book for a specific symbol
    std::shared_ptr<OrderBook> getOrderBook(const std::string& symbol) const;
    
    // Get market statistics
    std::shared_ptr<MarketStats> getMarketStats() const;
    
    // Register callback for specific message types
    void registerCallback(const std::string& messageType, 
                         std::function<void(const nlohmann::json&)> callback);

private:
    // Internal message processing methods
    void processAddOrder(const nlohmann::json& message);
    void processDeleteOrder(const nlohmann::json& message);
    void processReplaceOrder(const nlohmann::json& message);
    void processOrderExecuted(const nlohmann::json& message);
    void processOrderExecutedWithPrice(const nlohmann::json& message);
    void processOrderCancelled(const nlohmann::json& message);
    void processNonCrossTrade(const nlohmann::json& message);
    void processCrossTrade(const nlohmann::json& message);
    
    // Internal state
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> orderBooks;
    std::shared_ptr<MarketStats> marketStats;
    
    // Message statistics
    size_t totalMessagesProcessed;
    std::unordered_map<std::string, size_t> messageTypeCount;
    
    // Current position in the data file
    size_t currentMessageIndex;
    
    // JSON data
    std::vector<nlohmann::json> messages;
    
    // Callbacks for message types
    std::unordered_map<std::string, std::vector<std::function<void(const nlohmann::json&)>>> callbacks;
    
    // Order reference to symbol mapping
    std::unordered_map<uint64_t, std::string> orderRefToSymbol;
};

} // namespace market_data
