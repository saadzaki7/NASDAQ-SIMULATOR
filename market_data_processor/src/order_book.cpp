#include "../include/order_book.h"
#include <algorithm>
#include <limits>
#include <iostream>

namespace market_data {

OrderBook::OrderBook(const std::string& symbol) 
    : symbol(symbol) {
}

OrderBook::~OrderBook() {
    // Clean up resources
}

void OrderBook::addOrder(uint64_t reference, bool isBuy, uint32_t shares, double price, uint64_t timestamp) {
    // Create the order
    Order order;
    order.reference = reference;
    order.symbol = symbol;
    order.isBuy = isBuy;
    order.shares = shares;
    order.price = price;
    order.timestamp = timestamp;
    
    // Add to orders map
    orders[reference] = order;
    
    // Add to appropriate price level
    addOrderToPriceLevel(reference, price, shares, isBuy);
}

void OrderBook::deleteOrder(uint64_t reference) {
    auto it = orders.find(reference);
    if (it == orders.end()) {
        // Order not found
        return;
    }
    
    // Get order details
    const Order& order = it->second;
    
    // Remove from price level
    removeOrderFromPriceLevel(reference, order.price, order.isBuy);
    
    // Remove from orders map
    orders.erase(reference);
}

void OrderBook::executeOrder(uint64_t reference, uint32_t shares) {
    auto it = orders.find(reference);
    if (it == orders.end()) {
        // Order not found
        return;
    }
    
    Order& order = it->second;
    
    // If executing all shares, just delete the order
    if (shares >= order.shares) {
        deleteOrder(reference);
        return;
    }
    
    // Otherwise, update the order and price level
    double price = order.price;
    bool isBuy = order.isBuy;
    
    // Update price level
    if (isBuy) {
        auto bidIt = bids.find(price);
        if (bidIt != bids.end()) {
            bidIt->second.totalVolume -= shares;
        }
    } else {
        auto askIt = asks.find(price);
        if (askIt != asks.end()) {
            askIt->second.totalVolume -= shares;
        }
    }
    
    // Update order
    order.shares -= shares;
}

void OrderBook::cancelOrder(uint64_t reference, uint32_t shares) {
    // Cancellation is similar to execution
    executeOrder(reference, shares);
}

void OrderBook::replaceOrder(uint64_t oldReference, uint64_t newReference, uint32_t shares, double price) {
    auto it = orders.find(oldReference);
    if (it == orders.end()) {
        // Old order not found
        return;
    }
    
    const Order& oldOrder = it->second;
    bool isBuy = oldOrder.isBuy;
    
    // Delete the old order
    deleteOrder(oldReference);
    
    // Add the new order
    addOrder(newReference, isBuy, shares, price, oldOrder.timestamp);
}

double OrderBook::getBestBid() const {
    if (bids.empty()) {
        return 0.0;
    }
    return bids.begin()->first;
}

double OrderBook::getBestAsk() const {
    if (asks.empty()) {
        return std::numeric_limits<double>::max();
    }
    return asks.begin()->first;
}

double OrderBook::getMidPrice() const {
    double bestBid = getBestBid();
    double bestAsk = getBestAsk();
    
    if (bestBid == 0.0 || bestAsk == std::numeric_limits<double>::max()) {
        return 0.0;
    }
    
    return (bestBid + bestAsk) / 2.0;
}

double OrderBook::getSpread() const {
    double bestBid = getBestBid();
    double bestAsk = getBestAsk();
    
    if (bestBid == 0.0 || bestAsk == std::numeric_limits<double>::max()) {
        return std::numeric_limits<double>::max();
    }
    
    return bestAsk - bestBid;
}

uint32_t OrderBook::getDepth(bool isBuy) const {
    if (isBuy) {
        return bids.size();
    } else {
        return asks.size();
    }
}

uint32_t OrderBook::getVolumeAtPrice(bool isBuy, double price) const {
    if (isBuy) {
        auto it = bids.find(price);
        if (it != bids.end()) {
            return it->second.totalVolume;
        }
    } else {
        auto it = asks.find(price);
        if (it != asks.end()) {
            return it->second.totalVolume;
        }
    }
    return 0;
}

uint32_t OrderBook::getTotalVolume(bool isBuy) const {
    uint32_t totalVolume = 0;
    
    if (isBuy) {
        for (const auto& pair : bids) {
            totalVolume += pair.second.totalVolume;
        }
    } else {
        for (const auto& pair : asks) {
            totalVolume += pair.second.totalVolume;
        }
    }
    
    return totalVolume;
}

std::vector<PriceLevel> OrderBook::getBidLevels(size_t depth) const {
    std::vector<PriceLevel> levels;
    levels.reserve(std::min(depth, bids.size()));
    
    size_t count = 0;
    for (const auto& pair : bids) {
        if (count >= depth) break;
        levels.push_back(pair.second);
        count++;
    }
    
    return levels;
}

std::vector<PriceLevel> OrderBook::getAskLevels(size_t depth) const {
    std::vector<PriceLevel> levels;
    levels.reserve(std::min(depth, asks.size()));
    
    size_t count = 0;
    for (const auto& pair : asks) {
        if (count >= depth) break;
        levels.push_back(pair.second);
        count++;
    }
    
    return levels;
}

const std::string& OrderBook::getSymbol() const {
    return symbol;
}

const Order* OrderBook::getOrder(uint64_t reference) const {
    auto it = orders.find(reference);
    if (it != orders.end()) {
        return &(it->second);
    }
    return nullptr;
}

void OrderBook::clear() {
    bids.clear();
    asks.clear();
    orders.clear();
}

// Private helper methods

void OrderBook::removeOrderFromPriceLevel(uint64_t reference, double price, bool isBuy) {
    if (isBuy) {
        auto bidIt = bids.find(price);
        if (bidIt != bids.end()) {
            PriceLevel& level = bidIt->second;
            
            // Find and remove the order reference
            auto refIt = std::find(level.orderRefs.begin(), level.orderRefs.end(), reference);
            if (refIt != level.orderRefs.end()) {
                // Get the order to subtract its shares from the total volume
                auto orderIt = orders.find(reference);
                if (orderIt != orders.end()) {
                    level.totalVolume -= orderIt->second.shares;
                }
                
                // Remove the reference
                level.orderRefs.erase(refIt);
                
                // If no more orders at this price level, remove the price level
                if (level.orderRefs.empty()) {
                    bids.erase(bidIt);
                }
            }
        }
    } else {
        auto askIt = asks.find(price);
        if (askIt != asks.end()) {
            PriceLevel& level = askIt->second;
            
            // Find and remove the order reference
            auto refIt = std::find(level.orderRefs.begin(), level.orderRefs.end(), reference);
            if (refIt != level.orderRefs.end()) {
                // Get the order to subtract its shares from the total volume
                auto orderIt = orders.find(reference);
                if (orderIt != orders.end()) {
                    level.totalVolume -= orderIt->second.shares;
                }
                
                // Remove the reference
                level.orderRefs.erase(refIt);
                
                // If no more orders at this price level, remove the price level
                if (level.orderRefs.empty()) {
                    asks.erase(askIt);
                }
            }
        }
    }
}

void OrderBook::addOrderToPriceLevel(uint64_t reference, double price, uint32_t shares, bool isBuy) {
    if (isBuy) {
        auto bidIt = bids.find(price);
        if (bidIt == bids.end()) {
            // Create new price level
            PriceLevel level;
            level.price = price;
            level.totalVolume = shares;
            level.orderRefs.push_back(reference);
            bids[price] = level;
        } else {
            // Add to existing price level
            bidIt->second.totalVolume += shares;
            bidIt->second.orderRefs.push_back(reference);
        }
    } else {
        auto askIt = asks.find(price);
        if (askIt == asks.end()) {
            // Create new price level
            PriceLevel level;
            level.price = price;
            level.totalVolume = shares;
            level.orderRefs.push_back(reference);
            asks[price] = level;
        } else {
            // Add to existing price level
            askIt->second.totalVolume += shares;
            askIt->second.orderRefs.push_back(reference);
        }
    }
}

} // namespace market_data
