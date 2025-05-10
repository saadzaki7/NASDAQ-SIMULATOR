#pragma once

#include "message.h"
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>

namespace itch {

class Parser {
private:
    static constexpr size_t BUFFER_SIZE = 8 * 1024; 
    std::vector<uint8_t> buffer;
    size_t current_pos = 0;
    size_t bytes_read = 0;
    std::unique_ptr<std::istream> stream;
    bool is_end_of_stream = false;
    
    uint8_t read_u8();
    uint16_t read_u16();
    uint32_t read_u32();
    uint64_t read_u64();
    uint64_t read_u48();
    void read_bytes(void* data, size_t len);
    bool fetch_more_bytes();
    ArrayString8 read_stock();
    ArrayString4 read_array_string4();
    bool parse_char_to_bool();
    std::optional<bool> parse_maybe_char_to_bool();
    std::optional<bool> parse_etp_flag();
    
    StockDirectory parse_stock_directory();
    SystemEvent parse_system_event();
    TradingAction parse_trading_action();
    RegShoRestriction parse_reg_sho_restriction();
    MarketParticipantPosition parse_participant_position();
    AddOrder parse_add_order(bool with_mpid);
    OrderExecuted parse_order_executed();
    OrderExecutedWithPrice parse_order_executed_with_price();
    OrderCancelled parse_order_cancelled();
    ReplaceOrder parse_replace_order();
    DeleteOrder parse_delete_order();
    BrokenTrade parse_broken_trade();
    NonCrossTrade parse_noncross_trade();
    CrossTrade parse_cross_trade();
    ImbalanceIndicator parse_imbalance_indicator();
    RetailPriceImprovementIndicator parse_retail_price_improvement_indicator();
    IpoQuotingPeriod parse_ipo_quoting_period();
    LULDAuctionCollar parse_luld_auction_collar();
    MwcbDeclineLevel parse_mwcb_decline_level();
    LevelBreached parse_breach();

public:
    explicit Parser(std::unique_ptr<std::istream> stream);
    
    std::optional<Message> parse_message();
    
    void reset();
    
    bool eof() const;
    
    static std::unique_ptr<Parser> from_file(const std::string& path);
    
    static std::unique_ptr<Parser> from_gzip(const std::string& path);
};

}
