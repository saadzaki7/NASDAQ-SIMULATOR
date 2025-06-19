#include "../include/parser.h"
#include "../include/json_serializer.h"
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <map>
#include <cstdint>
#include <memory>
#include <variant>
#include <iomanip>
#include <sys/resource.h>

namespace fs = std::filesystem;

// Function to get current memory usage in KB
size_t get_memory_usage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
}

// Default configuration
bool debug_mode = false;
size_t message_limit = 0; // 0 means unlimited

// CLI configuration structure
struct CliConfig {
    std::string input_path;
    std::string output_path;
    bool debug_mode = false;
    size_t message_limit = 0; // 0 means unlimited
    bool output_to_stdout = false;
    bool show_stats = false;
};

void print_usage(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [options] <path-to-itch-file>" << std::endl;
    std::cout << "Parses NASDAQ ITCH 5.0 file and outputs JSON." << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  -h, --help       Show this help message" << std::endl;
    std::cout << "  -o <file>        Output to specified file (default: <input-file>.json)" << std::endl;
    std::cout << "  -l <number>      Limit number of messages to process (default: all)" << std::endl;
    std::cout << "  -d               Enable debug mode with verbose output" << std::endl;
    std::cout << "  -s               Show statistics after parsing" << std::endl;
    std::cout << "  -c               Output to stdout instead of file" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  " << program_name << " data.itch              # Basic usage" << std::endl;
    std::cout << "  " << program_name << " -l 2000000 data.itch   # Process 2M messages" << std::endl;
    std::cout << "  " << program_name << " -o output.json data.itch # Custom output file" << std::endl;
}

CliConfig parse_arguments(int argc, char** argv) {
    CliConfig config;
    
    int i = 1;
    while (i < argc) {
        std::string arg = argv[i++];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            exit(0);
        } else if (arg == "-o" && i < argc) {
            config.output_path = argv[i++];
        } else if (arg == "-l" && i < argc) {
            try {
                config.message_limit = std::stoull(argv[i++]);
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid message limit. Must be a positive number." << std::endl;
                exit(1);
            }
        } else if (arg == "-d") {
            config.debug_mode = true;
        } else if (arg == "-s") {
            config.show_stats = true;
        } else if (arg == "-c") {
            config.output_to_stdout = true;
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            exit(1);
        } else {
            // Assume this is the input file
            config.input_path = arg;
        }
    }
    
    // Check that we have an input file
    if (config.input_path.empty()) {
        std::cerr << "Error: No input file specified." << std::endl;
        print_usage(argv[0]);
        exit(1);
    }
    
    // If no output path was specified, use input path + .json
    if (config.output_path.empty()) {
        config.output_path = config.input_path + ".json";
    }
    
    return config;
}

