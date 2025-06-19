#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <deque>
#include <fstream>
#include "order_book.h"
#include <nlohmann/json.hpp>
#include <mutex>

using json = nlohmann::json;

namespace hft {

struct Position {
    std::string symbol;
    int quantity;
    double entry_price;
    uint64_t entry_time;
    double pnl;
};

struct Trade {
    std::string symbol;
    std::string side;
    int quantity;
    double price;
    uint64_t timestamp;
    double pnl;
};

class LiquidityReversionStrategy {
public:
    LiquidityReversionStrategy(
        OrderBook& order_book,
        const std::string& output_dir,
        double initial_capital = 1000000.0,
        double liquidity_threshold = 1.5,
        double reverse_threshold = 0.67,
        int position_size = 100,
        int hold_time_ticks = 20
    );
    
    ~LiquidityReversionStrategy();
    
    // Process market update and execute trading strategy
    void process_market_update(const std::string& symbol, 
                              double bid_price, double ask_price,
                              uint32_t bid_volume, uint32_t ask_volume,
                              double imbalance, uint64_t timestamp);
    
    // Run strategy on all updates in the order book
    void run();
    
    // Print performance metrics
    void print_performance();

private:
    OrderBook& order_book_;
    std::string output_dir_;
    double initial_capital_;
    double current_capital_;
    
    // Strategy parameters
    double liquidity_threshold_;
    double reverse_threshold_;
    int position_size_;
    int hold_time_ticks_;
    
    // Trading state
    std::unordered_map<std::string, Position> positions_;
    std::unordered_map<std::string, int> position_hold_time_;
    std::vector<Trade> trades_;
    std::unordered_map<std::string, std::deque<Position>> position_history_;
    std::mutex trades_mutex_;
    std::ofstream trades_file_;
    std::vector<Trade> pending_trades_;
    std::mutex pending_trades_mutex_;
    
    // Timing
    std::chrono::time_point<std::chrono::system_clock> start_time_;
    
    // Market data cache
    std::unordered_map<std::string, std::deque<double>> price_history_;
    std::vector<std::string> trade_buffer_;  // Buffer for batching trade writes
    
    static constexpr size_t BATCH_SIZE = 1000;  // Write to disk every 1000 trades
    
    // Execute buy/sell orders
    void execute_buy(const std::string& symbol, double price, int quantity, uint64_t timestamp);
    void execute_sell(const std::string& symbol, double price, int quantity, uint64_t timestamp);
    
    // Manage positions
    void update_positions(uint64_t current_time);
    void close_position(const std::string& symbol, double price, uint64_t timestamp);
    
    // Write trade to buffer and optionally flush to disk
    void write_trade(const Trade& trade, bool force_flush = false);
    
    // Flush buffered trades to disk
    void flush_trades();
    
    // Calculate metrics
    double calculate_sharpe_ratio();
    double calculate_total_pnl();
    int calculate_win_rate();
};

} // namespace hft