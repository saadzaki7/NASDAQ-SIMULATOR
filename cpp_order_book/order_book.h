#pragma once

#include <unordered_map>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>

namespace hft {

struct Order {
    std::string stock;
    uint64_t reference;
    double price;
    uint32_t shares;
    std::string side;  // "Buy" or "Sell"
    uint64_t timestamp;
};

class OrderBook {
public:
    OrderBook();
    ~OrderBook();

    // Process a single message and update the order book
    void process_message(const std::string& message_json);
    
    // Get the current state of the order book for a stock
    std::string get_order_book_snapshot(const std::string& stock);
    
    // Get the best bid and ask for a stock
    std::pair<double, double> get_best_prices(const std::string& stock);
    
    // Get the total volume at the bid and ask for a stock
    std::pair<uint32_t, uint32_t> get_volumes(const std::string& stock);
    
    // Calculate the imbalance for a stock
    double get_imbalance(const std::string& stock);

private:
    // Use efficient data structures for the order book
    // Key: price level, Value: total volume at that level
    using PriceLevel = std::map<double, uint32_t>;
    
    // Store orders by reference ID
    std::unordered_map<uint64_t, Order> orders_;
    
    // Store order books for each stock (bids are stored in reverse order)
    std::unordered_map<std::string, std::pair<PriceLevel, PriceLevel>> books_;  // stock -> (bids, asks)
    
    // Track best bid/ask for fast access
    std::unordered_map<std::string, std::pair<double, double>> best_prices_;  // stock -> (bid, ask)
    
    // Process specific message types
    void process_add_order(const Order& order);
    void process_execute_order(uint64_t reference, uint32_t shares);
    void process_delete_order(uint64_t reference);
    void process_cancel_order(uint64_t reference, uint32_t shares);
    void process_replace_order(uint64_t old_reference, uint64_t new_reference,
                              double price, uint32_t shares);
    
    // Update best prices after order book changes
    void update_best_prices(const std::string& stock);
};

}  // namespace hft
