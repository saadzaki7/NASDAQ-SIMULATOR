#pragma once

#include "../cpp_order_book/order_book.h"
#include "../cpp_order_book/trading_strategy.h"
#include "thread_pool.h"
#include "parsed_message_queue.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <filesystem>

namespace integrated {

// Market update structure (same as in parallel_processor.h)
struct MarketUpdate {
    std::string symbol;
    double bid_price;
    double ask_price;
    uint32_t bid_volume;
    uint32_t ask_volume;
    double imbalance;
    uint64_t timestamp;
};

// Thread-safe queue for market updates
class MarketUpdateQueue {
public:
    MarketUpdateQueue(bool debug_mode = false) : debug_mode_(debug_mode), update_count_(0) {
        if (debug_mode_) {
            std::cout << "DEBUG: MarketUpdateQueue initialized" << std::endl;
        }
    }
    
    void push(const MarketUpdate& update) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(update);
        update_count_++;
        
        if (debug_mode_ && update_count_ % 10000 == 0) {
            std::cout << "DEBUG: MarketUpdateQueue pushed update #" << update_count_ 
                      << ", current queue size: " << queue_.size() << std::endl;
        }
        
        lock.unlock();
        condition_.notify_one();
    }
    
    bool pop(MarketUpdate& update) {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { 
            return !queue_.empty() || is_done_; 
        });
        
        if (queue_.empty() && is_done_) {
            if (debug_mode_) {
                std::cout << "DEBUG: MarketUpdateQueue is empty and marked as done, signaling consumer to exit" << std::endl;
            }
            return false; // Signal consumer to exit
        }
        
        update = queue_.front();
        queue_.pop();
        
        if (debug_mode_ && pop_count_ % 10000 == 0) {
            std::cout << "DEBUG: MarketUpdateQueue popped update #" << pop_count_ 
                      << ", remaining queue size: " << queue_.size() << std::endl;
        }
        pop_count_++;
        
        return true;
    }
    
    void set_done() {
        std::unique_lock<std::mutex> lock(mutex_);
        is_done_ = true;
        if (debug_mode_) {
            std::cout << "DEBUG: MarketUpdateQueue marked as done, total updates: " << update_count_ << std::endl;
        }
        lock.unlock();
        condition_.notify_all();
    }
    
    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    size_t total_updates() const {
        return update_count_;
    }
    
private:
    std::queue<MarketUpdate> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool is_done_ = false;
    bool debug_mode_ = false;
    size_t update_count_ = 0;
    size_t pop_count_ = 0;
};

class IntegratedProcessor {
public:
    IntegratedProcessor(
        ParsedMessageQueue& message_queue,
        size_t num_threads,
        const std::string& trading_output_dir,
        const std::vector<std::string>& stock_filters = {},
        bool debug_mode = false
    ) : thread_pool(num_threads, debug_mode),
        message_queue_(message_queue),
        trading_output_dir_(trading_output_dir),
        stock_filters_(stock_filters),
        debug_mode_(debug_mode) {
        
        if (debug_mode_) {
            std::cout << "DEBUG: IntegratedProcessor initialized with:" << std::endl
                      << "  - Threads: " << num_threads << std::endl
                      << "  - Trading output directory: " << trading_output_dir_ << std::endl;
            
            if (!stock_filters_.empty()) {
                std::cout << "  - Stock filters: ";
                for (const auto& stock : stock_filters_) {
                    std::cout << stock << " ";
                }
                std::cout << std::endl;
            }
        }
        
        // Create output directory if it doesn't exist
        std::filesystem::create_directories(trading_output_dir_);
    }
    
