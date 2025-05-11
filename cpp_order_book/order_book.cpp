#include "order_book.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

using json = nlohmann::json;

namespace hft {

OrderBook::OrderBook() {}
OrderBook::~OrderBook() {}

void OrderBook::process_message(const std::string& message_json) {
    try {
        // Parse JSON message
        json j = json::parse(message_json);
        
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
                
                // Get stock and trim whitespace
                std::string stock = add_order["stock"].get<std::string>();
                stock.erase(0, stock.find_first_not_of(" \t\n\r\f\v"));
                stock.erase(stock.find_last_not_of(" \t\n\r\f\v") + 1);
                
                // Get price as string and convert to double
                std::string price_str = add_order["price"].get<std::string>();
                double price = 0.0;
                try {
                    price = std::stod(price_str);
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing price: " << price_str << ": " << e.what() << std::endl;
                    return;
                }
                
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
                if (!executed.contains("reference") || !executed.contains("executed_shares")) {
                    std::cerr << "OrderExecuted message missing required fields, skipping" << std::endl;
                    return;
                }
                process_execute_order(executed["reference"].get<uint64_t>(), executed["executed_shares"].get<uint32_t>());
            }
            else if (body.contains("OrderCancelled")) {
                const auto& cancelled = body["OrderCancelled"];
                if (!cancelled.contains("reference") || !cancelled.contains("cancelled_shares")) {
                    std::cerr << "OrderCancelled message missing required fields, skipping" << std::endl;
                    return;
                }
                process_cancel_order(cancelled["reference"].get<uint64_t>(), cancelled["cancelled_shares"].get<uint32_t>());
            }
            else if (body.contains("ReplaceOrder")) {
                const auto& replace = body["ReplaceOrder"];
                if (!replace.contains("original_reference") || !replace.contains("new_reference") || 
                    !replace.contains("price") || !replace.contains("shares")) {
                    std::cerr << "ReplaceOrder message missing required fields, skipping" << std::endl;
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
    orders_[order.reference] = order;
    
    // Update the order book
    const std::string& stock = order.stock;
    double price = order.price;
    uint32_t shares = order.shares;
    
    // Determine which side of the book to update
    if (order.side == "Buy") {
        books_[stock].first[price] += shares;
    } else {
        books_[stock].second[price] += shares;
    }
    
    // Update best prices
    update_best_prices(stock);
}

void OrderBook::process_execute_order(uint64_t reference, uint32_t shares) {
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

void OrderBook::process_delete_order(uint64_t reference) {
    if (orders_.find(reference) == orders_.end()) {
        return;  // Order not found
    }
    
    Order& order = orders_[reference];
    const std::string& stock = order.stock;
    double price = order.price;
    uint32_t shares = order.shares;
    
    // Remove shares from the order book
    if (order.side == "Buy") {
        books_[stock].first[price] -= shares;
        if (books_[stock].first[price] == 0) {
            books_[stock].first.erase(price);
        }
    } else {
        books_[stock].second[price] -= shares;
        if (books_[stock].second[price] == 0) {
            books_[stock].second.erase(price);
        }
    }
    
    // Remove the order
    orders_.erase(reference);
    
    // Update best prices
    update_best_prices(stock);
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

void OrderBook::update_best_prices(const std::string& stock) {
    double best_bid = 0.0;
    double best_ask = 0.0;
    
    // Find the best bid (highest price)
    auto& bids = books_[stock].first;
    if (!bids.empty()) {
        best_bid = bids.rbegin()->first;
    }
    
    // Find the best ask (lowest price)
    auto& asks = books_[stock].second;
    if (!asks.empty()) {
        best_ask = asks.begin()->first;
    }
    
    // Update best prices
    best_prices_[stock] = {best_bid, best_ask};
}

std::string OrderBook::get_order_book_snapshot(const std::string& stock) {
    if (books_.find(stock) == books_.end()) {
        return "{}";  // Stock not found
    }
    
    json snapshot;
    
    // Add bids to snapshot
    json bids = json::array();
    for (const auto& [price, volume] : books_[stock].first) {
        bids.push_back({
            {"price", price},
            {"volume", volume},
            {"side", "bid"}
        });
    }
    snapshot["bids"] = bids;
    
    // Add asks to snapshot
    json asks = json::array();
    for (const auto& [price, volume] : books_[stock].second) {
        asks.push_back({
            {"price", price},
            {"volume", volume},
            {"side", "ask"}
        });
    }
    snapshot["asks"] = asks;
    
    return snapshot.dump();
}

std::pair<double, double> OrderBook::get_best_prices(const std::string& stock) {
    if (best_prices_.find(stock) == best_prices_.end()) {
        return {0.0, 0.0};  // Stock not found
    }
    
    return best_prices_[stock];
}

std::pair<uint32_t, uint32_t> OrderBook::get_volumes(const std::string& stock) {
    if (books_.find(stock) == books_.end()) {
        return {0, 0};  // Stock not found
    }
    
    // Calculate total bid volume
    uint32_t bid_volume = 0;
    for (const auto& [price, volume] : books_[stock].first) {
        bid_volume += volume;
    }
    
    // Calculate total ask volume
    uint32_t ask_volume = 0;
    for (const auto& [price, volume] : books_[stock].second) {
        ask_volume += volume;
    }
    
    return {bid_volume, ask_volume};
}

double OrderBook::get_imbalance(const std::string& stock) {
    auto volumes = get_volumes(stock);
    uint32_t bid_volume = volumes.first;
    uint32_t ask_volume = volumes.second;
    
    if (bid_volume + ask_volume == 0) {
        return 0.0;  // No orders, no imbalance
    }
    
    // Calculate imbalance ratio: (bid_volume - ask_volume) / (bid_volume + ask_volume)
    return static_cast<double>(bid_volume - ask_volume) / 
           static_cast<double>(bid_volume + ask_volume);
}

}  // namespace hft
