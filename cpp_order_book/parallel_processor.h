#pragma once

#include "order_book.h"
#include "trading_strategy.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <set>
#include <chrono>
#include <filesystem>

using json = nlohmann::json;

namespace hft {

// Market update structure to pass between threads
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
    void push(const MarketUpdate& update) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(update);
        lock.unlock();
        condition_.notify_one();
    }
    
    bool pop(MarketUpdate& update) {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { 
            return !queue_.empty() || is_done_; 
        });
        
        if (queue_.empty() && is_done_) {
            return false; // Signal consumer to exit
        }
        
        update = queue_.front();
        queue_.pop();
        return true;
    }
    
    void set_done() {
        std::unique_lock<std::mutex> lock(mutex_);
        is_done_ = true;
        lock.unlock();
        condition_.notify_all();
    }
    
    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
private:
    std::queue<MarketUpdate> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool is_done_ = false;
};

// Thread pool implementation
class ThreadPool {
public:
    ThreadPool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] {
                            return this->stop || !this->tasks.empty();
                        });
                        
                        if (this->stop && this->tasks.empty()) {
                            return;
                        }
                        
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    
                    task();
                }
            });
        }
    }
    
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        
        condition.notify_all();
        
        for (std::thread &worker : workers) {
            worker.join();
        }
    }
    
    // Add task to the thread pool
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) {
        using return_type = std::invoke_result_t<F, Args...>;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> res = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            
            tasks.emplace([task]() { (*task)(); });
        }
        
        condition.notify_one();
        return res;
    }
    
private:
    // Worker threads
    std::vector<std::thread> workers;
    
    // Task queue
    std::queue<std::function<void()>> tasks;
    
    // Synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// Parallel processor with producer-consumer pattern
class ParallelProcessor {
public:
    ParallelProcessor(
        size_t num_threads, 
        const std::string& input_file,
        const std::string& trading_output_dir,
        size_t num_messages = 0,
        const std::vector<std::string>& stock_filters = {}
    ) : thread_pool(num_threads),
        input_file_(input_file),
        trading_output_dir_(trading_output_dir),
        num_messages_(num_messages),
        stock_filters_(stock_filters) {
        
        // If num_threads is 0, use hardware concurrency
        if (num_threads == 0) {
            num_threads = std::thread::hardware_concurrency();
        }
        
        std::cout << "Using " << num_threads << " threads for processing" << std::endl;
    }
    
    void run() {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Create shared queue for market updates
        MarketUpdateQueue market_updates;
        
        // Create order book
        OrderBook order_book;
        
        // Create trading strategy with optimized parameters
        LiquidityReversionStrategy strategy(
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
                
                // Print progress every 100,000 updates
                if (updates_processed % 100000 == 0) {
                    std::cout << "Strategy processed " << updates_processed 
                              << " market updates" << std::endl;
                    
                    // Optionally print strategy performance
                    if (updates_processed % 1000000 == 0) {
                        strategy.print_performance();
                    }
                }
            }
            strategy_done = true;
        });
        
        // Load and process JSON data from file
        std::cout << "Loading JSON data from " << input_file_ << "..." << std::endl;
        
        std::ifstream file(input_file_);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << input_file_ << std::endl;
            market_updates.set_done();
            strategy_thread.join();
            return;
        }
        
        // Get file size for progress reporting
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::cout << "File size: " << file_size << " bytes" << std::endl;
        
        // Check if the file starts with a '[' character
        char first_char = file.peek();
        bool is_json_array = (first_char == '[');
        
        if (is_json_array) {
            // Skip the opening bracket
            file.get();
        }
        
        // Process JSON data
        std::string line;
        size_t count = 0;
        size_t line_number = 0;
        auto load_start_time = std::chrono::high_resolution_clock::now();
        size_t last_report_time = 0;
        
        // Create a set of futures for batch processing
        std::vector<std::future<void>> futures;
        std::vector<json> batch;
        size_t batch_size = 1000; // Process in batches of 1000 messages
        
        while (std::getline(file, line) && (num_messages_ == 0 || count < num_messages_)) {
            line_number++;
            
            // Report progress periodically
            if (line_number % 100000 == 0 || count % 100000 == 0) {
                auto current_time = std::chrono::high_resolution_clock::now();
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - load_start_time).count();
                
                if (elapsed_ms - last_report_time > 2000 || line_number % 1000000 == 0) {
                    last_report_time = elapsed_ms;
                    size_t current_pos = file.tellg();
                    double percent_complete = (current_pos * 100.0) / file_size;
                    double lines_per_sec = line_number * 1000.0 / elapsed_ms;
                    double msgs_per_sec = count * 1000.0 / elapsed_ms;
                    
                    std::cout << "Loading: " << std::fixed << std::setprecision(2) << percent_complete 
                              << "% complete, read " << count << " messages, " << line_number 
                              << " lines (" << std::setprecision(0) << lines_per_sec << " lines/sec, "
                              << std::setprecision(0) << msgs_per_sec << " msgs/sec)" << std::endl;
                    
                    // Also report strategy progress
                    std::cout << "Queue size: " << market_updates.size()
                              << ", Strategy processed: " << updates_processed.load() << std::endl;
                }
            }
            
            try {
                // Trim whitespace
                line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));
                line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);
                
                // Skip empty lines
                if (line.empty()) {
                    continue;
                }
                
                // Remove trailing comma if present
                if (line.back() == ',') {
                    line.pop_back();
                }
                
                // Skip the closing bracket if we're at the end of a JSON array
                if (is_json_array && line == "]") {
                    continue;
                }
                
                // Parse JSON
                json j = json::parse(line);
                batch.push_back(j);
                count++;
                
                // When batch is full, process it
                if (batch.size() >= batch_size) {
                    auto batch_copy = batch; // Copy to capture by value
                    futures.push_back(
                        thread_pool.enqueue([this, batch_copy, &order_book, &market_updates]() {
                            process_batch(batch_copy, order_book, market_updates);
                        })
                    );
                    batch.clear();
                }
            } catch (const json::parse_error& e) {
                std::cerr << "JSON parse error at line " << line_number << ": " << e.what() << std::endl;
            }
        }
        
        // Process any remaining messages in the last batch
        if (!batch.empty()) {
            auto batch_copy = batch;
            futures.push_back(
                thread_pool.enqueue([this, batch_copy, &order_book, &market_updates]() {
                    process_batch(batch_copy, order_book, market_updates);
                })
            );
        }
        
        // Wait for all futures to complete
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
    // Thread pool
    ThreadPool thread_pool;
    
    // Input file
    std::string input_file_;
    std::string trading_output_dir_;
    size_t num_messages_;
    std::vector<std::string> stock_filters_;
    
    // Process a batch of messages
    void process_batch(
        const std::vector<json>& messages, 
        OrderBook& order_book,
        MarketUpdateQueue& market_updates
    ) {
        std::mutex order_book_mutex;
        std::set<std::string> unique_stocks;
        
        for (const auto& message : messages) {
            std::string message_json = message.dump();
            
            // Process message in order book (thread-safe via mutex)
            {
                std::lock_guard<std::mutex> lock(order_book_mutex);
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
                    unique_stocks.insert(stock);
                }
            }
            
            // Update market data and trading strategy if we have a valid stock
            if (!stock.empty()) {
                // Get market data from order book (thread-safe via mutex)
                double imbalance;
                std::pair<double, double> best_prices;
                std::pair<uint32_t, uint32_t> volumes;
                
                {
                    std::lock_guard<std::mutex> lock(order_book_mutex);
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

} // namespace hft
