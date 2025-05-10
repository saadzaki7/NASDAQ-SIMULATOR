#pragma once

#include "message.h"
#include <string>
#include <nlohmann/json.hpp>

namespace itch {

// Forward declarations
class JsonSerializer {
public:
    // Convert a message to a JSON object
    static nlohmann::json to_json(const Message& message);

private:
    // Helpers for each message type
    static nlohmann::json add_order_to_json(const AddOrder& order);
    static nlohmann::json level_breached_to_json(const LevelBreached& breach);
    static nlohmann::json broken_trade_to_json(const BrokenTrade& trade);
    static nlohmann::json cross_trade_to_json(const CrossTrade& trade);
    static nlohmann::json delete_order_to_json(const DeleteOrder& order);
    static nlohmann::json imbalance_indicator_to_json(const ImbalanceIndicator& indicator);
    static nlohmann::json ipo_quoting_period_to_json(const IpoQuotingPeriod& period);
    static nlohmann::json luld_auction_collar_to_json(const LULDAuctionCollar& collar);
    static nlohmann::json mwcb_decline_level_to_json(const MwcbDeclineLevel& level);
    static nlohmann::json non_cross_trade_to_json(const NonCrossTrade& trade);
    static nlohmann::json order_cancelled_to_json(const OrderCancelled& order);
    static nlohmann::json order_executed_to_json(const OrderExecuted& order);
    static nlohmann::json order_executed_with_price_to_json(const OrderExecutedWithPrice& order);
    static nlohmann::json market_participant_position_to_json(const MarketParticipantPosition& position);
    static nlohmann::json reg_sho_restriction_to_json(const RegShoRestriction& restriction);
    static nlohmann::json replace_order_to_json(const ReplaceOrder& order);
    static nlohmann::json stock_directory_to_json(const StockDirectory& directory);
    static nlohmann::json system_event_to_json(const SystemEvent& event);
    static nlohmann::json trading_action_to_json(const TradingAction& action);
    static nlohmann::json retail_price_improvement_indicator_to_json(const RetailPriceImprovementIndicator& indicator);
    
    // Helpers for enum types
    static std::string event_code_to_string(EventCode code);
    static std::string market_category_to_string(MarketCategory category);
    static std::string financial_status_to_string(FinancialStatus status);
    static std::string issue_classification_to_string(IssueClassification classification);
    static std::string issue_subtype_to_string(IssueSubType subtype);
    static std::string luld_ref_price_tier_to_string(LuldRefPriceTier tier);
    static std::string market_maker_mode_to_string(MarketMakerMode mode);
    static std::string market_participant_state_to_string(MarketParticipantState state);
    static std::string reg_sho_action_to_string(RegShoAction action);
    static std::string trading_state_to_string(TradingState state);
    static std::string side_to_string(Side side);
    static std::string imbalance_direction_to_string(ImbalanceDirection direction);
    static std::string cross_type_to_string(CrossType type);
    static std::string ipo_release_qualifier_to_string(IpoReleaseQualifier qualifier);
    static std::string level_breached_to_string(LevelBreached level);
    static std::string interest_flag_to_string(InterestFlag flag);
};

} // namespace itch
