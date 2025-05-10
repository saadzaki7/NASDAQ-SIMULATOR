#include "../include/market_stats.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <iostream>

namespace market_data {

MarketStats::MarketStats() {
}

MarketStats::~MarketStats() {
    // Clean up resources
}

void MarketStats::updateWithTrade(const std::string& symbol, double price, uint32_t volume, 
                                 uint64_t timestamp, bool isBuySideAggressor) {
    // Initialize stats for this symbol if it doesn't exist
    if (symbolStats.find(symbol) == symbolStats.end()) {
        initializeSymbolStats(symbol, price);
    }
    
    SymbolStats& stats = symbolStats[symbol];
    
    // Update high/low prices
    if (price > stats.high) {
        stats.high = price;
    }
    if (price < stats.low) {
        stats.low = price;
    }
    
    // Update last price
    stats.last = price;
    
    // Update volume
    stats.volume += volume;
    stats.tradeCount++;
    
    // Update volume-weighted sum for VWAP calculation
    stats.volumeWeightedSum += price * volume;
    
    // Update buy/sell volume and trade count
    if (isBuySideAggressor) {
        stats.buyVolume += volume;
        stats.buyTradeCount++;
    } else {
        stats.sellVolume += volume;
        stats.sellTradeCount++;
    }
    
    // Update recent prices for volatility calculation
    stats.recentPrices.push_back(price);
    if (stats.recentPrices.size() > 100) { // Keep a maximum of 100 recent prices
        stats.recentPrices.pop_front();
    }
}

void MarketStats::updateWithOrder(const std::string& symbol, double price, uint32_t volume, 
                                 bool isBuy, uint64_t timestamp) {
    // Initialize stats for this symbol if it doesn't exist
    if (symbolStats.find(symbol) == symbolStats.end()) {
        initializeSymbolStats(symbol, price);
    }
    
    // For now, we don't track anything specific for new orders
    // This could be extended to track order flow metrics
}

void MarketStats::updateWithCancel(const std::string& symbol, double price, uint32_t volume, 
                                  bool isBuy, uint64_t timestamp) {
    // Initialize stats for this symbol if it doesn't exist
    if (symbolStats.find(symbol) == symbolStats.end()) {
        initializeSymbolStats(symbol, price);
    }
    
    // For now, we don't track anything specific for cancellations
    // This could be extended to track cancellation metrics
}

double MarketStats::getVWAP(const std::string& symbol) const {
    auto it = symbolStats.find(symbol);
    if (it == symbolStats.end() || it->second.volume == 0) {
        return 0.0;
    }
    
    return it->second.volumeWeightedSum / it->second.volume;
}

uint64_t MarketStats::getVolume(const std::string& symbol) const {
    auto it = symbolStats.find(symbol);
    if (it == symbolStats.end()) {
        return 0;
    }
    
    return it->second.volume;
}

uint64_t MarketStats::getTradeCount(const std::string& symbol) const {
    auto it = symbolStats.find(symbol);
    if (it == symbolStats.end()) {
        return 0;
    }
    
    return it->second.tradeCount;
}

double MarketStats::getHighPrice(const std::string& symbol) const {
    auto it = symbolStats.find(symbol);
    if (it == symbolStats.end()) {
        return 0.0;
    }
    
    return it->second.high;
}

double MarketStats::getLowPrice(const std::string& symbol) const {
    auto it = symbolStats.find(symbol);
    if (it == symbolStats.end()) {
        return 0.0;
    }
    
    return it->second.low;
}

double MarketStats::getOpenPrice(const std::string& symbol) const {
    auto it = symbolStats.find(symbol);
    if (it == symbolStats.end()) {
        return 0.0;
    }
    
    return it->second.open;
}

double MarketStats::getLastPrice(const std::string& symbol) const {
    auto it = symbolStats.find(symbol);
    if (it == symbolStats.end()) {
        return 0.0;
    }
    
    return it->second.last;
}

double MarketStats::getPriceChange(const std::string& symbol) const {
    auto it = symbolStats.find(symbol);
    if (it == symbolStats.end()) {
        return 0.0;
    }
    
    return it->second.last - it->second.open;
}

double MarketStats::getPriceChangePercent(const std::string& symbol) const {
    auto it = symbolStats.find(symbol);
    if (it == symbolStats.end() || it->second.open == 0.0) {
        return 0.0;
    }
    
    return (it->second.last - it->second.open) / it->second.open * 100.0;
}

double MarketStats::getOrderImbalance(const std::string& symbol) const {
    auto it = symbolStats.find(symbol);
    if (it == symbolStats.end()) {
        return 0.0;
    }
    
    uint64_t buyVolume = it->second.buyVolume;
    uint64_t sellVolume = it->second.sellVolume;
    
    if (buyVolume + sellVolume == 0) {
        return 0.0;
    }
    
    return static_cast<double>(buyVolume - sellVolume) / (buyVolume + sellVolume);
}

double MarketStats::getTradeImbalance(const std::string& symbol) const {
    auto it = symbolStats.find(symbol);
    if (it == symbolStats.end()) {
        return 0.0;
    }
    
    uint64_t buyTrades = it->second.buyTradeCount;
    uint64_t sellTrades = it->second.sellTradeCount;
    
    if (buyTrades + sellTrades == 0) {
        return 0.0;
    }
    
    return static_cast<double>(buyTrades - sellTrades) / (buyTrades + sellTrades);
}

double MarketStats::getVolatility(const std::string& symbol, size_t lookbackPeriod) const {
    auto it = symbolStats.find(symbol);
    if (it == symbolStats.end()) {
        return 0.0;
    }
    
    const std::deque<double>& prices = it->second.recentPrices;
    
    if (prices.size() < 2) {
        return 0.0;
    }
    
    // Use the minimum of the available prices and the requested lookback period
    size_t period = std::min(lookbackPeriod, prices.size());
    
    // Get the last 'period' prices
    std::deque<double> recentPrices;
    auto startIt = prices.end() - period;
    recentPrices.insert(recentPrices.end(), startIt, prices.end());
    
    return calculateVolatility(recentPrices);
}

std::vector<std::pair<std::string, uint64_t>> MarketStats::getMostActiveSymbols(size_t count) const {
    std::vector<std::pair<std::string, uint64_t>> result;
    result.reserve(symbolStats.size());
    
    for (const auto& pair : symbolStats) {
        result.emplace_back(pair.first, pair.second.volume);
    }
    
    // Sort by volume in descending order
    std::sort(result.begin(), result.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Limit to requested count
    if (result.size() > count) {
        result.resize(count);
    }
    
    return result;
}

std::vector<std::pair<std::string, double>> MarketStats::getTopGainers(size_t count) const {
    std::vector<std::pair<std::string, double>> result;
    result.reserve(symbolStats.size());
    
    for (const auto& pair : symbolStats) {
        double changePercent = getPriceChangePercent(pair.first);
        result.emplace_back(pair.first, changePercent);
    }
    
    // Sort by price change percentage in descending order
    std::sort(result.begin(), result.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Limit to requested count
    if (result.size() > count) {
        result.resize(count);
    }
    
    return result;
}

std::vector<std::pair<std::string, double>> MarketStats::getTopLosers(size_t count) const {
    std::vector<std::pair<std::string, double>> result;
    result.reserve(symbolStats.size());
    
    for (const auto& pair : symbolStats) {
        double changePercent = getPriceChangePercent(pair.first);
        result.emplace_back(pair.first, changePercent);
    }
    
    // Sort by price change percentage in ascending order
    std::sort(result.begin(), result.end(), 
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    // Limit to requested count
    if (result.size() > count) {
        result.resize(count);
    }
    
    return result;
}

void MarketStats::reset() {
    symbolStats.clear();
}

// Private helper methods

void MarketStats::initializeSymbolStats(const std::string& symbol, double price) {
    SymbolStats stats;
    stats.open = price;
    stats.high = price;
    stats.low = price;
    stats.last = price;
    stats.volume = 0;
    stats.tradeCount = 0;
    stats.volumeWeightedSum = 0.0;
    stats.buyVolume = 0;
    stats.sellVolume = 0;
    stats.buyTradeCount = 0;
    stats.sellTradeCount = 0;
    
    symbolStats[symbol] = stats;
}

double MarketStats::calculateVolatility(const std::deque<double>& prices) const {
    if (prices.size() < 2) {
        return 0.0;
    }
    
    // Calculate returns
    std::vector<double> returns;
    returns.reserve(prices.size() - 1);
    
    for (size_t i = 1; i < prices.size(); ++i) {
        double ret = (prices[i] / prices[i-1]) - 1.0;
        returns.push_back(ret);
    }
    
    // Calculate mean
    double sum = 0.0;
    for (double ret : returns) {
        sum += ret;
    }
    double mean = sum / returns.size();
    
    // Calculate variance
    double variance = 0.0;
    for (double ret : returns) {
        double diff = ret - mean;
        variance += diff * diff;
    }
    variance /= returns.size();
    
    // Return standard deviation
    return std::sqrt(variance);
}

} // namespace market_data
