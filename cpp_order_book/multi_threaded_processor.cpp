#include "multi_threaded_processor.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <thread>
#include <filesystem>
#include <set>
#include <iomanip>

namespace hft {

// Thread pool implementation
ThreadPool::ThreadPool(size_t num_threads) : stop(false) {
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

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    
    condition.notify_all();
    
    for (std::thread &worker : workers) {
        worker.join();
    }
}



MultiThreadedProcessor::MultiThreadedProcessor(
    size_t num_threads, 
    const std::string& input_file,
    const std::string& output_file,
    const std::string& trading_output_dir,
    size_t num_messages,
    const std::vector<std::string>& stock_filters
) : thread_pool(num_threads),
    input_file_(input_file),
    output_file_(output_file),
    trading_output_dir_(trading_output_dir),
    num_messages_(num_messages),
    stock_filters_(stock_filters) {
    
    // If num_threads is 0, use hardware concurrency
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
    }
    
    std::cout << "Using " << num_threads << " threads for processing" << std::endl;
}

// Helper function to load JSON data from file
std::vector<json> load_json_data(const std::string& filename, size_t max_messages = 0) {
    std::vector<json> messages;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return messages;
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
    
    std::string line;
    size_t count = 0;
    size_t line_number = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    size_t last_report_time = 0;
    
    while (std::getline(file, line) && (max_messages == 0 || count < max_messages)) {
        line_number++;
        
        // Report progress every 100,000 lines or 2 seconds
        if (line_number % 100000 == 0 || count % 100000 == 0) {
            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time).count();
            
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
            messages.push_back(j);
            count++;
        } catch (const json::parse_error& e) {
            std::cerr << "JSON parse error at line " << line_number << ": " << e.what() << std::endl;
        }
    }
    
    return messages;
}

// Helper function to filter messages by stock
std::vector<json> filter_messages_by_stock(
    const std::vector<json>& messages,
    const std::vector<std::string>& stocks) {
    
    std::vector<json> filtered;
    std::set<std::string> stock_set(stocks.begin(), stocks.end());
    
    for (const auto& message : messages) {
        if (message.contains("body")) {
            const auto& body = message["body"];
            
            if (body.contains("AddOrder") && body["AddOrder"].contains("stock")) {
                std::string stock = body["AddOrder"]["stock"];
                // Trim whitespace
                stock.erase(0, stock.find_first_not_of(" \t\n\r\f\v"));
                stock.erase(stock.find_last_not_of(" \t\n\r\f\v") + 1);
                
                if (stock_set.count(stock) > 0) {
                    filtered.push_back(message);
                }
            }
        }
    }
    
    return filtered;
}

// Helper function to write market data to output file
void write_market_data(
    const std::string& stock,
    const std::pair<double, double>& prices,
    const std::pair<uint32_t, uint32_t>& volumes,
    double imbalance,
    uint64_t timestamp,
    std::ofstream& output_stream) {
    
    json output;
    output["stock"] = stock;
    output["bid_price"] = prices.first;
    output["ask_price"] = prices.second;
    output["bid_volume"] = volumes.first;
    output["ask_volume"] = volumes.second;
    output["imbalance"] = imbalance;
    output["timestamp"] = timestamp;
    
    output_stream << output.dump() << std::endl;
}

