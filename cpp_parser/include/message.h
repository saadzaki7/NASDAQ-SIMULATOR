#pragma once

#include "enums.h"
#include "price.h"
#include <variant>
#include <cstdint>
#include <optional>
#include <string>
#include <memory>

namespace itch {

// Forward declarations for all message types
struct StockDirectory;
struct MarketParticipantPosition;
struct AddOrder;
struct OrderExecuted;
struct OrderExecutedWithPrice;
struct OrderCancelled;
struct ReplaceOrder;
struct DeleteOrder;
struct CrossTrade;
struct BrokenTrade;
struct NonCrossTrade;
struct RetailPriceImprovementIndicator;
struct ImbalanceIndicator;
struct IpoQuotingPeriod;
struct MwcbDeclineLevel;
struct LULDAuctionCollar;
struct SystemEvent;
struct TradingAction;
struct RegShoRestriction;

// Add Order message
struct AddOrder {
    uint64_t reference;
    Side side;
    uint32_t shares;
    ArrayString8 stock;
    Price4 price;
    std::optional<ArrayString4> mpid;
};

// Replace Order message
struct ReplaceOrder {
    uint64_t old_reference;
    uint64_t new_reference;
    uint32_t shares;
    Price4 price;
};

// Imbalance Indicator message
struct ImbalanceIndicator {
    uint64_t paired_shares;
    uint64_t imbalance_shares;
    ImbalanceDirection imbalance_direction;
    ArrayString8 stock;
    Price4 far_price;
    Price4 near_price;
    Price4 current_ref_price;
    CrossType cross_type;
    char price_variation_indicator;
};

// Cross Trade message
struct CrossTrade {
    uint64_t shares;
    ArrayString8 stock;
    Price4 cross_price;
    uint64_t match_number;
    CrossType cross_type;
};

// Retail Price Improvement Indicator message
struct RetailPriceImprovementIndicator {
    ArrayString8 stock;
    InterestFlag interest_flag;
};

// Non-Cross Trade message
struct NonCrossTrade {
    uint64_t reference;
    Side side;
    uint32_t shares;
    ArrayString8 stock;
    Price4 price;
    uint64_t match_number;
};

// IPO Quoting Period message
struct IpoQuotingPeriod {
    ArrayString8 stock;
    uint32_t release_time;
    IpoReleaseQualifier release_qualifier;
    Price4 price;
};

// Stock Directory message
struct StockDirectory {
    ArrayString8 stock;
    MarketCategory market_category;
    FinancialStatus financial_status;
    uint32_t round_lot_size;
    bool round_lots_only;
    IssueClassification issue_classification;
    IssueSubType issue_subtype;
    bool authenticity;
    std::optional<bool> short_sale_threshold;
    std::optional<bool> ipo_flag;
    LuldRefPriceTier luld_ref_price_tier;
    std::optional<bool> etp_flag;
    uint32_t etp_leverage_factor;
    bool inverse_indicator;
};

// Market Participant Position message
struct MarketParticipantPosition {
    ArrayString4 mpid;
    ArrayString8 stock;
    bool primary_market_maker;
    MarketMakerMode market_maker_mode;
    MarketParticipantState market_participant_state;
};

// System Event message
struct SystemEvent {
    EventCode event;
};

// Trading Action message
struct TradingAction {
    ArrayString8 stock;
    TradingState trading_state;
    ArrayString4 reason;
};

// Regulation SHO Short Sale Price Restriction message
struct RegShoRestriction {
    ArrayString8 stock;
    RegShoAction action;
};

// Order Executed message
struct OrderExecuted {
    uint64_t reference;
    uint32_t executed;
    uint64_t match_number;
};

// Order Executed with Price message
struct OrderExecutedWithPrice {
    uint64_t reference;
    uint32_t executed;
    uint64_t match_number;
    bool printable;
    Price4 price;
};

// Order Cancel message
struct OrderCancelled {
    uint64_t reference;
    uint32_t cancelled;
};

// Delete Order message
struct DeleteOrder {
    uint64_t reference;
};

// Broken Trade message
struct BrokenTrade {
    uint64_t match_number;
};

// LULD Auction Collar message
struct LULDAuctionCollar {
    ArrayString8 stock;
    Price4 ref_price;
    Price4 upper_price;
    Price4 lower_price;
    uint32_t extension;
};

// MWCB Decline Level message
struct MwcbDeclineLevel {
    Price8 level1;
    Price8 level2;
    Price8 level3;
};

// Message body variant that holds one of the possible message types
using MessageBody = std::variant<
    AddOrder,
    LevelBreached,
    BrokenTrade,
    CrossTrade,
    DeleteOrder,
    ImbalanceIndicator,
    IpoQuotingPeriod,
    LULDAuctionCollar,
    MwcbDeclineLevel,
    NonCrossTrade,
    OrderCancelled,
    OrderExecuted,
    OrderExecutedWithPrice,
    MarketParticipantPosition,
    RegShoRestriction,
    ReplaceOrder,
    StockDirectory,
    SystemEvent,
    TradingAction,
    RetailPriceImprovementIndicator
>;

// Main message structure
struct Message {
    uint8_t tag;
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint64_t timestamp;
    MessageBody body;
};

} // namespace itch