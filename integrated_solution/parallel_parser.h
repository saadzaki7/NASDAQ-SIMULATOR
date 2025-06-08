#pragma once

#include "../cpp_parser/include/parser.h"
#include "../cpp_parser/include/json_serializer.h"
#include "thread_pool.h"
#include "parsed_message_queue.h"
#include <string>
#include <vector>
#include <future>
#include <chrono>
#include <iomanip>

namespace integrated {

class ParallelParser {
public:
    ParallelParser(
        const std::string& input_file,
        ParsedMessageQueue& message_queue,
        size_t num_threads,
        size_t message_limit = 0,
        bool debug_mode = false
    ) : thread_pool(num_threads, debug_mode),
        input_file_(input_file),
        message_queue_(message_queue),
        message_limit_(message_limit),
        debug_mode_(debug_mode) {
        
        if (debug_mode_) {
            std::cout << "DEBUG: ParallelParser initialized with:" << std::endl
                      << "  - Input file: " << input_file_ << std::endl
                      << "  - Threads: " << num_threads << std::endl
                      << "  - Message limit: " << (message_limit_ > 0 ? std::to_string(message_limit_) : "No limit") << std::endl;
        }
    }
    
    void run() {
        if (debug_mode_) {
            std::cout << "DEBUG: Starting parser" << std::endl;
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            // Create parser
            if (debug_mode_) {
                std::cout << "DEBUG: Creating parser from file: " << input_file_ << std::endl;
            }
            
            auto parser = itch::Parser::from_file(input_file_);
            
            // Process messages in batches
            std::vector<std::future<void>> futures;
            std::vector<itch::Message> batch;
            size_t batch_size = 100; // Smaller batch size for proof of concept
            size_t message_count = 0;
            size_t last_report_time = 0;
            auto batch_start_time = std::chrono::high_resolution_clock::now();
            
            if (debug_mode_) {
                std::cout << "DEBUG: Starting to parse messages with batch size: " << batch_size << std::endl;
            }
            
            while (auto message = parser->parse_message()) {
                batch.push_back(*message);
                message_count++;
                
                // Report progress periodically
                if (message_count % 10000 == 0 || message_count == 1) {
                    auto current_time = std::chrono::high_resolution_clock::now();
                    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        current_time - start_time).count();
                    
                    if (elapsed_ms - last_report_time > 1000 || message_count == 1) {
                        last_report_time = elapsed_ms;
                        double msgs_per_sec = message_count * 1000.0 / elapsed_ms;
                        
                        std::cout << "Parser: Processed " << message_count << " messages"
                                  << " (" << std::fixed << std::setprecision(0) << msgs_per_sec << " msgs/sec)" << std::endl;
                    }
                }
                
                if (batch.size() >= batch_size) {
                    auto batch_copy = batch;
                    futures.push_back(
                        thread_pool.enqueue([this, batch_copy]() {
                            process_batch(batch_copy);
                        })
                    );
                    batch.clear();
                }
                
                if (message_limit_ > 0 && message_count >= message_limit_) {
                    if (debug_mode_) {
                        std::cout << "DEBUG: Reached message limit of " << message_limit_ << ". Stopping." << std::endl;
                    }
                    break;
                }
            }
            
            // Process any remaining messages
            if (!batch.empty()) {
                if (debug_mode_) {
                    std::cout << "DEBUG: Processing final batch of " << batch.size() << " messages" << std::endl;
                }
                
                auto batch_copy = batch;
                futures.push_back(
                    thread_pool.enqueue([this, batch_copy]() {
                        process_batch(batch_copy);
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
            
            // Signal that no more messages will be coming
            message_queue_.set_done();
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time).count();
            
            std::cout << "Parser completed: Processed " << message_count << " messages in " 
                      << (elapsed / 1000.0) << " seconds" << std::endl;
            std::cout << "Parser throughput: " << std::fixed << std::setprecision(2) 
                      << (message_count * 1000.0 / elapsed) << " messages/second" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "ERROR in parser: " << e.what() << std::endl;
            message_queue_.set_done(); // Signal that we're done in case of error
        }
    }
    
private:
    ThreadPool thread_pool;
    std::string input_file_;
    ParsedMessageQueue& message_queue_;
    size_t message_limit_;
    bool debug_mode_;
    
    void process_batch(const std::vector<itch::Message>& messages) {
        if (debug_mode_ && messages.size() > 0) {
            std::cout << "DEBUG: Processing batch of " << messages.size() << " messages" << std::endl;
        }
        
        for (const auto& message : messages) {
            // Convert to JSON
            json json_message = itch::JsonSerializer::to_json(message);
            
            // Push to queue
            message_queue_.push(json_message);
        }
    }
};

} // namespace integrated
