#include "../include/market_data_processor.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <chrono>
#include <thread>

void printUsage() {
    std::cout << "Market Data Processor CLI\n";
    std::cout << "-------------------------\n";
    std::cout << "Commands:\n";
    std::cout << "  load <file_path>           - Load ITCH 5.0 JSON data from file\n";
    std::cout << "  process <count>            - Process specified number of messages\n";
    std::cout << "  process_all                - Process all remaining messages\n";
    std::cout << "  stats                      - Show processing statistics\n";
    std::cout << "  symbols                    - List all symbols in the data\n";
    std::cout << "  book <symbol>              - Show order book for a symbol\n";
    std::cout << "  top_active <count>         - Show most active symbols by volume\n";
    std::cout << "  top_gainers <count>        - Show top gaining symbols\n";
    std::cout << "  top_losers <count>         - Show top losing symbols\n";
    std::cout << "  reset                      - Reset processor state\n";
    std::cout << "  help                       - Show this help message\n";
    std::cout << "  exit                       - Exit the program\n";
}

void printOrderBook(const std::shared_ptr<market_data::OrderBook>& orderBook) {
    if (!orderBook) {
        std::cout << "Order book not found\n";
        return;
    }
    
    std::cout << "Order Book for " << orderBook->getSymbol() << "\n";
    std::cout << "----------------------------\n";
    
    // Print some basic stats
    std::cout << "Best Bid: " << orderBook->getBestBid() << "\n";
    std::cout << "Best Ask: " << orderBook->getBestAsk() << "\n";
    std::cout << "Mid Price: " << orderBook->getMidPrice() << "\n";
    std::cout << "Spread: " << orderBook->getSpread() << "\n";
    std::cout << "Bid Depth: " << orderBook->getDepth(true) << " levels\n";
    std::cout << "Ask Depth: " << orderBook->getDepth(false) << " levels\n";
    std::cout << "Total Bid Volume: " << orderBook->getTotalVolume(true) << "\n";
    std::cout << "Total Ask Volume: " << orderBook->getTotalVolume(false) << "\n";
    
    // Print top levels
    std::cout << "\nBid Levels:\n";
    std::cout << "Price\t\tVolume\n";
    for (const auto& level : orderBook->getBidLevels()) {
        std::cout << level.price << "\t\t" << level.totalVolume << "\n";
    }
    
    std::cout << "\nAsk Levels:\n";
    std::cout << "Price\t\tVolume\n";
    for (const auto& level : orderBook->getAskLevels()) {
        std::cout << level.price << "\t\t" << level.totalVolume << "\n";
    }
}

void printMarketStats(const std::shared_ptr<market_data::MarketStats>& marketStats, const std::string& symbol) {
    if (!marketStats) {
        std::cout << "Market stats not available\n";
        return;
    }
    
    std::cout << "Market Statistics for " << symbol << "\n";
    std::cout << "----------------------------\n";
    std::cout << "Open: " << marketStats->getOpenPrice(symbol) << "\n";
    std::cout << "High: " << marketStats->getHighPrice(symbol) << "\n";
    std::cout << "Low: " << marketStats->getLowPrice(symbol) << "\n";
    std::cout << "Last: " << marketStats->getLastPrice(symbol) << "\n";
    std::cout << "Change: " << marketStats->getPriceChange(symbol) << " (" 
              << std::fixed << std::setprecision(2) << marketStats->getPriceChangePercent(symbol) << "%)\n";
    std::cout << "Volume: " << marketStats->getVolume(symbol) << "\n";
    std::cout << "VWAP: " << marketStats->getVWAP(symbol) << "\n";
    std::cout << "Trade Count: " << marketStats->getTradeCount(symbol) << "\n";
    std::cout << "Order Imbalance: " << std::fixed << std::setprecision(2) 
              << marketStats->getOrderImbalance(symbol) * 100 << "%\n";
    std::cout << "Trade Imbalance: " << std::fixed << std::setprecision(2) 
              << marketStats->getTradeImbalance(symbol) * 100 << "%\n";
    std::cout << "Volatility (20 trades): " << std::fixed << std::setprecision(4) 
              << marketStats->getVolatility(symbol) * 100 << "%\n";
}

