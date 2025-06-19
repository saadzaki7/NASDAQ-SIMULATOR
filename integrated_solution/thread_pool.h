#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <iostream>

namespace integrated {

// Thread pool implementation
class ThreadPool {
public:
    ThreadPool(size_t num_threads, bool debug_mode = false) 
        : stop(false), debug_mode_(debug_mode) {
        
        if (debug_mode_) {
            std::cout << "DEBUG: Creating thread pool with " << num_threads << " threads" << std::endl;
        }
        
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this, i] {
                if (debug_mode_) {
                    std::cout << "DEBUG: Thread " << i << " started" << std::endl;
                }
                
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] {
                            return this->stop || !this->tasks.empty();
                        });
                        
                        if (this->stop && this->tasks.empty()) {
                            if (debug_mode_) {
                                std::cout << "DEBUG: Thread exiting" << std::endl;
                            }
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
        
        if (debug_mode_) {
            std::cout << "DEBUG: Thread pool destroyed" << std::endl;
        }
    }
    
    // Get the number of threads in the pool
    size_t get_thread_count() const {
        return workers.size();
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
    bool debug_mode_;
};

} // namespace integrated