void MultiThreadedProcessor::run() {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Load JSON data from file
    std::cout << "Loading JSON data from " << input_file_ << "..." << std::endl;
    auto messages = load_json_data(input_file_, num_messages_);
    
    if (messages.empty()) {
        std::cerr << "No messages loaded from " << input_file_ << std::endl;
        return;
    }
    
    std::cout << "Loaded " << messages.size() << " messages" << std::endl;
    
    // Filter messages by stock if requested
    if (!stock_filters_.empty()) {
        std::cout << "Filtering messages for stocks: ";
        for (const auto& stock : stock_filters_) {
            std::cout << stock << " ";
        }
        std::cout << std::endl;
        
        messages = filter_messages_by_stock(messages, stock_filters_);
        std::cout << "After filtering: " << messages.size() << " messages" << std::endl;
    }
    
    // Open output file
    std::ofstream output_stream(output_file_);
    if (!output_stream.is_open()) {
        std::cerr << "Failed to open output file: " << output_file_ << std::endl;
        return;
    }
    
    // Determine batch size based on number of messages and threads
    size_t num_batches = std::thread::hardware_concurrency() * 2; // 2 batches per thread
    size_t batch_size = (messages.size() + num_batches - 1) / num_batches;
    
    std::cout << "Processing " << messages.size() << " messages in " << num_batches 
              << " batches of approximately " << batch_size << " messages each" << std::endl;
    
    // Create shared order book for all threads
    OrderBook shared_order_book;
    
    // Create trading strategy
    LiquidityReversionStrategy strategy(
        shared_order_book,
        trading_output_dir_,
        1000000.0,  // Initial capital
        1.8,        // Liquidity threshold - increased to reduce false signals
        0.6,        // Reverse threshold - more aggressive selling on imbalance
        100,        // Position size
        15          // Hold time ticks - reduced to lock in profits faster
    );
    
    // Divide messages into batches and process in parallel
    std::vector<std::future<void>> futures;
    
    for (size_t i = 0; i < messages.size(); i += batch_size) {
        size_t end = std::min(i + batch_size, messages.size());
        std::vector<json> batch(messages.begin() + i, messages.begin() + end);
        
        futures.push_back(
            thread_pool.enqueue(
                &MultiThreadedProcessor::process_batch, 
                this, 
                batch, 
                std::ref(shared_order_book), 
                std::ref(strategy),
                std::ref(output_stream)
            )
        );
    }
    
    // Wait for all batches to complete and show progress
    size_t total_processed = 0;
    unsigned long last_progress_time = 0;
    
    while (total_processed < messages.size()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        size_t current_processed = processed_count_.load();
        if (current_processed > total_processed) {
            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time).count();
            
            // Progress update every 2 seconds
            if (elapsed_ms > last_progress_time + 2000) {
                last_progress_time = elapsed_ms;
                size_t current_processed = processed_count_.load();
                
                double msgs_per_sec = current_processed * 1000.0 / elapsed_ms;
                double completion_pct = (current_processed * 100.0) / messages.size();
                
                std::cout << "Processed " << current_processed << "/" << messages.size() 
                          << " messages (" << std::fixed << std::setprecision(2) << completion_pct << "%, "
                          << std::setprecision(2) << msgs_per_sec << " msgs/sec)" << std::endl;
                
                // Print some stats from the strategy periodically
                if (current_processed > total_processed + 1000000) {
                    strategy.print_performance();
                }
                
                total_processed = current_processed;
            }
        }
    }
    
    // Wait for all futures to complete
    for (auto& f : futures) {
        f.get();
    }
    
    // Print performance statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time).count();
    
    std::cout << "Processing complete!" << std::endl;
    std::cout << "Processed " << messages.size() << " messages in " << elapsed << " seconds" << std::endl;
    std::cout << "Rate: " << (messages.size() / (double)elapsed) << " messages per second" << std::endl;
    
    // Print trading strategy performance
    strategy.print_performance();
    
    // Close output file
    output_stream.close();
}

void MultiThreadedProcessor::process_batch(
    const std::vector<json>& messages, 
    OrderBook& order_book,
    LiquidityReversionStrategy& strategy,
    std::ofstream& output_stream) {
    
    std::set<std::string> unique_stocks;
    
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
            
            // Write market data to output (thread-safe via mutex)
            {
                std::lock_guard<std::mutex> lock(output_mutex_);
                write_market_data(stock, best_prices, volumes, imbalance, timestamp, output_stream);
            }
            
            // Execute trading strategy (thread-safe internally)
            strategy.process_market_update(
                stock, 
                best_prices.first,   // bid price
                best_prices.second,  // ask price
                volumes.first,       // bid volume
                volumes.second,      // ask volume
                imbalance,           // order book imbalance
                timestamp            // timestamp
            );
        }
        
        // Increment processed count
        processed_count_++;
    }
}

} // namespace hft
