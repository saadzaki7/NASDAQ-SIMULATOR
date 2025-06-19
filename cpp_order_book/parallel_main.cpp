#include "parallel_processor.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>

using namespace hft;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file> [num_messages] [trading_output_dir] [num_threads] [stocks...]" << std::endl;
        return 1;
    }
    
    // Parse command line arguments
    std::string input_file = argv[1];
    size_t num_messages = (argc > 2) ? std::stoi(argv[2]) : 0;
    std::string trading_output_dir = (argc > 3) ? argv[3] : "trading_output_parallel";
    
    // Determine number of threads (0 means use hardware concurrency)
    size_t num_threads = (argc > 4) ? std::stoi(argv[4]) : 0;
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
    }
    
    // Collect stock filters
    std::vector<std::string> stocks;
    for (int i = 5; i < argc; i++) {
        stocks.push_back(argv[i]);
    }
    
    std::cout << "Parallel Order Book Processor" << std::endl;
    std::cout << "---------------------------------" << std::endl;
    std::cout << "Input file: " << input_file << std::endl;
    std::cout << "Trading output directory: " << trading_output_dir << std::endl;
    std::cout << "Number of threads: " << num_threads << std::endl;
    std::cout << "Message limit: " << (num_messages > 0 ? std::to_string(num_messages) : "No limit") << std::endl;
    
    if (!stocks.empty()) {
        std::cout << "Stock filters: ";
        for (const auto& stock : stocks) {
            std::cout << stock << " ";
        }
        std::cout << std::endl;
    }
    std::cout << "---------------------------------" << std::endl;
    
    // Create and run the parallel processor
    ParallelProcessor processor(
        num_threads,
        input_file,
        trading_output_dir,
        num_messages,
        stocks
    );
    
    // Run the processor
    processor.run();
    
    return 0;
}