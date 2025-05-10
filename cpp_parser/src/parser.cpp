#include "../include/parser.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <zlib.h>

namespace itch {

Parser::Parser(std::unique_ptr<std::istream> stream)
    : stream(std::move(stream)), buffer(BUFFER_SIZE) {
    // Initialize the buffer
    fetch_more_bytes();
}

std::unique_ptr<Parser> Parser::from_file(const std::string& path) {
    auto file_stream = std::make_unique<std::ifstream>(path, std::ios::binary);
    if (!file_stream->is_open()) {
        throw std::runtime_error("Could not open file: " + path);
    }
    return std::make_unique<Parser>(std::move(file_stream));
}

std::unique_ptr<Parser> Parser::from_gzip(const std::string& path) {
    // Open the gzipped file
    auto file = std::make_unique<std::ifstream>(path, std::ios::binary);
    if (!file->is_open()) {
        throw std::runtime_error("Could not open gzipped file: " + path);
    }
    
    // Gzip decompression not implemented in current version
    throw std::runtime_error("Gzip decompression not available - please use uncompressed ITCH files");
}

bool Parser::eof() const {
    return is_end_of_stream && current_pos >= bytes_read;
}

void Parser::reset() {
    current_pos = 0;
    bytes_read = 0;
    is_end_of_stream = false;
    
    if (stream) {
        // Reset the stream to the beginning
        stream->clear();
        stream->seekg(0, std::ios::beg);
        fetch_more_bytes();
    }
}

bool Parser::fetch_more_bytes() {
    if (is_end_of_stream) {
        return false;
    }
    
    // If we've consumed more than half the buffer, move the remaining data to the beginning
    if (current_pos > BUFFER_SIZE / 2) {
        const size_t remaining = bytes_read - current_pos;
        if (remaining > 0) {
            std::memcpy(buffer.data(), buffer.data() + current_pos, remaining);
        }
        bytes_read = remaining;
        current_pos = 0;
    }
    
    // Fill the rest of the buffer
    const size_t bytes_to_read = BUFFER_SIZE - bytes_read;
    if (bytes_to_read > 0) {
        stream->read(reinterpret_cast<char*>(buffer.data() + bytes_read), bytes_to_read);
        const size_t read_count = stream->gcount();
        bytes_read += read_count;
        
        if (read_count < bytes_to_read) {
            is_end_of_stream = true;
        }
    }
    
    return bytes_read > current_pos;
}

uint8_t Parser::read_u8() {
    if (current_pos + sizeof(uint8_t) > bytes_read) {
        if (!fetch_more_bytes() || current_pos + sizeof(uint8_t) > bytes_read) {
            throw std::runtime_error("Unexpected end of stream while reading uint8");
        }
    }
    
    uint8_t value = buffer[current_pos];
    current_pos += sizeof(uint8_t);
    return value;
}

uint16_t Parser::read_u16() {
    if (current_pos + sizeof(uint16_t) > bytes_read) {
        if (!fetch_more_bytes() || current_pos + sizeof(uint16_t) > bytes_read) {
            throw std::runtime_error("Unexpected end of stream while reading uint16");
        }
    }
    
    // Read big-endian 16-bit value
    const uint16_t value = static_cast<uint16_t>(buffer[current_pos]) << 8 |
                          static_cast<uint16_t>(buffer[current_pos + 1]);
    current_pos += sizeof(uint16_t);
    return value;
}

uint32_t Parser::read_u32() {
    if (current_pos + sizeof(uint32_t) > bytes_read) {
        if (!fetch_more_bytes() || current_pos + sizeof(uint32_t) > bytes_read) {
            throw std::runtime_error("Unexpected end of stream while reading uint32");
        }
    }
    
    // Read big-endian 32-bit value
    const uint32_t value = static_cast<uint32_t>(buffer[current_pos]) << 24 |
                          static_cast<uint32_t>(buffer[current_pos + 1]) << 16 |
                          static_cast<uint32_t>(buffer[current_pos + 2]) << 8 |
                          static_cast<uint32_t>(buffer[current_pos + 3]);
    current_pos += sizeof(uint32_t);
    return value;
}

uint64_t Parser::read_u64() {
    if (current_pos + sizeof(uint64_t) > bytes_read) {
        if (!fetch_more_bytes() || current_pos + sizeof(uint64_t) > bytes_read) {
            throw std::runtime_error("Unexpected end of stream while reading uint64");
        }
    }
    
    // Read big-endian 64-bit value
    const uint64_t value = static_cast<uint64_t>(buffer[current_pos]) << 56 |
                          static_cast<uint64_t>(buffer[current_pos + 1]) << 48 |
                          static_cast<uint64_t>(buffer[current_pos + 2]) << 40 |
                          static_cast<uint64_t>(buffer[current_pos + 3]) << 32 |
                          static_cast<uint64_t>(buffer[current_pos + 4]) << 24 |
                          static_cast<uint64_t>(buffer[current_pos + 5]) << 16 |
                          static_cast<uint64_t>(buffer[current_pos + 6]) << 8 |
                          static_cast<uint64_t>(buffer[current_pos + 7]);
    current_pos += sizeof(uint64_t);
    return value;
}

uint64_t Parser::read_u48() {
    if (current_pos + 6 > bytes_read) {
        if (!fetch_more_bytes() || current_pos + 6 > bytes_read) {
            throw std::runtime_error("Unexpected end of stream while reading uint48");
        }
    }
    
    // Read big-endian 48-bit value
    const uint64_t value = static_cast<uint64_t>(buffer[current_pos]) << 40 |
                          static_cast<uint64_t>(buffer[current_pos + 1]) << 32 |
                          static_cast<uint64_t>(buffer[current_pos + 2]) << 24 |
                          static_cast<uint64_t>(buffer[current_pos + 3]) << 16 |
                          static_cast<uint64_t>(buffer[current_pos + 4]) << 8 |
                          static_cast<uint64_t>(buffer[current_pos + 5]);
    current_pos += 6;
    return value;
}

void Parser::read_bytes(void* data, size_t len) {
    if (current_pos + len > bytes_read) {
        if (!fetch_more_bytes() || current_pos + len > bytes_read) {
            throw std::runtime_error("Unexpected end of stream while reading bytes");
        }
    }
    
    std::memcpy(data, buffer.data() + current_pos, len);
    current_pos += len;
}

ArrayString8 Parser::read_stock() {
    ArrayString8 stock;
    read_bytes(stock.data(), stock.size());
    return stock;
}

ArrayString4 Parser::read_array_string4() {
    ArrayString4 str;
    read_bytes(str.data(), str.size());
    return str;
}

bool Parser::parse_char_to_bool() {
    char c = read_u8();
    return char_to_bool(c);
}

std::optional<bool> Parser::parse_maybe_char_to_bool() {
    char c = read_u8();
    return maybe_char_to_bool(c);
}

std::optional<bool> Parser::parse_etp_flag() {
    char c = read_u8();
    return maybe_char_to_bool(c);
}

std::optional<Message> Parser::parse_message() {
    if (eof()) {
        return std::nullopt;
    }
    
    try {
        uint16_t message_length = read_u16();
        uint8_t message_type = read_u8();
        uint16_t stock_locate = read_u16();
        uint16_t tracking_number = read_u16();
        uint64_t timestamp = read_u48();
        
        MessageBody body;
        
        switch (message_type) {
            case 'S': // System Event
                body = parse_system_event();
                break;
            case 'R': // Stock Directory
                body = parse_stock_directory();
                break;
            case 'H': // Trading Action
                body = parse_trading_action();
                break;
            case 'Y': // Reg SHO Short Sale Price Test Restricted Indicator
                body = parse_reg_sho_restriction();
                break;
            case 'L': // Market Participant Position
                body = parse_participant_position();
                break;
            case 'A': // Add Order (no MPID)
                body = parse_add_order(false);
                break;
            case 'F': // Add Order with MPID
                body = parse_add_order(true);
                break;
            case 'E': // Order Executed
                body = parse_order_executed();
                break;
            case 'C': // Order Executed With Price
                body = parse_order_executed_with_price();
                break;
            case 'X': // Order Cancel
                body = parse_order_cancelled();
                break;
            case 'D': // Order Delete
                body = parse_delete_order();
                break;
            case 'U': // Order Replace
                body = parse_replace_order();
                break;
            case 'P': // Trade (Non-Cross)
                body = parse_noncross_trade();
                break;
            case 'Q': // Cross Trade
                body = parse_cross_trade();
                break;
            case 'B': // Broken Trade
                body = parse_broken_trade();
                break;
            case 'I': // NOII (Imbalance)
                body = parse_imbalance_indicator();
                break;
            case 'N': // Retail Price Improvement Indicator
                body = parse_retail_price_improvement_indicator();
                break;
            case 'K': // IPO Quoting Period Update
                body = parse_ipo_quoting_period();
                break;
            case 'J': // LULD Auction Collar
                body = parse_luld_auction_collar();
                break;
            case 'V': // MWCB Decline Level
                body = parse_mwcb_decline_level();
                break;
            case 'W': // MWCB Breach
                body = parse_breach();
                break;
            default:
                throw std::runtime_error("Unknown message type: " + std::to_string(message_type));
        }
        
        return Message {
            message_type,
            stock_locate,
            tracking_number,
            timestamp,
            body
        };
    } catch (const std::runtime_error& e) {
        std::cerr << "Error parsing message: " << e.what() << std::endl;
        return std::nullopt;
    }
}

SystemEvent Parser::parse_system_event() {
    char event_code = read_u8();
    EventCode code;
    
    switch (event_code) {
        case 'O': code = EventCode::StartOfMessages; break;
        case 'S': code = EventCode::StartOfSystemHours; break;
        case 'Q': code = EventCode::StartOfMarketHours; break;
        case 'M': code = EventCode::EndOfMarketHours; break;
        case 'E': code = EventCode::EndOfSystemHours; break;
        case 'C': code = EventCode::EndOfMessages; break;
        default:
            throw std::runtime_error("Unknown system event code: " + std::string(1, event_code));
    }
    
    return SystemEvent { code };
}

StockDirectory Parser::parse_stock_directory() {
    auto stock = read_stock();
    
    char market_cat = read_u8();
    MarketCategory market_category;
    switch (market_cat) {
        case 'Q': market_category = MarketCategory::NasdaqGlobalSelect; break;
        case 'G': market_category = MarketCategory::NasdaqGlobalMarket; break;
        case 'S': market_category = MarketCategory::NasdaqCapitalMarket; break;
        case 'N': market_category = MarketCategory::Nyse; break;
        case 'A': market_category = MarketCategory::NyseMkt; break;
        case 'P': market_category = MarketCategory::NyseArca; break;
        case 'Z': market_category = MarketCategory::BatsZExchange; break;
        case 'V': market_category = MarketCategory::InvestorsExchange; break;
        case ' ': market_category = MarketCategory::Unavailable; break;
        default:
            throw std::runtime_error("Unknown market category: " + std::string(1, market_cat));
    }
    
    char fin_status = read_u8();
    FinancialStatus financial_status;
    switch (fin_status) {
        case 'N': financial_status = FinancialStatus::Normal; break;
        case 'D': financial_status = FinancialStatus::Deficient; break;
        case 'E': financial_status = FinancialStatus::Delinquent; break;
        case 'Q': financial_status = FinancialStatus::Bankrupt; break;
        case 'S': financial_status = FinancialStatus::Suspended; break;
        case 'G': financial_status = FinancialStatus::DeficientBankrupt; break;
        case 'H': financial_status = FinancialStatus::DeficientDelinquent; break;
        case 'J': financial_status = FinancialStatus::DelinquentBankrupt; break;
        case 'K': financial_status = FinancialStatus::DeficientDelinquentBankrupt; break;
        case 'C': financial_status = FinancialStatus::EtpSuspended; break;
        case ' ': financial_status = FinancialStatus::Unavailable; break;
        default:
            throw std::runtime_error("Unknown financial status: " + std::string(1, fin_status));
    }
    
    uint32_t round_lot_size = read_u32();
    bool round_lots_only = parse_char_to_bool();
    
    char issue_class = read_u8();
    IssueClassification issue_classification = parse_issue_classification(issue_class);
    
    char subtype[2];
    read_bytes(subtype, 2);
    IssueSubType issue_subtype = parse_issue_subtype(subtype);
    
    char auth = read_u8();
    bool authenticity = (auth == 'P');
    
    std::optional<bool> short_sale_threshold = parse_maybe_char_to_bool();
    std::optional<bool> ipo_flag = parse_maybe_char_to_bool();
    
    char luld_tier = read_u8();
    LuldRefPriceTier luld_ref_price_tier;
    switch (luld_tier) {
        case '1': luld_ref_price_tier = LuldRefPriceTier::Tier1; break;
        case '2': luld_ref_price_tier = LuldRefPriceTier::Tier2; break;
        case ' ': luld_ref_price_tier = LuldRefPriceTier::Na; break;
        default:
            throw std::runtime_error("Unknown LULD reference price tier: " + std::string(1, luld_tier));
    }
    
    std::optional<bool> etp_flag = parse_etp_flag();
    uint32_t etp_leverage_factor = read_u32();
    bool inverse_indicator = parse_char_to_bool();
    
    return StockDirectory {
        stock,
        market_category,
        financial_status,
        round_lot_size,
        round_lots_only,
        issue_classification,
        issue_subtype,
        authenticity,
        short_sale_threshold,
        ipo_flag,
        luld_ref_price_tier,
        etp_flag,
        etp_leverage_factor,
        inverse_indicator
    };
}

TradingAction Parser::parse_trading_action() {
    auto stock = read_stock();
    
    char trading_state_char = read_u8();
    TradingState trading_state;
    switch (trading_state_char) {
        case 'H': trading_state = TradingState::Halted; break;
        case 'P': trading_state = TradingState::Paused; break;
        case 'Q': trading_state = TradingState::QuotationOnly; break;
        case 'T': trading_state = TradingState::Trading; break;
        default:
            throw std::runtime_error("Unknown trading state: " + std::string(1, trading_state_char));
    }
    
    // Skip reserved byte
    read_u8();
    
    ArrayString4 reason = read_array_string4();
    
    return TradingAction {
        stock,
        trading_state,
        reason
    };
}

RegShoRestriction Parser::parse_reg_sho_restriction() {
    auto stock = read_stock();
    
    char action_char = read_u8();
    RegShoAction action;
    switch (action_char) {
        case '0': action = RegShoAction::None; break;
        case '1': action = RegShoAction::Intraday; break;
        case '2': action = RegShoAction::Extant; break;
        default:
            throw std::runtime_error("Unknown RegSho action: " + std::string(1, action_char));
    }
    
    return RegShoRestriction {
        stock,
        action
    };
}

MarketParticipantPosition Parser::parse_participant_position() {
    auto mpid = read_array_string4();
    auto stock = read_stock();
    bool primary_market_maker = parse_char_to_bool();
    
    char mode_char = read_u8();
    MarketMakerMode market_maker_mode;
    switch (mode_char) {
        case 'N': market_maker_mode = MarketMakerMode::Normal; break;
        case 'P': market_maker_mode = MarketMakerMode::Passive; break;
        case 'S': market_maker_mode = MarketMakerMode::Syndicate; break;
        case 'R': market_maker_mode = MarketMakerMode::Presyndicate; break;
        case 'L': market_maker_mode = MarketMakerMode::Penalty; break;
        default:
            throw std::runtime_error("Unknown market maker mode: " + std::string(1, mode_char));
    }
    
    char state_char = read_u8();
    MarketParticipantState market_participant_state;
    switch (state_char) {
        case 'A': market_participant_state = MarketParticipantState::Active; break;
        case 'E': market_participant_state = MarketParticipantState::Excused; break;
        case 'W': market_participant_state = MarketParticipantState::Withdrawn; break;
        case 'S': market_participant_state = MarketParticipantState::Suspended; break;
        case 'D': market_participant_state = MarketParticipantState::Deleted; break;
        default:
            throw std::runtime_error("Unknown market participant state: " + std::string(1, state_char));
    }
    
    return MarketParticipantPosition {
        mpid,
        stock,
        primary_market_maker,
        market_maker_mode,
        market_participant_state
    };
}

AddOrder Parser::parse_add_order(bool with_mpid) {
    uint64_t reference = read_u64();
    
    char side_char = read_u8();
    Side side;
    switch (side_char) {
        case 'B': side = Side::Buy; break;
        case 'S': side = Side::Sell; break;
        default:
            throw std::runtime_error("Unknown side: " + std::string(1, side_char));
    }
    
    uint32_t shares = read_u32();
    auto stock = read_stock();
    uint32_t price_raw = read_u32();
    std::optional<ArrayString4> mpid;
    
    if (with_mpid) {
        mpid = read_array_string4();
    }
    
    return AddOrder {
        reference,
        side,
        shares,
        stock,
        Price4(price_raw),
        mpid
    };
}

OrderExecuted Parser::parse_order_executed() {
    uint64_t reference = read_u64();
    uint32_t executed = read_u32();
    uint64_t match_number = read_u64();
    
    return OrderExecuted {
        reference,
        executed,
        match_number
    };
}

OrderExecutedWithPrice Parser::parse_order_executed_with_price() {
    uint64_t reference = read_u64();
    uint32_t executed = read_u32();
    uint64_t match_number = read_u64();
    bool printable = parse_char_to_bool();
    uint32_t price_raw = read_u32();
    
    return OrderExecutedWithPrice {
        reference,
        executed,
        match_number,
        printable,
        Price4(price_raw)
    };
}

OrderCancelled Parser::parse_order_cancelled() {
    uint64_t reference = read_u64();
    uint32_t cancelled = read_u32();
    
    return OrderCancelled {
        reference,
        cancelled
    };
}

ReplaceOrder Parser::parse_replace_order() {
    uint64_t old_reference = read_u64();
    uint64_t new_reference = read_u64();
    uint32_t shares = read_u32();
    uint32_t price_raw = read_u32();
    
    return ReplaceOrder {
        old_reference,
        new_reference,
        shares,
        Price4(price_raw)
    };
}

DeleteOrder Parser::parse_delete_order() {
    uint64_t reference = read_u64();
    
    return DeleteOrder {
        reference
    };
}

BrokenTrade Parser::parse_broken_trade() {
    uint64_t match_number = read_u64();
    
    return BrokenTrade {
        match_number
    };
}

NonCrossTrade Parser::parse_noncross_trade() {
    uint64_t reference = read_u64();
    
    char side_char = read_u8();
    Side side;
    switch (side_char) {
        case 'B': side = Side::Buy; break;
        case 'S': side = Side::Sell; break;
        default:
            throw std::runtime_error("Unknown side: " + std::string(1, side_char));
    }
    
    uint32_t shares = read_u32();
    auto stock = read_stock();
    uint32_t price_raw = read_u32();
    uint64_t match_number = read_u64();
    
    return NonCrossTrade {
        reference,
        side,
        shares,
        stock,
        Price4(price_raw),
        match_number
    };
}

CrossTrade Parser::parse_cross_trade() {
    uint64_t shares = read_u64();
    auto stock = read_stock();
    uint32_t price_raw = read_u32();
    uint64_t match_number = read_u64();
    
    char cross_type_char = read_u8();
    CrossType cross_type;
    switch (cross_type_char) {
        case 'O': cross_type = CrossType::Opening; break;
        case 'C': cross_type = CrossType::Closing; break;
        case 'H': cross_type = CrossType::IpoOrHalted; break;
        case 'I': cross_type = CrossType::Intraday; break;
        case 'A': cross_type = CrossType::ExtendedTradingClose; break;
        default:
            throw std::runtime_error("Unknown cross type: " + std::string(1, cross_type_char));
    }
    
    return CrossTrade {
        shares,
        stock,
        Price4(price_raw),
        match_number,
        cross_type
    };
}

ImbalanceIndicator Parser::parse_imbalance_indicator() {
    uint64_t paired_shares = read_u64();
    uint64_t imbalance_shares = read_u64();
    
    char direction_char = read_u8();
    ImbalanceDirection imbalance_direction;
    switch (direction_char) {
        case 'B': imbalance_direction = ImbalanceDirection::Buy; break;
        case 'S': imbalance_direction = ImbalanceDirection::Sell; break;
        case 'N': imbalance_direction = ImbalanceDirection::NoImbalance; break;
        case 'O': imbalance_direction = ImbalanceDirection::InsufficientOrders; break;
        default:
            throw std::runtime_error("Unknown imbalance direction: " + std::string(1, direction_char));
    }
    
    auto stock = read_stock();
    uint32_t far_price_raw = read_u32();
    uint32_t near_price_raw = read_u32();
    uint32_t current_ref_price_raw = read_u32();
    
    char cross_type_char = read_u8();
    CrossType cross_type;
    switch (cross_type_char) {
        case 'O': cross_type = CrossType::Opening; break;
        case 'C': cross_type = CrossType::Closing; break;
        case 'H': cross_type = CrossType::IpoOrHalted; break;
        case 'A': cross_type = CrossType::ExtendedTradingClose; break;
        default:
            throw std::runtime_error("Unknown cross type: " + std::string(1, cross_type_char));
    }
    
    char price_variation_indicator = read_u8();
    
    return ImbalanceIndicator {
        paired_shares,
        imbalance_shares,
        imbalance_direction,
        stock,
        Price4(far_price_raw),
        Price4(near_price_raw),
        Price4(current_ref_price_raw),
        cross_type,
        static_cast<char>(price_variation_indicator)
    };
}

RetailPriceImprovementIndicator Parser::parse_retail_price_improvement_indicator() {
    auto stock = read_stock();
    
    char flag_char = read_u8();
    InterestFlag interest_flag;
    switch (flag_char) {
        case 'B': interest_flag = InterestFlag::RPIAvailableBuySide; break;
        case 'S': interest_flag = InterestFlag::RPIAvailableSellSide; break;
        case 'A': interest_flag = InterestFlag::RPIAvailableBothSides; break;
        case 'N': interest_flag = InterestFlag::RPINoneAvailable; break;
        default:
            throw std::runtime_error("Unknown interest flag: " + std::string(1, flag_char));
    }
    
    return RetailPriceImprovementIndicator {
        stock,
        interest_flag
    };
}

IpoQuotingPeriod Parser::parse_ipo_quoting_period() {
    auto stock = read_stock();
    uint32_t release_time = read_u32();
    
    char qualifier_char = read_u8();
    IpoReleaseQualifier release_qualifier;
    switch (qualifier_char) {
        case 'A': release_qualifier = IpoReleaseQualifier::Anticipated; break;
        case 'C': release_qualifier = IpoReleaseQualifier::Cancelled; break;
        default:
            throw std::runtime_error("Unknown IPO release qualifier: " + std::string(1, qualifier_char));
    }
    
    uint32_t price_raw = read_u32();
    
    return IpoQuotingPeriod {
        stock,
        release_time,
        release_qualifier,
        Price4(price_raw)
    };
}

LULDAuctionCollar Parser::parse_luld_auction_collar() {
    auto stock = read_stock();
    uint32_t ref_price_raw = read_u32();
    uint32_t upper_price_raw = read_u32();
    uint32_t lower_price_raw = read_u32();
    uint32_t extension = read_u32();
    
    return LULDAuctionCollar {
        stock,
        Price4(ref_price_raw),
        Price4(upper_price_raw),
        Price4(lower_price_raw),
        extension
    };
}

MwcbDeclineLevel Parser::parse_mwcb_decline_level() {
    uint64_t level1_raw = read_u64();
    uint64_t level2_raw = read_u64();
    uint64_t level3_raw = read_u64();
    
    return MwcbDeclineLevel {
        Price8(level1_raw),
        Price8(level2_raw),
        Price8(level3_raw)
    };
}

LevelBreached Parser::parse_breach() {
    char level_char = read_u8();
    
    switch (level_char) {
        case '1': return LevelBreached::L1;
        case '2': return LevelBreached::L2;
        case '3': return LevelBreached::L3;
        default:
            throw std::runtime_error("Unknown breach level: " + std::string(1, level_char));
    }
}

} // namespace itch
