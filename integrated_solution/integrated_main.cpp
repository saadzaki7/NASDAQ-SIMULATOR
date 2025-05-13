#include "parallel_parser.h"
#include "integrated_processor.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

using namespace integrated;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_itch_file> <num_messages> [trading_output_dir] [parser_threads] [processor_threads] [debug] [stocks...]" << std::endl;
        std::cerr << "  <input_itch_file>   : Path to the NASDAQ ITCH 5.0 binary file" << std::endl;
        std::cerr << "  <num_messages>      : Number of messages to process (0 for all)" << std::endl;
        std::cerr << "  [trading_output_dir]: Directory for trading output (default: trading_output_integrated)" << std::endl;
        std::cerr << "  [parser_threads]    : Number of threads for parser (default: half of hardware concurrency)" << std::endl;
        std::cerr << "  [processor_threads] : Number of threads for processor (default: half of hardware concurrency)" << std::endl;
        std::cerr << "  [debug]             : Enable debug mode (1) or disable (0) (default: 0)" << std::endl;
        std::cerr << "  [stocks...]         : Optional list of stock symbols to filter (default: process all stocks)" << std::endl;
        return 1;
    }
    
    // Parse command line arguments
    std::string input_file = argv[1];
    size_t num_messages = std::stoi(argv[2]);
    std::string trading_output_dir = (argc > 3) ? argv[3] : "trading_output_integrated";
    
    // Determine number of threads (0 means use hardware concurrency)
    size_t parser_threads = (argc > 4) ? std::stoi(argv[4]) : 0;
    size_t processor_threads = (argc > 5) ? std::stoi(argv[5]) : 0;
    bool debug_mode = (argc > 6) ? (std::stoi(argv[6]) != 0) : false;
    
    if (parser_threads == 0) {
        parser_threads = std::max(1u, std::thread::hardware_concurrency() / 2);
    }
    
    if (processor_threads == 0) {
        processor_threads = std::max(1u, std::thread::hardware_concurrency() / 2);
    }
    
    // Collect stock filters
    std::vector<std::string> stocks;
    for (int i = 7; i < argc; i++) {
        stocks.push_back(argv[i]);
    }
    
    std::cout << "Integrated ITCH Parser and Order Book Processor" << std::endl;
    std::cout << "---------------------------------" << std::endl;
    std::cout << "Input file: " << input_file << std::endl;
    std::cout << "Trading output directory: " << trading_output_dir << std::endl;
    std::cout << "Parser threads: " << parser_threads << std::endl;
    std::cout << "Processor threads: " << processor_threads << std::endl;
    std::cout << "Debug mode: " << (debug_mode ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Message limit: " << (num_messages > 0 ? std::to_string(num_messages) : "No limit") << std::endl;
    
    if (!stocks.empty()) {
        std::cout << "Stock filters: ";
        for (const auto& stock : stocks) {
            std::cout << stock << " ";
        }
        std::cout << std::endl;
    }
    std::cout << "---------------------------------" << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Create shared message queue
    ParsedMessageQueue message_queue(debug_mode);
    
    // Create parser and processor
    ParallelParser parser(input_file, message_queue, parser_threads, num_messages, debug_mode);
    IntegratedProcessor processor(message_queue, processor_threads, trading_output_dir, stocks, debug_mode);
    
    // Start parser thread
    std::thread parser_thread([&parser]() {
        parser.run();
    });
    
    // Start processor
    processor.run();
    
    // Wait for parser thread to complete (should already be done by this point)
    parser_thread.join();
    
    // Print overall performance statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time).count();
    
    std::cout << "\nOverall performance:" << std::endl;
    std::cout << "---------------------------------" << std::endl;
    std::cout << "Total execution time: " << elapsed << " seconds" << std::endl;
    std::cout << "Total messages processed: " << message_queue.total_messages() << std::endl;
    std::cout << "Overall throughput: " << (message_queue.total_messages() / (double)elapsed) 
              << " messages per second" << std::endl;
    
    return 0;
}
