#include "trading_strategy.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <sstream>

namespace hft {

LiquidityReversionStrategy::LiquidityReversionStrategy(
    OrderBook& order_book,
    const std::string& output_dir,
    double initial_capital,
    double liquidity_threshold,
    double reverse_threshold,
    int position_size,
    int hold_time_ticks
) : order_book_(order_book),
    output_dir_(output_dir),
    initial_capital_(initial_capital),
    current_capital_(initial_capital),
    liquidity_threshold_(liquidity_threshold),
    reverse_threshold_(reverse_threshold),
    position_size_(position_size),
    hold_time_ticks_(hold_time_ticks) {
    
    // Create output directory if it doesn't exist
    std::filesystem::create_directories(output_dir_);
    
    // Get current date for trade output file
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << output_dir_ << "/trades_" << std::put_time(std::localtime(&in_time_t), "%Y%m%d") << ".csv";
    
    // Open trade output file
    trades_file_.open(ss.str());
    
    // Write header to trade output file
    trades_file_ << "timestamp,symbol,side,quantity,price,pnl" << std::endl;
}

LiquidityReversionStrategy::~LiquidityReversionStrategy() {
    // Close trades file
    if (trades_file_.is_open()) {
        trades_file_.close();
    }
    
    // Write performance summary
    std::ofstream summary_file(output_dir_ + "/performance_summary.json");
    if (summary_file.is_open()) {
        json summary;
        summary["initial_capital"] = initial_capital_;
        summary["final_capital"] = current_capital_;
        summary["total_pnl"] = calculate_total_pnl();
        summary["return_pct"] = (current_capital_ - initial_capital_) / initial_capital_ * 100.0;
        summary["num_trades"] = trades_.size();
        summary["win_rate"] = calculate_win_rate();
        summary["sharpe_ratio"] = calculate_sharpe_ratio();
        
        summary_file << summary.dump(4) << std::endl;
        summary_file.close();
    }
}

void LiquidityReversionStrategy::process_market_update(
    const std::string& symbol, 
    double bid_price, double ask_price,
    uint32_t bid_volume, uint32_t ask_volume,
    double imbalance, uint64_t timestamp) {
    
    // Skip invalid prices
    if (bid_price <= 0 || ask_price <= 0) {
        return;
    }
    
    // Calculate mid price
    double mid_price = (bid_price + ask_price) / 2.0;
    
    // Update price history
    if (price_history_[symbol].size() >= 100) {
        price_history_[symbol].pop_front();
    }
    price_history_[symbol].push_back(mid_price);
    
    // Update positions (check for exit based on hold time)
    update_positions(timestamp);
    
    // Skip if we already have a position in this symbol
    if (positions_.find(symbol) != positions_.end()) {
        return;
    }
    
    // Check if we have enough price history
    if (price_history_[symbol].size() < 5) {
        return;
    }
    
    // Liquidity Reversion Strategy
    // - Buy when imbalance > threshold (more buy orders than sell orders)
    // - Sell when imbalance < -threshold (more sell orders than buy orders)
    
    // Check imbalance for buy signal
    if (imbalance > liquidity_threshold_) {
        execute_buy(symbol, ask_price, position_size_, timestamp);
    }
    // Check imbalance for sell signal
    else if (imbalance < reverse_threshold_) {
        execute_sell(symbol, bid_price, position_size_, timestamp);
    }
}

void LiquidityReversionStrategy::run() {
    // Implementation will depend on how market updates are received
    // This method would be called if all market data is available upfront
    // Otherwise, process_market_update is called for each update
}

void LiquidityReversionStrategy::print_performance() {
    // We still collect the data for statistics, but don't print it
    calculate_total_pnl();
    calculate_win_rate();
    calculate_sharpe_ratio();
}

void LiquidityReversionStrategy::execute_buy(
    const std::string& symbol, double price, int quantity, uint64_t timestamp) {
    
    // Create position
    Position position;
    position.symbol = symbol;
    position.quantity = quantity;
    position.entry_price = price;
    position.entry_time = timestamp;
    position.pnl = 0.0;
    
    positions_[symbol] = position;
    position_hold_time_[symbol] = 0;
    
    // Record trade
    Trade trade;
    trade.symbol = symbol;
    trade.side = "Buy";
    trade.quantity = quantity;
    trade.price = price;
    trade.timestamp = timestamp;
    trade.pnl = 0.0;
    
    trades_.push_back(trade);
    write_trade(trade);
    
    // Update capital
    current_capital_ -= price * quantity;
}

void LiquidityReversionStrategy::execute_sell(
    const std::string& symbol, double price, int quantity, uint64_t timestamp) {
    
    // Create position (negative quantity for short)
    Position position;
    position.symbol = symbol;
    position.quantity = -quantity;
    position.entry_price = price;
    position.entry_time = timestamp;
    position.pnl = 0.0;
    
    positions_[symbol] = position;
    position_hold_time_[symbol] = 0;
    
    // Record trade
    Trade trade;
    trade.symbol = symbol;
    trade.side = "Sell";
    trade.quantity = quantity;
    trade.price = price;
    trade.timestamp = timestamp;
    trade.pnl = 0.0;
    
    trades_.push_back(trade);
    write_trade(trade);
    
    // Update capital
    current_capital_ += price * quantity;
}

void LiquidityReversionStrategy::update_positions(uint64_t current_time) {
    // Check all positions for hold time expiration
    std::vector<std::string> symbols_to_close;
    
    for (auto& pair : position_hold_time_) {
        const std::string& symbol = pair.first;
        int& hold_time = pair.second;
        
        // Increment hold time
        hold_time++;
        
        // If hold time exceeded, add to list of positions to close
        if (hold_time >= hold_time_ticks_) {
            symbols_to_close.push_back(symbol);
        }
    }
    
    // Close positions that have reached hold time
    for (const auto& symbol : symbols_to_close) {
        // Get current prices
        auto prices = order_book_.get_best_prices(symbol);
        double bid_price = prices.first;
        double ask_price = prices.second;
        
        // Close position at market price
        if (bid_price > 0 && ask_price > 0) {
            close_position(symbol, (bid_price + ask_price) / 2.0, current_time);
        }
    }
}

void LiquidityReversionStrategy::close_position(
    const std::string& symbol, double price, uint64_t timestamp) {
    
    // Find position
    auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        return;
    }
    
