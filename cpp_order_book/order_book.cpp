#include "order_book.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cstring> // for strncpy
#include <charconv> // for fast string to number conversion

using json = nlohmann::json;

// String to double conversion with error handling
inline double fast_stod(const std::string& str) {
    try {
        size_t pos = 0;
        double result = std::stod(str, &pos);
        // Check if the entire string was processed
        if (pos == str.size()) {
            return result;
        }
    } catch (const std::exception& e) {
        // Fall through to return 0.0
    }
    return 0.0;
}

namespace hft {

OrderBook::OrderBook() {}
OrderBook::~OrderBook() {}

// Thread-local JSON object to avoid reallocation
thread_local json j;

void OrderBook::process_message(const std::string& message_json) {
    try {
        // Parse JSON message - reuse the thread-local json object
        j = json::parse(message_json);
        
        // Check message type based on your ITCH format
        if (j.contains("body")) {
            const auto& body = j["body"];
            
            // Process different message types
            if (body.contains("AddOrder")) {
                const auto& add_order = body["AddOrder"];
                
                // Check if all required fields exist
                if (!add_order.contains("stock") || !add_order.contains("reference") || 
                    !add_order.contains("price") || !add_order.contains("shares") || 
                    !add_order.contains("side") || !j.contains("timestamp")) {
                    std::cerr << "AddOrder message missing required fields, skipping" << std::endl;
                    return;
                }
                
                // Get stock and trim whitespace in-place
                std::string stock = add_order["stock"].get<std::string>();
                const auto first = stock.find_first_not_of(" \t\n\r\f\v");
                const auto last = stock.find_last_not_of(" \t\n\r\f\v");
                if (first != std::string::npos && last != std::string::npos) {
                    stock = stock.substr(first, last - first + 1);
                } else {
                    stock.clear();
                }
                
                // Fast price conversion
                double price = fast_stod(add_order["price"].get<std::string>());
                
                Order order{
                    stock,
                    add_order["reference"].get<uint64_t>(),
                    price,
                    add_order["shares"].get<uint32_t>(),
                    add_order["side"].get<std::string>(),
                    j["timestamp"].get<uint64_t>()
                };
                process_add_order(order);
            }
            else if (body.contains("DeleteOrder")) {
                const auto& delete_order = body["DeleteOrder"];
                if (!delete_order.contains("reference")) {
                    std::cerr << "DeleteOrder message missing reference field, skipping" << std::endl;
                    return;
                }
                process_delete_order(delete_order["reference"].get<uint64_t>());
            }
            else if (body.contains("OrderExecuted")) {
                const auto& executed = body["OrderExecuted"];
                if (!executed.contains("reference") || !executed.contains("executed")) {
                    std::cerr << "OrderExecuted message missing required fields, skipping" << std::endl;
                    return;
                }
                process_execute_order(executed["reference"].get<uint64_t>(), executed["executed"].get<uint32_t>());
            }
            else if (body.contains("OrderCancelled")) {
                const auto& cancelled = body["OrderCancelled"];
                if (!cancelled.contains("reference") || !cancelled.contains("cancelled")) {
                    std::cerr << "OrderCancelled message missing required fields, skipping" << std::endl;
                    return;
                }
                process_cancel_order(cancelled["reference"].get<uint64_t>(), cancelled["cancelled"].get<uint32_t>());
            }
            else if (body.contains("ReplaceOrder")) {
                const auto& replace = body["ReplaceOrder"];
                if (!replace.contains("original_reference") || !replace.contains("new_reference") || 
                    !replace.contains("price") || !replace.contains("shares")) {
                    // Skip silently
                    return;
                }
                
                // Get price as string and convert to double
                std::string price_str = replace["price"].get<std::string>();
                double price = 0.0;
                try {
                    price = std::stod(price_str);
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing price: " << price_str << ": " << e.what() << std::endl;
                    return;
                }
                
                process_replace_order(
                    replace["original_reference"].get<uint64_t>(), 
                    replace["new_reference"].get<uint64_t>(),
                    price,
                    replace["shares"].get<uint32_t>()
                );
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing message: " << e.what() << std::endl;
    }
}

void OrderBook::process_add_order(const Order& order) {
    // Store the order
    orders_.emplace(order.reference, order);
    
    // Update the order book
    auto& book = books_[order.stock];
    
    // Use pointer to the appropriate price level map
    PriceLevel* price_level = (order.side[0] == 'B') ? &book.first : &book.second;
    (*price_level)[order.price] += order.shares;
    
    // Mark volumes as dirty
    volumes_cache_.erase(order.stock);
    
    // Update best prices
    update_best_prices(order.stock);
}

void OrderBook::process_execute_order(uint64_t reference, uint32_t shares) {
    auto order_it = orders_.find(reference);
    if (order_it == orders_.end()) {
        return;  // Order not found
    }
    
    Order& order = order_it->second;
    auto book_it = books_.find(order.stock);
    if (book_it == books_.end()) {
        return;  // No book for this stock
    }
    
    // Calculate the actual number of shares to execute
    uint32_t shares_to_execute = std::min(shares, order.shares);
    
    // Get the appropriate price level
    auto& price_levels = (order.side[0] == 'B') ? book_it->second.first : book_it->second.second;
    auto price_it = price_levels.find(order.price);
    
    if (price_it != price_levels.end()) {
        // Update the price level
        if (price_it->second > shares_to_execute) {
            price_it->second -= shares_to_execute;
        } else {
            price_levels.erase(price_it);
        }
    }
    
    // Mark volumes as dirty
    volumes_cache_.erase(order.stock);
    
    // Reduce shares in the order
    order.shares -= std::min(shares, order.shares);
    if (order.shares == 0) {
        orders_.erase(reference);
    }
    
    // Update best prices
    update_best_prices(order.stock);
}

void OrderBook::process_delete_order(uint64_t reference) {
    auto order_it = orders_.find(reference);
    if (order_it == orders_.end()) {
        return;  // Order not found
    }
    
    const Order& order = order_it->second;
    auto book_it = books_.find(order.stock);
    if (book_it == books_.end()) {
        orders_.erase(order_it);
        return;
    }
    
    // Get the appropriate price level
    auto& price_levels = (order.side[0] == 'B') ? book_it->second.first : book_it->second.second;
    auto price_it = price_levels.find(order.price);
    
    if (price_it != price_levels.end()) {
        // Update the price level
        if (price_it->second > order.shares) {
            price_it->second -= order.shares;
        } else {
            price_levels.erase(price_it);
        }
    }
    
    // Mark volumes as dirty
    volumes_cache_.erase(order.stock);
    
    // Remove the order
    orders_.erase(order_it);
    
    // Update best prices
    update_best_prices(order.stock);
}

void OrderBook::process_cancel_order(uint64_t reference, uint32_t shares) {
    if (orders_.find(reference) == orders_.end()) {
        return;  // Order not found
    }
    
    Order& order = orders_[reference];
    const std::string& stock = order.stock;
    double price = order.price;
    
    // Reduce shares in the order book
    if (order.side == "Buy") {
        books_[stock].first[price] -= std::min(shares, order.shares);
        if (books_[stock].first[price] == 0) {
            books_[stock].first.erase(price);
        }
    } else {
        books_[stock].second[price] -= std::min(shares, order.shares);
        if (books_[stock].second[price] == 0) {
            books_[stock].second.erase(price);
        }
    }
    
    // Reduce shares in the order
    order.shares -= std::min(shares, order.shares);
    if (order.shares == 0) {
        orders_.erase(reference);
    }
    
    // Update best prices
    update_best_prices(stock);
}

void OrderBook::process_replace_order(uint64_t old_reference, uint64_t new_reference,
                                     double price, uint32_t shares) {
    if (orders_.find(old_reference) == orders_.end()) {
        return;  // Original order not found
    }
    
    // Get original order details
    Order old_order = orders_[old_reference];
    const std::string& stock = old_order.stock;
    
    // First remove the old order
    process_delete_order(old_reference);
    
    // Then add the new order
    Order new_order{
        stock,
        new_reference,
        price,
        shares,
        old_order.side,
        old_order.timestamp  // Keep the same timestamp
    };
    process_add_order(new_order);
}

void OrderBook::update_best_prices(std::string_view stock) {
    auto it = books_.find(std::string(stock));
    if (it == books_.end()) {
        best_prices_.erase(std::string(stock));
        return;
    }
    
    const auto& bids = it->second.first;
    const auto& asks = it->second.second;
    
    double best_bid = bids.empty() ? 0.0 : bids.rbegin()->first;
    double best_ask = asks.empty() ? 0.0 : asks.begin()->first;
    
    best_prices_[std::string(stock)] = {best_bid, best_ask};
}

void OrderBook::update_volumes_cache(std::string_view stock) const {
    auto it = books_.find(std::string(stock));
    if (it == books_.end()) {
        volumes_cache_.erase(std::string(stock));
        return;
    }
    
    const auto& bids = it->second.first;
    const auto& asks = it->second.second;
    
    uint32_t bid_volume = 0;
    uint32_t ask_volume = 0;
    
    for (const auto& [price, qty] : bids) {
        bid_volume += qty;
    }
    
    for (const auto& [price, qty] : asks) {
        ask_volume += qty;
    }
    
    volumes_cache_[std::string(stock)] = {bid_volume, ask_volume};
}

std::pair<uint32_t, uint32_t> OrderBook::get_volumes(std::string_view stock) const {
    auto it = volumes_cache_.find(std::string(stock));
    if (it == volumes_cache_.end()) {
        update_volumes_cache(stock);
        it = volumes_cache_.find(std::string(stock));
    }
    return it != volumes_cache_.end() ? it->second : std::make_pair(0u, 0u);
}

double OrderBook::get_imbalance(std::string_view stock) const {
    auto [bid_volume, ask_volume] = get_volumes(stock);
    if (bid_volume + ask_volume == 0) return 0.0;
    return static_cast<double>(bid_volume) / (bid_volume + ask_volume);
}

std::string OrderBook::get_order_book_snapshot(std::string_view stock) const {
    std::string result = "Order Book Snapshot for ";
    result += stock;
    result += "\nBids (price x size):\n";
    
    auto it = books_.find(std::string(stock));
    if (it == books_.end()) {
        return result + "No orders for this stock\n";
    }
    
    // Pre-allocate string buffer to avoid reallocations
    std::string buffer;
    buffer.reserve(128);
    
    // Get bids in descending order (highest price first)
    for (auto rit = it->second.first.rbegin(); rit != it->second.first.rend(); ++rit) {
        buffer.clear();
        buffer = std::to_string(rit->first) + " x " + std::to_string(rit->second) + "\n";
        result += buffer;
    }
    
    result += "---\nAsks (price x size):\n";
    
    // Get asks in ascending order (lowest price first)
    for (const auto& [price, shares] : it->second.second) {
        buffer.clear();
        buffer = std::to_string(price) + " x " + std::to_string(shares) + "\n";
        result += buffer;
    }
    
    // Add best prices and imbalance
    auto [best_bid, best_ask] = get_best_prices(stock);
    auto [bid_vol, ask_vol] = get_volumes(stock);
    double imbalance = get_imbalance(stock);
    
    result += "---\nSummary:\n";
    result += "Best Bid: " + std::to_string(best_bid) + " x " + std::to_string(bid_vol) + "\n";
    result += "Best Ask: " + std::to_string(best_ask) + " x " + std::to_string(ask_vol) + "\n";
    result += "Imbalance: " + std::to_string(imbalance * 100.0) + "%\n";
    
    return result;
}

std::string OrderBook::get_order_book_json(std::string_view stock) const {
    auto it = books_.find(std::string(stock));
    if (it == books_.end()) {
        return "{}";  // Stock not found
    }
    
    json snapshot;
    
    // Add bids to snapshot
    json bids = json::array();
    for (auto rit = it->second.first.rbegin(); rit != it->second.first.rend(); ++rit) {
        bids.push_back({
            {"price", rit->first},
            {"volume", rit->second},
            {"side", "bid"}
        });
    }
    snapshot["bids"] = bids;
    
    // Add asks to snapshot
    json asks = json::array();
    for (const auto& [price, volume] : it->second.second) {
        asks.push_back({
            {"price", price},
            {"volume", volume},
            {"side", "ask"}
        });
    }
    snapshot["asks"] = asks;
    
    // Add summary information
    auto [best_bid, best_ask] = get_best_prices(stock);
    auto [bid_vol, ask_vol] = get_volumes(stock);
    double imbalance = get_imbalance(stock);
    
    snapshot["summary"] = {
        {"best_bid", best_bid},
        {"best_ask", best_ask},
        {"bid_volume", bid_vol},
        {"ask_volume", ask_vol},
        {"imbalance", imbalance}
    };
    
    return snapshot.dump();
}

std::pair<double, double> OrderBook::get_best_prices(std::string_view stock) const {
    auto it = best_prices_.find(std::string(stock));
    return it != best_prices_.end() ? it->second : std::make_pair(0.0, 0.0);
}

// get_imbalance is already implemented with string_view

}  // namespace hft
