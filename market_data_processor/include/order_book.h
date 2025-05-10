#pragma once

#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <memory>

namespace market_data {

// Represents a single order in the order book
struct Order {
    uint64_t reference;      // Order reference number
    std::string symbol;      // Stock symbol
    bool isBuy;              // true for buy, false for sell
    uint32_t shares;         // Number of shares
    double price;            // Order price
    uint64_t timestamp;      // Timestamp when order was added
};

// Represents a price level in the order book
struct PriceLevel {
    double price;                    // Price level
    uint32_t totalVolume;            // Total volume at this price level
    std::vector<uint64_t> orderRefs; // Order references at this price level
};

/**
 * OrderBook - Maintains the state of the limit order book for a single symbol
 * 
 * This class tracks all orders for a specific symbol and provides methods
 * to query the current state of the market.
 */
class OrderBook {
public:
    explicit OrderBook(const std::string& symbol);
    ~OrderBook();
    
    // Order book operations
    void addOrder(uint64_t reference, bool isBuy, uint32_t shares, double price, uint64_t timestamp);
    void deleteOrder(uint64_t reference);
    void executeOrder(uint64_t reference, uint32_t shares);
    void cancelOrder(uint64_t reference, uint32_t shares);
    void replaceOrder(uint64_t oldReference, uint64_t newReference, uint32_t shares, double price);
    
    // Order book queries
    double getBestBid() const;
    double getBestAsk() const;
    double getMidPrice() const;
    double getSpread() const;
    uint32_t getDepth(bool isBuy) const;
    uint32_t getVolumeAtPrice(bool isBuy, double price) const;
    uint32_t getTotalVolume(bool isBuy) const;
    
    // Get price levels for display
    std::vector<PriceLevel> getBidLevels(size_t depth = 5) const;
    std::vector<PriceLevel> getAskLevels(size_t depth = 5) const;
    
    // Get symbol
    const std::string& getSymbol() const;
    
    // Get order by reference
    const Order* getOrder(uint64_t reference) const;
    
    // Clear the order book
    void clear();

private:
    std::string symbol;
    
    // Price-ordered maps for bids and asks
    // For bids, we use std::greater to sort in descending order
    std::map<double, PriceLevel, std::greater<double>> bids;
    std::map<double, PriceLevel> asks;
    
    // Hash map for quick order lookup by reference
    std::unordered_map<uint64_t, Order> orders;
    
    // Helper methods
    void removeOrderFromPriceLevel(uint64_t reference, double price, bool isBuy);
    void addOrderToPriceLevel(uint64_t reference, double price, uint32_t shares, bool isBuy);
};

} // namespace market_data
