#include "../include/parser.h"
#include "../include/json_serializer.h"
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

// Configuration
const bool DEBUG_MODE = true;
const size_t MESSAGE_LIMIT = 50000;

void print_usage(const std::string& program_name) {
    std::cerr << "Usage: " << program_name << " <path-to-itch-file>" << std::endl;
    std::cerr << "Parses NASDAQ ITCH 5.0 file and outputs JSON to <input-file>.json" << std::endl;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string input_path = argv[1];
    std::string output_path = input_path + ".json";
    
    try {
        std::cout << "*** ITCH 5.0 Parser Debug Mode: " << (DEBUG_MODE ? "ON" : "OFF") << " ***" << std::endl;
        std::cout << "*** Message limit: " << (MESSAGE_LIMIT > 0 ? std::to_string(MESSAGE_LIMIT) : "No limit") << " ***" << std::endl;
        
        // Create the parser
        std::unique_ptr<itch::Parser> parser;
        
        // Check if the file is gzipped
        std::ifstream file(input_path, std::ios::binary);
        if (!file) {
            std::cerr << "Cannot open file: " << input_path << std::endl;
            return 1;
        }
        
        if (DEBUG_MODE) {
            std::cout << "Opening file: " << input_path << std::endl;
        }
        
        char header[2];
        file.read(header, 2);
        file.close();
        
        const bool is_gzipped = (header[0] == 0x1f && header[1] == 0x8b);
        
        try {
            if (is_gzipped) {
                std::cout << "Detected gzipped file. Processing..." << std::endl;
                parser = itch::Parser::from_gzip(input_path);
            } else {
                std::cout << "Processing raw ITCH file..." << std::endl;
                parser = itch::Parser::from_file(input_path);
            }
            
            if (DEBUG_MODE) {
                std::cout << "Parser initialized successfully." << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error initializing parser: " << e.what() << std::endl;
            return 1;
        }
        
        // Open output file
        std::ofstream output(output_path, std::ios::binary);
        if (!output) {
            std::cerr << "Cannot open output file: " << output_path << std::endl;
            return 1;
        }
        
        if (DEBUG_MODE) {
            std::cout << "Output file created: " << output_path << std::endl;
        }
        
        // Write JSON array start
        output << "[";
        
        // Process messages
        size_t message_count = 0;
        bool first_message = true;
        
        while (auto message = parser->parse_message()) {
            if (!first_message) {
                output << ",\n";
            } else {
                first_message = false;
                if (DEBUG_MODE) {
                    std::cout << "First message parsed successfully." << std::endl;
                }
            }
            
            // Convert to JSON and write to file
            nlohmann::json json_message = itch::JsonSerializer::to_json(*message);
            std::string json_str = json_message.dump();
            output << json_str;
            
            message_count++;
            
            if (DEBUG_MODE && message_count <= 5) {
                // Print first few messages for debugging
                std::cout << "Message " << message_count << ": " << json_str << std::endl;
            }
            
            // Print progress
            if (message_count % 10 == 0) {
                std::cout << "Processed " << message_count << " messages..." << std::endl;
            }
            
            // Stop if we've reached the message limit
            if (MESSAGE_LIMIT > 0 && message_count >= MESSAGE_LIMIT) {
                std::cout << "Reached message limit of " << MESSAGE_LIMIT << ". Stopping." << std::endl;
                break;
            }
        }
        
        // Write JSON array end
        output << "]";
        output.close();
        
        std::cout << "Successfully processed " << message_count << " messages." << std::endl;
        std::cout << "Output written to: " << output_path << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