int main(int argc, char* argv[]) {
    market_data::MarketDataProcessor processor;
    std::string command;
    std::string filePath;
    
    std::cout << "Market Data Processor CLI\n";
    std::cout << "Type 'help' for available commands\n";
    
    // If a file path is provided as a command-line argument, load it
    if (argc > 1) {
        filePath = argv[1];
        std::cout << "Loading data from " << filePath << "...\n";
        if (processor.loadDataFromFile(filePath)) {
            std::cout << "Data loaded successfully\n";
        } else {
            std::cout << "Failed to load data\n";
        }
    }
    
    while (true) {
        std::cout << "> ";
        std::string line;
        std::getline(std::cin, line);
        
        // Parse command and arguments
        std::istringstream iss(line);
        iss >> command;
        
        if (command == "exit") {
            break;
        } else if (command == "help") {
            printUsage();
        } else if (command == "load") {
            iss >> filePath;
            if (filePath.empty()) {
                std::cout << "Please provide a file path\n";
                continue;
            }
            
            std::cout << "Loading data from " << filePath << "...\n";
            auto start = std::chrono::high_resolution_clock::now();
            
            if (processor.loadDataFromFile(filePath)) {
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                std::cout << "Data loaded successfully in " << duration << " ms\n";
            } else {
                std::cout << "Failed to load data\n";
            }
        } else if (command == "process") {
            size_t count;
            iss >> count;
            if (count == 0) {
                std::cout << "Please provide a valid message count\n";
                continue;
            }
            
            std::cout << "Processing " << count << " messages...\n";
            auto start = std::chrono::high_resolution_clock::now();
            
            size_t processed = processor.processBatch(count);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            std::cout << "Processed " << processed << " messages in " << duration << " ms\n";
        } else if (command == "process_all") {
            std::cout << "Processing all remaining messages...\n";
            auto start = std::chrono::high_resolution_clock::now();
            
            size_t totalProcessed = 0;
            size_t batchSize = 10000;
            size_t processed;
            
            do {
                processed = processor.processBatch(batchSize);
                totalProcessed += processed;
                
                // Print progress every 100,000 messages
                if (totalProcessed % 100000 == 0 && totalProcessed > 0) {
                    std::cout << "Processed " << totalProcessed << " messages so far...\n";
                }
            } while (processed > 0);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            std::cout << "Processed " << totalProcessed << " messages in " << duration << " ms\n";
        } else if (command == "stats") {
            std::cout << "Processing Statistics\n";
            std::cout << "--------------------\n";
            std::cout << "Total Messages Processed: " << processor.getTotalMessagesProcessed() << "\n";
            std::cout << "Message Type Counts:\n";
            std::cout << "  Add Order: " << processor.getMessagesByType("AddOrder") << "\n";
            std::cout << "  Delete Order: " << processor.getMessagesByType("DeleteOrder") << "\n";
            std::cout << "  Replace Order: " << processor.getMessagesByType("ReplaceOrder") << "\n";
            std::cout << "  Order Executed: " << processor.getMessagesByType("OrderExecuted") << "\n";
            std::cout << "  Order Executed With Price: " << processor.getMessagesByType("OrderExecutedWithPrice") << "\n";
            std::cout << "  Order Cancelled: " << processor.getMessagesByType("OrderCancelled") << "\n";
            std::cout << "  Non-Cross Trade: " << processor.getMessagesByType("NonCrossTrade") << "\n";
            std::cout << "  Cross Trade: " << processor.getMessagesByType("CrossTrade") << "\n";
            std::cout << "  Other: " << processor.getMessagesByType("Other") << "\n";
        } else if (command == "symbols") {
            std::vector<std::string> symbols = processor.getAllSymbols();
            std::cout << "Symbols (" << symbols.size() << "):\n";
            for (const auto& symbol : symbols) {
                std::cout << symbol << "\n";
            }
        } else if (command == "book") {
            std::string symbol;
            iss >> symbol;
            if (symbol.empty()) {
                std::cout << "Please provide a symbol\n";
                continue;
            }
            
            auto orderBook = processor.getOrderBook(symbol);
            printOrderBook(orderBook);
            
            // Also print market stats for this symbol
            auto marketStats = processor.getMarketStats();
            printMarketStats(marketStats, symbol);
        } else if (command == "top_active") {
            size_t count = 10;
            iss >> count;
            
            auto marketStats = processor.getMarketStats();
            auto topActive = marketStats->getMostActiveSymbols(count);
            
            std::cout << "Top " << count << " Most Active Symbols\n";
            std::cout << "Symbol\tVolume\n";
            for (const auto& pair : topActive) {
                std::cout << pair.first << "\t" << pair.second << "\n";
            }
        } else if (command == "top_gainers") {
            size_t count = 10;
            iss >> count;
            
            auto marketStats = processor.getMarketStats();
            auto topGainers = marketStats->getTopGainers(count);
            
            std::cout << "Top " << count << " Gainers\n";
            std::cout << "Symbol\tChange %\n";
            for (const auto& pair : topGainers) {
                std::cout << pair.first << "\t" << std::fixed << std::setprecision(2) << pair.second << "%\n";
            }
        } else if (command == "top_losers") {
            size_t count = 10;
            iss >> count;
            
            auto marketStats = processor.getMarketStats();
            auto topLosers = marketStats->getTopLosers(count);
            
            std::cout << "Top " << count << " Losers\n";
            std::cout << "Symbol\tChange %\n";
            for (const auto& pair : topLosers) {
                std::cout << pair.first << "\t" << std::fixed << std::setprecision(2) << pair.second << "%\n";
            }
        } else if (command == "reset") {
            processor.reset();
            std::cout << "Processor state reset\n";
        } else {
            std::cout << "Unknown command. Type 'help' for available commands\n";
        }
    }
    
    return 0;
}
