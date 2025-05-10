#include "../include/market_data_processor.h"
#include "../include/order_book.h"
#include "../include/market_stats.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <string>
#include <cstring>

namespace market_data {

MarketDataProcessor::MarketDataProcessor() 
    : totalMessagesProcessed(0), 
      currentMessageIndex(0),
      marketStats(std::make_shared<MarketStats>()) {
}

MarketDataProcessor::~MarketDataProcessor() {
    // Clean up resources
}

bool MarketDataProcessor::loadDataFromFile(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filePath << std::endl;
            return false;
        }

        // Clear existing data
        reset();

        // Read JSON messages line by line
        std::string line;
        while (std::getline(file, line)) {
            // Skip empty lines
            if (line.empty()) continue;

            // Parse JSON
            try {
                nlohmann::json message = nlohmann::json::parse(line);
                messages.push_back(message);
            } catch (const nlohmann::json::exception& e) {
                std::cerr << "JSON parsing error: " << e.what() << std::endl;
                // Continue with next line
            }
        }

        std::cout << "Loaded " << messages.size() << " messages from file" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading data: " << e.what() << std::endl;
        return false;
    }
}

bool MarketDataProcessor::processMessage(const nlohmann::json& message) {
    try {
        // Increment message count
        totalMessagesProcessed++;

        // Extract message type from tag
        if (!message.contains("tag")) {
            std::cerr << "Message missing 'tag' field" << std::endl;
            return false;
        }

        uint8_t tag = message["tag"];
        std::string messageType;

        // Map tag to message type and update message type count
        switch (tag) {
            case 65: // 'A' - Add Order
                messageType = "AddOrder";
                messageTypeCount["AddOrder"]++;
                processAddOrder(message);
                break;
            case 68: // 'D' - Delete Order
                messageType = "DeleteOrder";
                messageTypeCount["DeleteOrder"]++;
                processDeleteOrder(message);
                break;
            case 85: // 'U' - Replace Order
                messageType = "ReplaceOrder";
                messageTypeCount["ReplaceOrder"]++;
                processReplaceOrder(message);
                break;
            case 69: // 'E' - Order Executed
                messageType = "OrderExecuted";
                messageTypeCount["OrderExecuted"]++;
                processOrderExecuted(message);
                break;
            case 67: // 'C' - Order Executed With Price
                messageType = "OrderExecutedWithPrice";
                messageTypeCount["OrderExecutedWithPrice"]++;
                processOrderExecutedWithPrice(message);
                break;
            case 88: // 'X' - Order Cancelled
                messageType = "OrderCancelled";
                messageTypeCount["OrderCancelled"]++;
                processOrderCancelled(message);
                break;
            case 80: // 'P' - Non-Cross Trade
                messageType = "NonCrossTrade";
                messageTypeCount["NonCrossTrade"]++;
                processNonCrossTrade(message);
                break;
            case 81: // 'Q' - Cross Trade
                messageType = "CrossTrade";
                messageTypeCount["CrossTrade"]++;
                processCrossTrade(message);
                break;
            default:
                // Other message types we're not processing in detail yet
                messageType = "Other";
                messageTypeCount["Other"]++;
                break;
        }

        // Call registered callbacks for this message type
        if (callbacks.find(messageType) != callbacks.end()) {
            for (const auto& callback : callbacks[messageType]) {
                callback(message);
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error processing message: " << e.what() << std::endl;
        return false;
    }
}

size_t MarketDataProcessor::processBatch(size_t messageCount) {
    size_t processedCount = 0;
    
    while (processedCount < messageCount && currentMessageIndex < messages.size()) {
        if (processMessage(messages[currentMessageIndex])) {
            processedCount++;
        }
        currentMessageIndex++;
    }
    
    return processedCount;
}

void MarketDataProcessor::reset() {
    // Clear all data structures
    orderBooks.clear();
    messages.clear();
    totalMessagesProcessed = 0;
    messageTypeCount.clear();
    currentMessageIndex = 0;
    orderRefToSymbol.clear();
    
    // Reset market stats
    marketStats->reset();
}

size_t MarketDataProcessor::getTotalMessagesProcessed() const {
    return totalMessagesProcessed;
}

size_t MarketDataProcessor::getMessagesByType(const std::string& type) const {
    auto it = messageTypeCount.find(type);
    if (it != messageTypeCount.end()) {
        return it->second;
    }
    return 0;
}

std::vector<std::string> MarketDataProcessor::getAllSymbols() const {
    std::vector<std::string> symbols;
    symbols.reserve(orderBooks.size());
    
    for (const auto& pair : orderBooks) {
        symbols.push_back(pair.first);
    }
    
    return symbols;
}

std::shared_ptr<OrderBook> MarketDataProcessor::getOrderBook(const std::string& symbol) const {
    auto it = orderBooks.find(symbol);
    if (it != orderBooks.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<MarketStats> MarketDataProcessor::getMarketStats() const {
    return marketStats;
}

void MarketDataProcessor::registerCallback(const std::string& messageType, 
                                          std::function<void(const nlohmann::json&)> callback) {
    callbacks[messageType].push_back(callback);
}

// Private message processing methods

void MarketDataProcessor::processAddOrder(const nlohmann::json& message) {
    try {
        if (!message.contains("body") || !message["body"].contains("AddOrder")) {
            return;
        }

        const auto& addOrder = message["body"]["AddOrder"];
        
        // Extract order details
        uint64_t reference = addOrder["reference"];
        std::string stock = addOrder["stock"];
        // Trim whitespace from stock symbol
        stock.erase(std::remove_if(stock.begin(), stock.end(), ::isspace), stock.end());
        
        bool isBuy = (addOrder["side"] == "Buy");
        uint32_t shares = addOrder["shares"];
        double price = std::stod(addOrder["price"].get<std::string>());
        uint64_t timestamp = message["timestamp"];
        
        // Store reference to symbol mapping
        orderRefToSymbol[reference] = stock;
        
        // Get or create order book for this symbol
        if (orderBooks.find(stock) == orderBooks.end()) {
            orderBooks[stock] = std::make_shared<OrderBook>(stock);
        }
        
        // Add order to the book
        orderBooks[stock]->addOrder(reference, isBuy, shares, price, timestamp);
        
        // Update market stats
        marketStats->updateWithOrder(stock, price, shares, isBuy, timestamp);
        
    } catch (const std::exception& e) {
        std::cerr << "Error processing AddOrder: " << e.what() << std::endl;
    }
}

void MarketDataProcessor::processDeleteOrder(const nlohmann::json& message) {
    try {
        if (!message.contains("body") || !message["body"].contains("DeleteOrder")) {
            return;
        }

        const auto& deleteOrder = message["body"]["DeleteOrder"];
        uint64_t reference = deleteOrder["reference"];
        
        // Find the symbol for this order reference
        auto symbolIt = orderRefToSymbol.find(reference);
        if (symbolIt == orderRefToSymbol.end()) {
            // We don't know which symbol this order belongs to
            return;
        }
        
        const std::string& symbol = symbolIt->second;
        auto orderBookIt = orderBooks.find(symbol);
        if (orderBookIt == orderBooks.end()) {
            return;
        }
        
        // Get the order details before deleting
        const Order* order = orderBookIt->second->getOrder(reference);
        if (order) {
            // Update market stats before deleting the order
            marketStats->updateWithCancel(symbol, order->price, order->shares, order->isBuy, message["timestamp"]);
        }
        
        // Delete the order
        orderBookIt->second->deleteOrder(reference);
        
        // Remove from reference map
        orderRefToSymbol.erase(reference);
        
    } catch (const std::exception& e) {
        std::cerr << "Error processing DeleteOrder: " << e.what() << std::endl;
    }
}

void MarketDataProcessor::processReplaceOrder(const nlohmann::json& message) {
    try {
        if (!message.contains("body") || !message["body"].contains("ReplaceOrder")) {
            return;
        }

        const auto& replaceOrder = message["body"]["ReplaceOrder"];
        uint64_t oldReference = replaceOrder["old_reference"];
        uint64_t newReference = replaceOrder["new_reference"];
        uint32_t shares = replaceOrder["shares"];
        double price = std::stod(replaceOrder["price"].get<std::string>());
        
        // Find the symbol for this order reference
        auto symbolIt = orderRefToSymbol.find(oldReference);
        if (symbolIt == orderRefToSymbol.end()) {
            return;
        }
        
        const std::string& symbol = symbolIt->second;
        auto orderBookIt = orderBooks.find(symbol);
        if (orderBookIt == orderBooks.end()) {
            return;
        }
        
        // Get the old order details before replacing
        const Order* oldOrder = orderBookIt->second->getOrder(oldReference);
        if (oldOrder) {
            // Update market stats for the cancellation part
            marketStats->updateWithCancel(symbol, oldOrder->price, oldOrder->shares, oldOrder->isBuy, message["timestamp"]);
            
            // Update market stats for the new order part
            marketStats->updateWithOrder(symbol, price, shares, oldOrder->isBuy, message["timestamp"]);
        }
        
        // Replace the order
        orderBookIt->second->replaceOrder(oldReference, newReference, shares, price);
        
        // Update reference map
        orderRefToSymbol.erase(oldReference);
        orderRefToSymbol[newReference] = symbol;
        
    } catch (const std::exception& e) {
        std::cerr << "Error processing ReplaceOrder: " << e.what() << std::endl;
    }
}

void MarketDataProcessor::processOrderExecuted(const nlohmann::json& message) {
    try {
        if (!message.contains("body") || !message["body"].contains("OrderExecuted")) {
            return;
        }

        const auto& orderExecuted = message["body"]["OrderExecuted"];
        uint64_t reference = orderExecuted["reference"];
        uint32_t executed = orderExecuted["executed"];
        
        // Find the symbol for this order reference
        auto symbolIt = orderRefToSymbol.find(reference);
        if (symbolIt == orderRefToSymbol.end()) {
            return;
        }
        
        const std::string& symbol = symbolIt->second;
        auto orderBookIt = orderBooks.find(symbol);
        if (orderBookIt == orderBooks.end()) {
            return;
        }
        
        // Get the order details before execution
        const Order* order = orderBookIt->second->getOrder(reference);
        if (order) {
            // Update market stats with the trade
            marketStats->updateWithTrade(symbol, order->price, executed, message["timestamp"], !order->isBuy);
        }
        
        // Execute the order
        orderBookIt->second->executeOrder(reference, executed);
        
    } catch (const std::exception& e) {
        std::cerr << "Error processing OrderExecuted: " << e.what() << std::endl;
    }
}

void MarketDataProcessor::processOrderExecutedWithPrice(const nlohmann::json& message) {
    try {
        if (!message.contains("body") || !message["body"].contains("OrderExecutedWithPrice")) {
            return;
        }

        const auto& orderExecuted = message["body"]["OrderExecutedWithPrice"];
        uint64_t reference = orderExecuted["reference"];
        uint32_t executed = orderExecuted["executed"];
        double price = std::stod(orderExecuted["price"].get<std::string>());
        
        // Find the symbol for this order reference
        auto symbolIt = orderRefToSymbol.find(reference);
        if (symbolIt == orderRefToSymbol.end()) {
            return;
        }
        
        const std::string& symbol = symbolIt->second;
        auto orderBookIt = orderBooks.find(symbol);
        if (orderBookIt == orderBooks.end()) {
            return;
        }
        
        // Get the order details before execution
        const Order* order = orderBookIt->second->getOrder(reference);
        if (order) {
            // Update market stats with the trade (using the execution price, not the order price)
            marketStats->updateWithTrade(symbol, price, executed, message["timestamp"], !order->isBuy);
        }
        
        // Execute the order
        orderBookIt->second->executeOrder(reference, executed);
        
    } catch (const std::exception& e) {
        std::cerr << "Error processing OrderExecutedWithPrice: " << e.what() << std::endl;
    }
}

void MarketDataProcessor::processOrderCancelled(const nlohmann::json& message) {
    try {
        if (!message.contains("body") || !message["body"].contains("OrderCancelled")) {
            return;
        }

        const auto& orderCancelled = message["body"]["OrderCancelled"];
        uint64_t reference = orderCancelled["reference"];
        uint32_t cancelled = orderCancelled["cancelled"];
        
        // Find the symbol for this order reference
        auto symbolIt = orderRefToSymbol.find(reference);
        if (symbolIt == orderRefToSymbol.end()) {
            return;
        }
        
        const std::string& symbol = symbolIt->second;
        auto orderBookIt = orderBooks.find(symbol);
        if (orderBookIt == orderBooks.end()) {
            return;
        }
        
        // Get the order details before cancellation
        const Order* order = orderBookIt->second->getOrder(reference);
        if (order) {
            // Update market stats
            marketStats->updateWithCancel(symbol, order->price, cancelled, order->isBuy, message["timestamp"]);
        }
        
        // Cancel the order
        orderBookIt->second->cancelOrder(reference, cancelled);
        
    } catch (const std::exception& e) {
        std::cerr << "Error processing OrderCancelled: " << e.what() << std::endl;
    }
}

void MarketDataProcessor::processNonCrossTrade(const nlohmann::json& message) {
    try {
        if (!message.contains("body") || !message["body"].contains("NonCrossTrade")) {
            return;
        }

        const auto& trade = message["body"]["NonCrossTrade"];
        std::string stock = trade["stock"];
        // Trim whitespace from stock symbol
        stock.erase(std::remove_if(stock.begin(), stock.end(), ::isspace), stock.end());
        
        uint32_t shares = trade["shares"];
        double price = std::stod(trade["price"].get<std::string>());
        bool isBuy = (trade["side"] == "Buy");
        
        // Update market stats with the trade
        marketStats->updateWithTrade(stock, price, shares, message["timestamp"], isBuy);
        
    } catch (const std::exception& e) {
        std::cerr << "Error processing NonCrossTrade: " << e.what() << std::endl;
    }
}

void MarketDataProcessor::processCrossTrade(const nlohmann::json& message) {
    try {
        if (!message.contains("body") || !message["body"].contains("CrossTrade")) {
            return;
        }

        const auto& trade = message["body"]["CrossTrade"];
        std::string stock = trade["stock"];
        // Trim whitespace from stock symbol
        stock.erase(std::remove_if(stock.begin(), stock.end(), ::isspace), stock.end());
        
        uint32_t shares = trade["shares"];
        double price = std::stod(trade["cross_price"].get<std::string>());
        
        // For cross trades, we don't have a side, so we'll use a neutral side
        // (false for isBuySideAggressor)
        marketStats->updateWithTrade(stock, price, shares, message["timestamp"], false);
        
    } catch (const std::exception& e) {
        std::cerr << "Error processing CrossTrade: " << e.what() << std::endl;
    }
}

} // namespace market_data