    void run() {
        if (debug_mode_) {
            std::cout << "DEBUG: Starting processor" << std::endl;
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Create market update queue
        MarketUpdateQueue market_updates(debug_mode_);
        
        // Create order book
        hft::OrderBook order_book;
        
        // Create trading strategy with optimized parameters from memory
        hft::LiquidityReversionStrategy strategy(
            order_book,
            trading_output_dir_,
            1000000.0,  // Initial capital
            1.8,        // Liquidity threshold - increased to reduce false signals
            0.6,        // Reverse threshold - more aggressive selling on imbalance
            100,        // Position size
            15          // Hold time ticks - reduced to lock in profits faster
        );
        
        // Start consumer thread for trading strategy
        std::atomic<bool> strategy_done(false);
        std::atomic<size_t> updates_processed(0);
        
        if (debug_mode_) {
            std::cout << "DEBUG: Starting strategy thread" << std::endl;
        }
        
        std::thread strategy_thread([&] {
            MarketUpdate update;
            while (market_updates.pop(update)) {
                strategy.process_market_update(
                    update.symbol,
                    update.bid_price,
                    update.ask_price,
                    update.bid_volume,
                    update.ask_volume,
                    update.imbalance,
                    update.timestamp
                );
                updates_processed++;
                
                // Print progress every 10,000 updates for proof of concept
                if (updates_processed % 10000 == 0) {
                    std::cout << "Strategy processed " << updates_processed 
                              << " market updates" << std::endl;
                    
                    // Optionally print strategy performance
                    if (updates_processed % 50000 == 0) {
                        strategy.print_performance();
                    }
                }
            }
            strategy_done = true;
            
            if (debug_mode_) {
                std::cout << "DEBUG: Strategy thread completed" << std::endl;
            }
        });
        
        // Process messages from queue
        json message;
        size_t count = 0;
        std::vector<std::future<void>> futures;
        std::vector<json> batch;
        size_t batch_size = 100; // Smaller batch size for proof of concept
        size_t last_report_time = 0;
        
        if (debug_mode_) {
            std::cout << "DEBUG: Starting to process messages with batch size: " << batch_size << std::endl;
        }
        
        while (message_queue_.pop(message)) {
            batch.push_back(message);
            count++;
            
            // Report progress periodically
            if (count % 10000 == 0 || count == 1) {
                auto current_time = std::chrono::high_resolution_clock::now();
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - start_time).count();
                
                if (elapsed_ms - last_report_time > 1000 || count == 1) {
                    last_report_time = elapsed_ms;
                    double msgs_per_sec = count * 1000.0 / elapsed_ms;
                    
                    std::cout << "Processor: Processed " << count << " messages"
                              << " (" << std::fixed << std::setprecision(0) << msgs_per_sec << " msgs/sec)" << std::endl;
                    std::cout << "Queue size: " << market_updates.size()
                              << ", Strategy processed: " << updates_processed.load() << std::endl;
                }
            }
            
            if (batch.size() >= batch_size) {
                auto batch_copy = batch;
                futures.push_back(
                    thread_pool.enqueue([this, batch_copy, &order_book, &market_updates]() {
                        process_batch(batch_copy, order_book, market_updates);
                    })
                );
                batch.clear();
            }
        }
        
        // Process any remaining messages
        if (!batch.empty()) {
            if (debug_mode_) {
                std::cout << "DEBUG: Processing final batch of " << batch.size() << " messages" << std::endl;
            }
            
            auto batch_copy = batch;
            futures.push_back(
                thread_pool.enqueue([this, batch_copy, &order_book, &market_updates]() {
                    process_batch(batch_copy, order_book, market_updates);
                })
            );
        }
        
        // Wait for all futures to complete
        if (debug_mode_) {
            std::cout << "DEBUG: Waiting for " << futures.size() << " batch processing tasks to complete" << std::endl;
        }
        
        for (auto& f : futures) {
            f.get();
        }
        
        std::cout << "All batches processed, waiting for strategy to catch up..." << std::endl;
        std::cout << "Queue size: " << market_updates.size() << std::endl;
        
        // Signal that no more updates will be coming
        market_updates.set_done();
        
        // Wait for strategy thread to complete
        strategy_thread.join();
        
        // Print performance statistics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            end_time - start_time).count();
        
        std::cout << "Processing complete!" << std::endl;
        std::cout << "Processed " << count << " messages in " << elapsed << " seconds" << std::endl;
        std::cout << "Rate: " << (count / (double)elapsed) << " messages per second" << std::endl;
        std::cout << "Market updates processed: " << updates_processed.load() << std::endl;
        
        // Print trading strategy performance
        strategy.print_performance();
    }
    
private:
    ThreadPool thread_pool;
    ParsedMessageQueue& message_queue_;
    std::string trading_output_dir_;
    std::vector<std::string> stock_filters_;
    bool debug_mode_;
    std::mutex order_book_mutex_;
    
    void process_batch(
        const std::vector<json>& messages, 
        hft::OrderBook& order_book,
        MarketUpdateQueue& market_updates
    ) {
        if (debug_mode_ && messages.size() > 0) {
            std::cout << "DEBUG: Processing batch of " << messages.size() << " messages in order book" << std::endl;
        }
        
        for (const auto& message : messages) {
            std::string message_json = message.dump();
            
            // Process message in order book (thread-safe via mutex)
            {
                std::lock_guard<std::mutex> lock(order_book_mutex_);
                order_book.process_message(message_json);
            }
            
            // Extract stock from message
            std::string stock;
            if (message.contains("body")) {
                const auto& body = message["body"];
                
                if (body.contains("AddOrder") && body["AddOrder"].contains("stock")) {
                    stock = body["AddOrder"]["stock"];
                    // Trim whitespace from stock symbol
                    stock.erase(0, stock.find_first_not_of(" \t\n\r\f\v"));
                    stock.erase(stock.find_last_not_of(" \t\n\r\f\v") + 1);
                }
            }
            
            // Update market data and trading strategy if we have a valid stock
            if (!stock.empty()) {
                // Skip if we have stock filters and this stock is not in the filter
                if (!stock_filters_.empty() && 
                    std::find(stock_filters_.begin(), stock_filters_.end(), stock) == stock_filters_.end()) {
                    continue;
                }
                
                // Get market data from order book (thread-safe via mutex)
                double imbalance;
                std::pair<double, double> best_prices;
                std::pair<uint32_t, uint32_t> volumes;
                
                {
                    std::lock_guard<std::mutex> lock(order_book_mutex_);
                    best_prices = order_book.get_best_prices(stock);
                    volumes = order_book.get_volumes(stock);
                    imbalance = order_book.get_imbalance(stock);
                }
                
                // Get timestamp
                uint64_t timestamp = 0;
                if (message.contains("timestamp")) {
                    if (message["timestamp"].is_string()) {
                        timestamp = std::stoull(message["timestamp"].get<std::string>());
                    } else {
                        timestamp = message["timestamp"].get<uint64_t>();
                    }
                }
                
                // Push market update to queue for strategy thread
                MarketUpdate update{
                    stock,
                    best_prices.first,   // bid price
                    best_prices.second,  // ask price
                    volumes.first,       // bid volume
                    volumes.second,      // ask volume
                    imbalance,           // order book imbalance
                    timestamp            // timestamp
                };
                
                market_updates.push(update);
            }
        }
    }
};

} // namespace integrated
