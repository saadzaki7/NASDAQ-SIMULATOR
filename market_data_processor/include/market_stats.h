#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <deque>
#include <utility>

namespace market_data {

/**
 * MarketStats - Tracks various market statistics and signals
 * 
 * This class calculates and maintains market statistics that can be used
 * for trading strategies and dashboard visualization.
 */
class MarketStats {
public:
    MarketStats();
    ~MarketStats();
    
    // Update statistics with a new trade
    void updateWithTrade(const std::string& symbol, double price, uint32_t volume, 
                         uint64_t timestamp, bool isBuySideAggressor);
    
    // Update statistics with a new order
    void updateWithOrder(const std::string& symbol, double price, uint32_t volume, 
                         bool isBuy, uint64_t timestamp);
    
    // Update statistics with an order cancellation
    void updateWithCancel(const std::string& symbol, double price, uint32_t volume, 
                          bool isBuy, uint64_t timestamp);
    
    // Get volume weighted average price (VWAP)
    double getVWAP(const std::string& symbol) const;
    
    // Get trading volume
    uint64_t getVolume(const std::string& symbol) const;
    
    // Get number of trades
    uint64_t getTradeCount(const std::string& symbol) const;
    
    // Get high price
    double getHighPrice(const std::string& symbol) const;
    
    // Get low price
    double getLowPrice(const std::string& symbol) const;
    
    // Get opening price
    double getOpenPrice(const std::string& symbol) const;
    
    // Get last price
    double getLastPrice(const std::string& symbol) const;
    
    // Get price change (from open)
    double getPriceChange(const std::string& symbol) const;
    
    // Get price change percentage
    double getPriceChangePercent(const std::string& symbol) const;
    
    // Get order imbalance (buy volume - sell volume) / (buy volume + sell volume)
    double getOrderImbalance(const std::string& symbol) const;
    
    // Get trade imbalance (buy trades - sell trades) / (buy trades + sell trades)
    double getTradeImbalance(const std::string& symbol) const;
    
    // Get recent price volatility (standard deviation of recent price changes)
    double getVolatility(const std::string& symbol, size_t lookbackPeriod = 20) const;
    
    // Get list of most active symbols by volume
    std::vector<std::pair<std::string, uint64_t>> getMostActiveSymbols(size_t count = 10) const;
    
    // Get list of symbols with highest price change percentage
    std::vector<std::pair<std::string, double>> getTopGainers(size_t count = 10) const;
    
    // Get list of symbols with lowest price change percentage
    std::vector<std::pair<std::string, double>> getTopLosers(size_t count = 10) const;
    
    // Reset all statistics
    void reset();

private:
    struct SymbolStats {
        double open;                 // Opening price
        double high;                 // Highest price
        double low;                  // Lowest price
        double last;                 // Last trade price
        uint64_t volume;             // Total volume
        uint64_t tradeCount;         // Number of trades
        double volumeWeightedSum;    // Sum of (price * volume) for VWAP calculation
        uint64_t buyVolume;          // Buy volume
        uint64_t sellVolume;         // Sell volume
        uint64_t buyTradeCount;      // Number of buy trades
        uint64_t sellTradeCount;     // Number of sell trades
        std::deque<double> recentPrices; // Recent prices for volatility calculation
    };
    
    // Statistics for each symbol
    std::unordered_map<std::string, SymbolStats> symbolStats;
    
    // Helper methods
    void initializeSymbolStats(const std::string& symbol, double price);
    double calculateVolatility(const std::deque<double>& prices) const;
};

} // namespace market_data
