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
#include <fstream>
#include <future>

using json = nlohmann::json;

namespace hft {

class ThreadPool {
public:
    ThreadPool(size_t num_threads);
    ~ThreadPool();

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

class MultiThreadedProcessor {
public:
    MultiThreadedProcessor(
        size_t num_threads, 
        const std::string& input_file,
        const std::string& output_file,
        const std::string& trading_output_dir,
        size_t num_messages = 0,
        const std::vector<std::string>& stock_filters = {}
    );
    
    void run();
    
private:
    // Thread pool
    ThreadPool thread_pool;
    
    // Input/output files
    std::string input_file_;
    std::string output_file_;
    std::string trading_output_dir_;
    size_t num_messages_;
    std::vector<std::string> stock_filters_;
    
    // Synchronization
    std::mutex output_mutex_;
    std::mutex order_book_mutex_;
    
    // Processing state
    std::atomic<size_t> processed_count_{0};
    std::atomic<size_t> message_count_{0};
    
    // Process a batch of messages
    void process_batch(const std::vector<json>& messages, OrderBook& order_book, LiquidityReversionStrategy& strategy, std::ofstream& output_stream);
};

} // namespace hft
