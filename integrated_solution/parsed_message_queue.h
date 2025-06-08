#pragma once

#include <nlohmann/json.hpp>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>

using json = nlohmann::json;

namespace integrated {

// Thread-safe queue for parsed ITCH messages
class ParsedMessageQueue {
public:
    ParsedMessageQueue(bool debug_mode = false) : debug_mode_(debug_mode), message_count_(0) {
        if (debug_mode_) {
            std::cout << "DEBUG: ParsedMessageQueue initialized" << std::endl;
        }
    }
    
    void push(const json& message) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(message);
        message_count_++;
        
        if (debug_mode_ && message_count_ % 10000 == 0) {
            std::cout << "DEBUG: Queue pushed message #" << message_count_ 
                      << ", current queue size: " << queue_.size() << std::endl;
        }
        
        lock.unlock();
        condition_.notify_one();
    }
    
    bool pop(json& message) {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { 
            return !queue_.empty() || is_done_; 
        });
        
        if (queue_.empty() && is_done_) {
            if (debug_mode_) {
                std::cout << "DEBUG: Queue is empty and marked as done, signaling consumer to exit" << std::endl;
            }
            return false; // Signal consumer to exit
        }
        
        message = queue_.front();
        queue_.pop();
        
        if (debug_mode_ && pop_count_ % 10000 == 0) {
            std::cout << "DEBUG: Queue popped message #" << pop_count_ 
                      << ", remaining queue size: " << queue_.size() << std::endl;
        }
        pop_count_++;
        
        return true;
    }
    
    void set_done() {
        std::unique_lock<std::mutex> lock(mutex_);
        is_done_ = true;
        if (debug_mode_) {
            std::cout << "DEBUG: Queue marked as done, total messages: " << message_count_ << std::endl;
        }
        lock.unlock();
        condition_.notify_all();
    }
    
    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    size_t total_messages() const {
        return message_count_;
    }
    
private:
    std::queue<json> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool is_done_ = false;
    bool debug_mode_ = false;
    size_t message_count_ = 0;
    size_t pop_count_ = 0;
};

} // namespace integrated