    // Calculate P&L
    Position& position = it->second;
    int quantity = std::abs(position.quantity);
    double pnl = 0.0;
    std::string side;
    
    if (position.quantity > 0) {
        // Long position
        pnl = (price - position.entry_price) * quantity;
        side = "Sell"; // Closing a long position by selling
    } else {
        // Short position
        pnl = (position.entry_price - price) * quantity;
        side = "Buy"; // Closing a short position by buying
    }
    
    // Record trade
    Trade trade;
    trade.symbol = symbol;
    trade.side = side;
    trade.quantity = quantity;
    trade.price = price;
    trade.timestamp = timestamp;
    trade.pnl = pnl;
    
    trades_.push_back(trade);
    write_trade(trade);
    
    // Update capital
    current_capital_ += (position.quantity > 0) ? price * quantity : -price * quantity;
    current_capital_ += pnl;
    
    // Remove position
    positions_.erase(symbol);
    position_hold_time_.erase(symbol);
}

void LiquidityReversionStrategy::write_trade(const Trade& trade) {
    if (trades_file_.is_open()) {
        trades_file_ << trade.timestamp << ","
                    << trade.symbol << ","
                    << trade.side << ","
                    << trade.quantity << ","
                    << std::fixed << std::setprecision(4) << trade.price << ","
                    << std::fixed << std::setprecision(2) << trade.pnl << std::endl;
    }
}

double LiquidityReversionStrategy::calculate_sharpe_ratio() {
    if (trades_.empty()) {
        return 0.0;
    }
    
    // Calculate daily returns
    std::vector<double> returns;
    double prev_capital = initial_capital_;
    
    for (const auto& trade : trades_) {
        double new_capital = prev_capital + trade.pnl;
        double return_pct = (new_capital - prev_capital) / prev_capital;
        returns.push_back(return_pct);
        prev_capital = new_capital;
    }
    
    // Calculate mean return
    double sum = 0.0;
    for (double r : returns) {
        sum += r;
    }
    double mean = sum / returns.size();
    
    // Calculate standard deviation
    double sq_sum = 0.0;
    for (double r : returns) {
        sq_sum += (r - mean) * (r - mean);
    }
    double std_dev = std::sqrt(sq_sum / returns.size());
    
    // Calculate Sharpe ratio (assuming 0 risk-free rate)
    return std_dev > 0 ? (mean / std_dev) * std::sqrt(252.0) : 0.0; // Annualized 
}

double LiquidityReversionStrategy::calculate_total_pnl() {
    double total_pnl = 0.0;
    for (const auto& trade : trades_) {
        total_pnl += trade.pnl;
    }
    return total_pnl;
}

int LiquidityReversionStrategy::calculate_win_rate() {
    if (trades_.empty()) {
        return 0;
    }
    
    int winning_trades = 0;
    for (const auto& trade : trades_) {
        if (trade.pnl > 0) {
            winning_trades++;
        }
    }
    
    return (winning_trades * 100) / trades_.size();
}

} // namespace hft
