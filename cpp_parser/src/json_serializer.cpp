#include "../include/json_serializer.h"
#include <string>
#include <variant>

namespace itch {

template<size_t N>
std::string array_to_json_string(const std::array<char, N>& arr) {
    return "\"" + array_to_string(arr) + "\"";
}

nlohmann::json JsonSerializer::to_json(const Message& message) {
    nlohmann::json result;
    
    result["tag"] = message.tag;
    result["stock_locate"] = message.stock_locate;
    result["tracking_number"] = message.tracking_number;
    result["timestamp"] = message.timestamp;
    
    result["body"] = std::visit([](auto&& arg) -> nlohmann::json {
        using T = std::decay_t<decltype(arg)>;
        
        nlohmann::json body_json;
        
        if constexpr (std::is_same_v<T, AddOrder>) {
            body_json["AddOrder"] = add_order_to_json(arg);
        } else if constexpr (std::is_same_v<T, LevelBreached>) {
            body_json["Breach"] = level_breached_to_string(arg);
        } else if constexpr (std::is_same_v<T, BrokenTrade>) {
            body_json["BrokenTrade"] = broken_trade_to_json(arg);
        } else if constexpr (std::is_same_v<T, CrossTrade>) {
            body_json["CrossTrade"] = cross_trade_to_json(arg);
        } else if constexpr (std::is_same_v<T, DeleteOrder>) {
            body_json["DeleteOrder"] = delete_order_to_json(arg);
        } else if constexpr (std::is_same_v<T, ImbalanceIndicator>) {
            body_json["Imbalance"] = imbalance_indicator_to_json(arg);
        } else if constexpr (std::is_same_v<T, IpoQuotingPeriod>) {
            body_json["IpoQuotingPeriod"] = ipo_quoting_period_to_json(arg);
        } else if constexpr (std::is_same_v<T, LULDAuctionCollar>) {
            body_json["LULDAuctionCollar"] = luld_auction_collar_to_json(arg);
        } else if constexpr (std::is_same_v<T, MwcbDeclineLevel>) {
            body_json["MwcbDeclineLevel"] = mwcb_decline_level_to_json(arg);
        } else if constexpr (std::is_same_v<T, NonCrossTrade>) {
            body_json["NonCrossTrade"] = non_cross_trade_to_json(arg);
        } else if constexpr (std::is_same_v<T, OrderCancelled>) {
            body_json["OrderCancelled"] = order_cancelled_to_json(arg);
        } else if constexpr (std::is_same_v<T, OrderExecuted>) {
            body_json["OrderExecuted"] = order_executed_to_json(arg);
        } else if constexpr (std::is_same_v<T, OrderExecutedWithPrice>) {
            body_json["OrderExecutedWithPrice"] = order_executed_with_price_to_json(arg);
        } else if constexpr (std::is_same_v<T, MarketParticipantPosition>) {
            body_json["ParticipantPosition"] = market_participant_position_to_json(arg);
        } else if constexpr (std::is_same_v<T, RegShoRestriction>) {
            body_json["RegShoRestriction"] = reg_sho_restriction_to_json(arg);
        } else if constexpr (std::is_same_v<T, ReplaceOrder>) {
            body_json["ReplaceOrder"] = replace_order_to_json(arg);
        } else if constexpr (std::is_same_v<T, StockDirectory>) {
            body_json["StockDirectory"] = stock_directory_to_json(arg);
        } else if constexpr (std::is_same_v<T, SystemEvent>) {
            body_json["SystemEvent"] = system_event_to_json(arg);
        } else if constexpr (std::is_same_v<T, TradingAction>) {
            body_json["TradingAction"] = trading_action_to_json(arg);
        } else if constexpr (std::is_same_v<T, RetailPriceImprovementIndicator>) {
            body_json["RetailPriceImprovementIndicator"] = retail_price_improvement_indicator_to_json(arg);
        }
        
        return body_json;
    }, message.body);
    
    return result;
}

nlohmann::json JsonSerializer::add_order_to_json(const AddOrder& order) {
    nlohmann::json json;
    
    json["reference"] = order.reference;
    json["side"] = to_string(order.side);
    json["shares"] = order.shares;
    json["stock"] = array_to_string(order.stock, true);
    json["price"] = order.price.to_string();
    
    if (order.mpid) {
        json["mpid"] = array_to_string(order.mpid.value(), true);
    }
    
    return json;
}

nlohmann::json JsonSerializer::broken_trade_to_json(const BrokenTrade& trade) {
    nlohmann::json json;
    json["match_number"] = trade.match_number;
    return json;
}

nlohmann::json JsonSerializer::cross_trade_to_json(const CrossTrade& trade) {
    nlohmann::json json;
    
    json["shares"] = trade.shares;
    json["stock"] = array_to_string(trade.stock, true);
    json["cross_price"] = trade.cross_price.to_string();
    json["match_number"] = trade.match_number;
    json["cross_type"] = to_string(trade.cross_type);
    
    return json;
}

nlohmann::json JsonSerializer::delete_order_to_json(const DeleteOrder& order) {
    nlohmann::json json;
    json["reference"] = order.reference;
    return json;
}

nlohmann::json JsonSerializer::imbalance_indicator_to_json(const ImbalanceIndicator& indicator) {
    nlohmann::json json;
    
    json["paired_shares"] = indicator.paired_shares;
    json["imbalance_shares"] = indicator.imbalance_shares;
    json["imbalance_direction"] = to_string(indicator.imbalance_direction);
    json["stock"] = array_to_string(indicator.stock, true);
    json["far_price"] = indicator.far_price.to_string();
    json["near_price"] = indicator.near_price.to_string();
    json["current_ref_price"] = indicator.current_ref_price.to_string();
    json["cross_type"] = to_string(indicator.cross_type);
    json["price_variation_indicator"] = std::string(1, indicator.price_variation_indicator);
    
    return json;
}

nlohmann::json JsonSerializer::ipo_quoting_period_to_json(const IpoQuotingPeriod& period) {
    nlohmann::json json;
    
    json["stock"] = array_to_string(period.stock, true);
    json["release_time"] = period.release_time;
    json["release_qualifier"] = to_string(period.release_qualifier);
    json["price"] = period.price.to_string();
    
    return json;
}

nlohmann::json JsonSerializer::luld_auction_collar_to_json(const LULDAuctionCollar& collar) {
    nlohmann::json json;
    
    json["stock"] = array_to_string(collar.stock, true);
    json["ref_price"] = collar.ref_price.to_string();
    json["upper_price"] = collar.upper_price.to_string();
    json["lower_price"] = collar.lower_price.to_string();
    json["extension"] = collar.extension;
    
    return json;
}

nlohmann::json JsonSerializer::mwcb_decline_level_to_json(const MwcbDeclineLevel& level) {
    nlohmann::json json;
    
    json["level1"] = level.level1.to_string();
    json["level2"] = level.level2.to_string();
    json["level3"] = level.level3.to_string();
    
    return json;
}

nlohmann::json JsonSerializer::non_cross_trade_to_json(const NonCrossTrade& trade) {
    nlohmann::json json;
    
    json["reference"] = trade.reference;
    json["side"] = to_string(trade.side);
    json["shares"] = trade.shares;
    json["stock"] = array_to_string(trade.stock, true);
    json["price"] = trade.price.to_string();
    json["match_number"] = trade.match_number;
    
    return json;
}

nlohmann::json JsonSerializer::order_cancelled_to_json(const OrderCancelled& order) {
    nlohmann::json json;
    
    json["reference"] = order.reference;
    json["cancelled"] = order.cancelled;
    
    return json;
}

nlohmann::json JsonSerializer::order_executed_to_json(const OrderExecuted& order) {
    nlohmann::json json;
    
    json["reference"] = order.reference;
    json["executed"] = order.executed;
    json["match_number"] = order.match_number;
    
    return json;
}

nlohmann::json JsonSerializer::order_executed_with_price_to_json(const OrderExecutedWithPrice& order) {
    nlohmann::json json;
    
    json["reference"] = order.reference;
    json["executed"] = order.executed;
    json["match_number"] = order.match_number;
    json["printable"] = order.printable;
    json["price"] = order.price.to_string();
    
    return json;
}

nlohmann::json JsonSerializer::market_participant_position_to_json(const MarketParticipantPosition& position) {
    nlohmann::json json;
    
    json["mpid"] = array_to_string(position.mpid, true);
    json["stock"] = array_to_string(position.stock, true);
    json["primary_market_maker"] = position.primary_market_maker;
    json["market_maker_mode"] = to_string(position.market_maker_mode);
    json["market_participant_state"] = to_string(position.market_participant_state);
    
    return json;
}

nlohmann::json JsonSerializer::reg_sho_restriction_to_json(const RegShoRestriction& restriction) {
    nlohmann::json json;
    
    json["stock"] = array_to_string(restriction.stock, true);
    json["action"] = to_string(restriction.action);
    
    return json;
}

nlohmann::json JsonSerializer::replace_order_to_json(const ReplaceOrder& order) {
    nlohmann::json json;
    
    json["old_reference"] = order.old_reference;
    json["new_reference"] = order.new_reference;
    json["shares"] = order.shares;
    json["price"] = order.price.to_string();
    
    return json;
}

nlohmann::json JsonSerializer::stock_directory_to_json(const StockDirectory& directory) {
    nlohmann::json json;
    
    json["stock"] = array_to_string(directory.stock, true);
    json["market_category"] = to_string(directory.market_category);
    json["financial_status"] = to_string(directory.financial_status);
    json["round_lot_size"] = directory.round_lot_size;
    json["round_lots_only"] = directory.round_lots_only;
    json["issue_classification"] = to_string(directory.issue_classification);
    json["issue_subtype"] = to_string(directory.issue_subtype);
    json["authenticity"] = directory.authenticity;
    
    if (directory.short_sale_threshold) {
        json["short_sale_threshold"] = directory.short_sale_threshold.value();
    } else {
        json["short_sale_threshold"] = nullptr;
    }
    
    if (directory.ipo_flag) {
        json["ipo_flag"] = directory.ipo_flag.value();
    } else {
        json["ipo_flag"] = nullptr;
    }
    
    json["luld_ref_price_tier"] = to_string(directory.luld_ref_price_tier);
    
    if (directory.etp_flag) {
        json["etp_flag"] = directory.etp_flag.value();
    } else {
        json["etp_flag"] = false;
    }
    
    json["etp_leverage_factor"] = directory.etp_leverage_factor;
    json["inverse_indicator"] = directory.inverse_indicator;
    
    return json;
}

nlohmann::json JsonSerializer::system_event_to_json(const SystemEvent& event) {
    nlohmann::json json;
    json["event"] = to_string(event.event);
    return json;
}

nlohmann::json JsonSerializer::trading_action_to_json(const TradingAction& action) {
    nlohmann::json json;
    
    json["stock"] = array_to_string(action.stock, true);
    json["trading_state"] = to_string(action.trading_state);
    json["reason"] = array_to_string(action.reason, true);
    
    return json;
}

nlohmann::json JsonSerializer::retail_price_improvement_indicator_to_json(const RetailPriceImprovementIndicator& indicator) {
    nlohmann::json json;
    
    json["stock"] = array_to_string(indicator.stock, true);
    json["interest_flag"] = to_string(indicator.interest_flag);
    
    return json;
}

std::string JsonSerializer::event_code_to_string(EventCode code) {
    return to_string(code);
}

std::string JsonSerializer::market_category_to_string(MarketCategory category) {
    return to_string(category);
}

std::string JsonSerializer::financial_status_to_string(FinancialStatus status) {
    return to_string(status);
}

std::string JsonSerializer::issue_classification_to_string(IssueClassification classification) {
    return to_string(classification);
}

std::string JsonSerializer::issue_subtype_to_string(IssueSubType subtype) {
    return to_string(subtype);
}

std::string JsonSerializer::luld_ref_price_tier_to_string(LuldRefPriceTier tier) {
    return to_string(tier);
}

std::string JsonSerializer::market_maker_mode_to_string(MarketMakerMode mode) {
    return to_string(mode);
}

std::string JsonSerializer::market_participant_state_to_string(MarketParticipantState state) {
    return to_string(state);
}

std::string JsonSerializer::reg_sho_action_to_string(RegShoAction action) {
    return to_string(action);
}

std::string JsonSerializer::trading_state_to_string(TradingState state) {
    return to_string(state);
}

std::string JsonSerializer::side_to_string(Side side) {
    return to_string(side);
}

std::string JsonSerializer::imbalance_direction_to_string(ImbalanceDirection direction) {
    return to_string(direction);
}

std::string JsonSerializer::cross_type_to_string(CrossType type) {
    return to_string(type);
}

std::string JsonSerializer::ipo_release_qualifier_to_string(IpoReleaseQualifier qualifier) {
    return to_string(qualifier);
}

std::string JsonSerializer::level_breached_to_string(LevelBreached level) {
    return to_string(level);
}

std::string JsonSerializer::interest_flag_to_string(InterestFlag flag) {
    return to_string(flag);
}

} 