int main(int argc, char** argv) {
    CliConfig config = parse_arguments(argc, argv);
    
    try {
        if (config.debug_mode) {
            std::cout << "*** ITCH 5.0 Parser Debug Mode: ON ***" << std::endl;
            std::cout << "*** Message limit: " << (config.message_limit > 0 ? std::to_string(config.message_limit) : "No limit") << " ***" << std::endl;
        }
        
        // Create the parser
        std::unique_ptr<itch::Parser> parser;
        
        // Check if the file is gzipped
        std::ifstream file(config.input_path, std::ios::binary);
        if (!file) {
            std::cerr << "Cannot open file: " << config.input_path << std::endl;
            return 1;
        }
        
        if (config.debug_mode) {
            std::cout << "Opening file: " << config.input_path << std::endl;
        }
        
        char header[2];
        file.read(header, 2);
        file.close();
        
        const bool is_gzipped = (header[0] == 0x1f && header[1] == 0x8b);
        
        try {
            if (is_gzipped) {
                std::cout << "Detected gzipped file. Processing..." << std::endl;
                parser = itch::Parser::from_gzip(config.input_path);
            } else {
                std::cout << "Processing raw ITCH file..." << std::endl;
                parser = itch::Parser::from_file(config.input_path);
            }
            
            if (config.debug_mode) {
                std::cout << "Parser initialized successfully." << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error initializing parser: " << e.what() << std::endl;
            return 1;
        }
        
        // Prepare output stream (file or stdout)
        std::ofstream output_file;
        std::ostream* output_stream = nullptr;
        
        if (config.output_to_stdout) {
            output_stream = &std::cout;
        } else {
            output_file.open(config.output_path, std::ios::binary);
            if (!output_file) {
                std::cerr << "Cannot open output file: " << config.output_path << std::endl;
                return 1;
            }
            output_stream = &output_file;
            
            if (config.debug_mode) {
                std::cout << "Output file created: " << config.output_path << std::endl;
            }
        }
        
        // Write JSON array start
        *output_stream << "[";
        
        // Set up message counters for statistics
        std::map<uint8_t, size_t> message_type_counts;
        size_t message_count = 0;
        bool first_message = true;
        auto start_time = std::chrono::high_resolution_clock::now();
        size_t start_memory = get_memory_usage();
        
        while (auto message = parser->parse_message()) {
            if (!first_message) {
                *output_stream << ",\n";
            } else {
                first_message = false;
                if (config.debug_mode) {
                    std::cout << "First message parsed successfully." << std::endl;
                }
            }
            
            // Convert to JSON and write to output
            nlohmann::json json_message = itch::JsonSerializer::to_json(*message);
            std::string json_str = json_message.dump();
            *output_stream << json_str;
            
            // Update message counters
            message_count++;
            message_type_counts[message->tag]++;
            
            if (config.debug_mode && message_count <= 5) {
                // Print first few messages for debugging
                std::cout << "Message " << message_count << ": " << json_str << std::endl;
            }
            
            // Print progress (less frequently for large datasets)
            size_t progress_interval = config.message_limit > 1000000 ? 100000 : 10000;
            if (message_count % progress_interval == 0) {
                std::cout << "Processed " << message_count << " messages..." << std::endl;
            }
            
            // Stop if we've reached the message limit
            if (config.message_limit > 0 && message_count >= config.message_limit) {
                std::cout << "Reached message limit of " << config.message_limit << ". Stopping." << std::endl;
                break;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        size_t end_memory = get_memory_usage();
        size_t memory_used = end_memory - start_memory;
        
        // Write JSON array end
        *output_stream << "]";
        
        if (!config.output_to_stdout) {
            output_file.close();
        }
        
        // Print summary statistics
        std::cout << "Successfully processed " << message_count << " messages." << std::endl;
        std::cout << "Output written to: " << config.output_path << std::endl;
        
        // Print processing performance statistics
        std::cout << "\nPerformance metrics:" << std::endl;
        std::cout << "-------------------" << std::endl;
        std::cout << "Processing time: " << (duration / 1000.0) << " seconds" << std::endl;
        std::cout << "Throughput: " << std::fixed << std::setprecision(2) 
                  << (message_count * 1000.0 / duration) << " messages/second" << std::endl;
        std::cout << "Memory usage: " << (memory_used / 1024.0) << " MB" << std::endl;
        
        // Print detailed message type statistics if requested
        if (config.show_stats) {
            std::cout << "\nMessage type statistics:" << std::endl;
            std::cout << "---------------------" << std::endl;
            for (const auto& [type, count] : message_type_counts) {
                // Get message type name
                std::string type_name;
                switch (type) {
                    case 'S': type_name = "System Event"; break;
                    case 'R': type_name = "Stock Directory"; break;
                    case 'H': type_name = "Stock Trading Action"; break;
                    case 'Y': type_name = "Reg SHO Restriction"; break;
                    case 'L': type_name = "Market Participant Position"; break;
                    case 'V': type_name = "MWCB Decline Level"; break;
                    case 'W': type_name = "MWCB Breach"; break;
                    case 'K': type_name = "IPO Quoting Period Update"; break;
                    case 'A': type_name = "Add Order"; break;
                    case 'F': type_name = "Add Order with MPID"; break;
                    case 'E': type_name = "Order Executed"; break;
                    case 'C': type_name = "Order Executed with Price"; break;
                    case 'X': type_name = "Order Cancel"; break;
                    case 'D': type_name = "Order Delete"; break;
                    case 'U': type_name = "Order Replace"; break;
                    case 'P': type_name = "Trade Message"; break;
                    case 'Q': type_name = "Cross Trade"; break;
                    case 'B': type_name = "Broken Trade"; break;
                    case 'I': type_name = "NOII"; break;
                    case 'N': type_name = "RPII"; break;
                    default: type_name = "Unknown Type " + std::to_string(type); break;
                }
                std::cout << type_name << ": " << count << " (" 
                          << (count * 100.0 / message_count) << "%)" << std::endl;
            }
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}