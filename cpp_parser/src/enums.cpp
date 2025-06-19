#include "../include/enums.h"
#include <stdexcept>
#include <string>
#include <algorithm>

namespace itch {

IssueClassification parse_issue_classification(char value) {
    switch (value) {
        case 'A': return IssueClassification::AmericanDepositaryShare;
        case 'B': return IssueClassification::Bond;
        case 'C': return IssueClassification::CommonStock;
        case 'F': return IssueClassification::DepositoryReceipt;
        case 'I': return IssueClassification::A144;
        case 'L': return IssueClassification::LimitedPartnership;
        case 'N': return IssueClassification::Notes;
        case 'O': return IssueClassification::OrdinaryShare;
        case 'P': return IssueClassification::PreferredStock;
        case 'Q': return IssueClassification::OtherSecurities;
        case 'R': return IssueClassification::Right;
        case 'S': return IssueClassification::SharesOfBeneficialInterest;
        case 'T': return IssueClassification::ConvertibleDebenture;
        case 'U': return IssueClassification::Unit;
        case 'V': return IssueClassification::UnitsPerBenifInt;
        case 'W': return IssueClassification::Warrant;
        default: throw std::runtime_error("Invalid issue classification: " + std::string(1, value));
    }
}

IssueSubType parse_issue_subtype(const char* value) {
    // Create a 2-character string from the value
    std::string val(value, 2);
    
    if (val == "A ") return IssueSubType::PreferredTrustSecurities;
    if (val == "AI") return IssueSubType::AlphaIndexETNs;
    if (val == "B ") return IssueSubType::IndexBasedDerivative;
    if (val == "C ") return IssueSubType::CommonShares;
    if (val == "CB") return IssueSubType::CommodityBasedTrustShares;
    if (val == "CF") return IssueSubType::CommodityFuturesTrustShares;
    if (val == "CL") return IssueSubType::CommodityLinkedSecurities;
    if (val == "CM") return IssueSubType::CommodityIndexTrustShares;
    if (val == "CO") return IssueSubType::CollateralizedMortgageObligation;
    if (val == "CT") return IssueSubType::CurrencyTrustShares;
    if (val == "CU") return IssueSubType::CommodityCurrencyLinkedSecurities;
    if (val == "CW") return IssueSubType::CurrencyWarrants;
    if (val == "D ") return IssueSubType::GlobalDepositaryShares;
    if (val == "E ") return IssueSubType::ETFPortfolioDepositaryReceipt;
    if (val == "EG") return IssueSubType::EquityGoldShares;
    if (val == "EI") return IssueSubType::ETNEquityIndexLinkedSecurities;
    if (val == "EM") return IssueSubType::ExchangeTradedManagedFunds;
    if (val == "EN") return IssueSubType::ExchangeTradedNotes;
    if (val == "EU") return IssueSubType::EquityUnits;
    if (val == "F ") return IssueSubType::Holdrs;
    if (val == "FI") return IssueSubType::ETNFixedIncomeLinkedSecurities;
    if (val == "FL") return IssueSubType::ETNFuturesLinkedSecurities;
    if (val == "G ") return IssueSubType::GlobalShares;
    if (val == "I ") return IssueSubType::ETFIndexFundShares;
    if (val == "IR") return IssueSubType::InterestRate;
    if (val == "IW") return IssueSubType::IndexWarrant;
    if (val == "IX") return IssueSubType::IndexLinkedExchangeableNotes;
    if (val == "J ") return IssueSubType::CorporateBackedTrustSecurity;
    if (val == "L ") return IssueSubType::ContingentLitigationRight;
    if (val == "LL") return IssueSubType::Llc;
    if (val == "M ") return IssueSubType::EquityBasedDerivative;
    if (val == "MF") return IssueSubType::ManagedFundShares;
    if (val == "ML") return IssueSubType::ETNMultiFactorIndexLinkedSecurities;
    if (val == "MT") return IssueSubType::ManagedTrustSecurities;
    if (val == "N ") return IssueSubType::NYRegistryShares;
    if (val == "O ") return IssueSubType::OpenEndedMutualFund;
    if (val == "P ") return IssueSubType::PrivatelyHeldSecurity;
    if (val == "PP") return IssueSubType::PoisonPill;
    if (val == "PU") return IssueSubType::PartnershipUnits;
    if (val == "Q ") return IssueSubType::ClosedEndFunds;
    if (val == "R ") return IssueSubType::RegS;
    if (val == "RC") return IssueSubType::CommodityRedeemableCommodityLinkedSecurities;
    if (val == "RF") return IssueSubType::ETNRedeemableFuturesLinkedSecurities;
    if (val == "RT") return IssueSubType::REIT;
    if (val == "RU") return IssueSubType::CommodityRedeemableCurrencyLinkedSecurities;
    if (val == "S ") return IssueSubType::Seed;
    if (val == "SC") return IssueSubType::SpotRateClosing;
    if (val == "SI") return IssueSubType::SpotRateIntraday;
    if (val == "T ") return IssueSubType::TrackingStock;
    if (val == "TC") return IssueSubType::TrustCertificates;
    if (val == "TU") return IssueSubType::TrustUnits;
    if (val == "U ") return IssueSubType::Portal;
    if (val == "V ") return IssueSubType::ContingentValueRight;
    if (val == "W ") return IssueSubType::TrustIssuedReceipts;
    if (val == "WC") return IssueSubType::WorldCurrencyOption;
    if (val == "X ") return IssueSubType::Trust;
    if (val == "Y ") return IssueSubType::Other;
    if (val == "Z ") return IssueSubType::NotApplicable;
    
    throw std::runtime_error("Invalid issue subtype: " + val);
}

std::string to_string(EventCode code) {
    switch (code) {
        case EventCode::StartOfMessages: return "StartOfMessages";
        case EventCode::StartOfSystemHours: return "StartOfSystemHours";
        case EventCode::StartOfMarketHours: return "StartOfMarketHours";
        case EventCode::EndOfMarketHours: return "EndOfMarketHours";
        case EventCode::EndOfSystemHours: return "EndOfSystemHours";
        case EventCode::EndOfMessages: return "EndOfMessages";
        default: return "Unknown";
    }
}

std::string to_string(MarketCategory category) {
    switch (category) {
        case MarketCategory::NasdaqGlobalSelect: return "NasdaqGlobalSelect";
        case MarketCategory::NasdaqGlobalMarket: return "NasdaqGlobalMarket";
        case MarketCategory::NasdaqCapitalMarket: return "NasdaqCapitalMarket";
        case MarketCategory::Nyse: return "Nyse";
        case MarketCategory::NyseMkt: return "NyseMkt";
        case MarketCategory::NyseArca: return "NyseArca";
        case MarketCategory::BatsZExchange: return "BatsZExchange";
        case MarketCategory::InvestorsExchange: return "InvestorsExchange";
        case MarketCategory::Unavailable: return "Unavailable";
        default: return "Unknown";
    }
}

std::string to_string(FinancialStatus status) {
    switch (status) {
        case FinancialStatus::Normal: return "Normal";
        case FinancialStatus::Deficient: return "Deficient";
        case FinancialStatus::Delinquent: return "Delinquent";
        case FinancialStatus::Bankrupt: return "Bankrupt";
        case FinancialStatus::Suspended: return "Suspended";
        case FinancialStatus::DeficientBankrupt: return "DeficientBankrupt";
        case FinancialStatus::DeficientDelinquent: return "DeficientDelinquent";
        case FinancialStatus::DelinquentBankrupt: return "DelinquentBankrupt";
        case FinancialStatus::DeficientDelinquentBankrupt: return "DeficientDelinquentBankrupt";
        case FinancialStatus::EtpSuspended: return "EtpSuspended";
        case FinancialStatus::Unavailable: return "Unavailable";
        default: return "Unknown";
    }
}

std::string to_string(IssueClassification classification) {
    switch (classification) {
        case IssueClassification::AmericanDepositaryShare: return "AmericanDepositaryShare";
        case IssueClassification::Bond: return "Bond";
        case IssueClassification::CommonStock: return "CommonStock";
        case IssueClassification::DepositoryReceipt: return "DepositoryReceipt";
        case IssueClassification::A144: return "A144";
        case IssueClassification::LimitedPartnership: return "LimitedPartnership";
        case IssueClassification::Notes: return "Notes";
        case IssueClassification::OrdinaryShare: return "OrdinaryShare";
        case IssueClassification::PreferredStock: return "PreferredStock";
        case IssueClassification::OtherSecurities: return "OtherSecurities";
        case IssueClassification::Right: return "Right";
        case IssueClassification::SharesOfBeneficialInterest: return "SharesOfBeneficialInterest";
        case IssueClassification::ConvertibleDebenture: return "ConvertibleDebenture";
        case IssueClassification::Unit: return "Unit";
        case IssueClassification::UnitsPerBenifInt: return "UnitsPerBenifInt";
        case IssueClassification::Warrant: return "Warrant";
        default: return "Unknown";
    }
}

std::string to_string(IssueSubType subtype) {
    switch (subtype) {
        case IssueSubType::PreferredTrustSecurities: return "PreferredTrustSecurities";
        case IssueSubType::AlphaIndexETNs: return "AlphaIndexETNs";
        case IssueSubType::IndexBasedDerivative: return "IndexBasedDerivative";
        case IssueSubType::CommonShares: return "CommonShares";
        case IssueSubType::CommodityBasedTrustShares: return "CommodityBasedTrustShares";
        case IssueSubType::CommodityFuturesTrustShares: return "CommodityFuturesTrustShares";
        case IssueSubType::CommodityLinkedSecurities: return "CommodityLinkedSecurities";
        case IssueSubType::CommodityIndexTrustShares: return "CommodityIndexTrustShares";
        case IssueSubType::CollateralizedMortgageObligation: return "CollateralizedMortgageObligation";
        case IssueSubType::CurrencyTrustShares: return "CurrencyTrustShares";
        case IssueSubType::CommodityCurrencyLinkedSecurities: return "CommodityCurrencyLinkedSecurities";
        case IssueSubType::CurrencyWarrants: return "CurrencyWarrants";
        case IssueSubType::GlobalDepositaryShares: return "GlobalDepositaryShares";
        case IssueSubType::ETFPortfolioDepositaryReceipt: return "ETFPortfolioDepositaryReceipt";
        case IssueSubType::EquityGoldShares: return "EquityGoldShares";
        case IssueSubType::ETNEquityIndexLinkedSecurities: return "ETNEquityIndexLinkedSecurities";
        case IssueSubType::ExchangeTradedManagedFunds: return "ExchangeTradedManagedFunds";
        case IssueSubType::ExchangeTradedNotes: return "ExchangeTradedNotes";
        case IssueSubType::EquityUnits: return "EquityUnits";
        case IssueSubType::Holdrs: return "Holdrs";
        case IssueSubType::ETNFixedIncomeLinkedSecurities: return "ETNFixedIncomeLinkedSecurities";
        case IssueSubType::ETNFuturesLinkedSecurities: return "ETNFuturesLinkedSecurities";
        case IssueSubType::GlobalShares: return "GlobalShares";
        case IssueSubType::ETFIndexFundShares: return "ETFIndexFundShares";
        case IssueSubType::InterestRate: return "InterestRate";
        case IssueSubType::IndexWarrant: return "IndexWarrant";
        case IssueSubType::IndexLinkedExchangeableNotes: return "IndexLinkedExchangeableNotes";
        case IssueSubType::CorporateBackedTrustSecurity: return "CorporateBackedTrustSecurity";
        case IssueSubType::ContingentLitigationRight: return "ContingentLitigationRight";
        case IssueSubType::Llc: return "Llc";
        case IssueSubType::EquityBasedDerivative: return "EquityBasedDerivative";
        case IssueSubType::ManagedFundShares: return "ManagedFundShares";
        case IssueSubType::ETNMultiFactorIndexLinkedSecurities: return "ETNMultiFactorIndexLinkedSecurities";
        case IssueSubType::ManagedTrustSecurities: return "ManagedTrustSecurities";
        case IssueSubType::NYRegistryShares: return "NYRegistryShares";
        case IssueSubType::OpenEndedMutualFund: return "OpenEndedMutualFund";
        case IssueSubType::PrivatelyHeldSecurity: return "PrivatelyHeldSecurity";
        case IssueSubType::PoisonPill: return "PoisonPill";
        case IssueSubType::PartnershipUnits: return "PartnershipUnits";
        case IssueSubType::ClosedEndFunds: return "ClosedEndFunds";
        case IssueSubType::RegS: return "RegS";
        case IssueSubType::CommodityRedeemableCommodityLinkedSecurities: return "CommodityRedeemableCommodityLinkedSecurities";
        case IssueSubType::ETNRedeemableFuturesLinkedSecurities: return "ETNRedeemableFuturesLinkedSecurities";
        case IssueSubType::REIT: return "REIT";
        case IssueSubType::CommodityRedeemableCurrencyLinkedSecurities: return "CommodityRedeemableCurrencyLinkedSecurities";
        case IssueSubType::Seed: return "Seed";
        case IssueSubType::SpotRateClosing: return "SpotRateClosing";
        case IssueSubType::SpotRateIntraday: return "SpotRateIntraday";
        case IssueSubType::TrackingStock: return "TrackingStock";
        case IssueSubType::TrustCertificates: return "TrustCertificates";
        case IssueSubType::TrustUnits: return "TrustUnits";
        case IssueSubType::Portal: return "Portal";
        case IssueSubType::ContingentValueRight: return "ContingentValueRight";
        case IssueSubType::TrustIssuedReceipts: return "TrustIssuedReceipts";
        case IssueSubType::WorldCurrencyOption: return "WorldCurrencyOption";
        case IssueSubType::Trust: return "Trust";
        case IssueSubType::Other: return "Other";
        case IssueSubType::NotApplicable: return "NotApplicable";
        default: return "Unknown";
    }
}

std::string to_string(LuldRefPriceTier tier) {
    switch (tier) {
        case LuldRefPriceTier::Tier1: return "Tier1";
        case LuldRefPriceTier::Tier2: return "Tier2";
        case LuldRefPriceTier::Na: return "Na";
        default: return "Unknown";
    }
}

std::string to_string(MarketMakerMode mode) {
    switch (mode) {
        case MarketMakerMode::Normal: return "Normal";
        case MarketMakerMode::Passive: return "Passive";
        case MarketMakerMode::Syndicate: return "Syndicate";
        case MarketMakerMode::Presyndicate: return "Presyndicate";
        case MarketMakerMode::Penalty: return "Penalty";
        default: return "Unknown";
    }
}

std::string to_string(MarketParticipantState state) {
    switch (state) {
        case MarketParticipantState::Active: return "Active";
        case MarketParticipantState::Excused: return "Excused";
        case MarketParticipantState::Withdrawn: return "Withdrawn";
        case MarketParticipantState::Suspended: return "Suspended";
        case MarketParticipantState::Deleted: return "Deleted";
        default: return "Unknown";
    }
}

std::string to_string(RegShoAction action) {
    switch (action) {
        case RegShoAction::None: return "None";
        case RegShoAction::Intraday: return "Intraday";
        case RegShoAction::Extant: return "Extant";
        default: return "Unknown";
    }
}

std::string to_string(TradingState state) {
    switch (state) {
        case TradingState::Halted: return "Halted";
        case TradingState::Paused: return "Paused";
        case TradingState::QuotationOnly: return "QuotationOnly";
        case TradingState::Trading: return "Trading";
        default: return "Unknown";
    }
}

std::string to_string(Side side) {
    switch (side) {
        case Side::Buy: return "Buy";
        case Side::Sell: return "Sell";
        default: return "Unknown";
    }
}

std::string to_string(ImbalanceDirection direction) {
    switch (direction) {
        case ImbalanceDirection::Buy: return "Buy";
        case ImbalanceDirection::Sell: return "Sell";
        case ImbalanceDirection::NoImbalance: return "NoImbalance";
        case ImbalanceDirection::InsufficientOrders: return "InsufficientOrders";
        default: return "Unknown";
    }
}

std::string to_string(CrossType type) {
    switch (type) {
        case CrossType::Opening: return "Opening";
        case CrossType::Closing: return "Closing";
        case CrossType::IpoOrHalted: return "IpoOrHalted";
        case CrossType::Intraday: return "Intraday";
        case CrossType::ExtendedTradingClose: return "ExtendedTradingClose";
        default: return "Unknown";
    }
}

std::string to_string(IpoReleaseQualifier qualifier) {
    switch (qualifier) {
        case IpoReleaseQualifier::Anticipated: return "Anticipated";
        case IpoReleaseQualifier::Cancelled: return "Cancelled";
        default: return "Unknown";
    }
}

std::string to_string(LevelBreached level) {
    switch (level) {
        case LevelBreached::L1: return "L1";
        case LevelBreached::L2: return "L2";
        case LevelBreached::L3: return "L3";
        default: return "Unknown";
    }
}

std::string to_string(InterestFlag flag) {
    switch (flag) {
        case InterestFlag::RPIAvailableBuySide: return "RPIAvailableBuySide";
        case InterestFlag::RPIAvailableSellSide: return "RPIAvailableSellSide";
        case InterestFlag::RPIAvailableBothSides: return "RPIAvailableBothSides";
        case InterestFlag::RPINoneAvailable: return "RPINoneAvailable";
        default: return "Unknown";
    }
}

std::string array_to_string(const ArrayString4& arr, bool preserve_spaces) {
    std::string result(arr.begin(), arr.end());
    
    // If preserve_spaces is false, trim trailing spaces
    if (!preserve_spaces) {
        result.erase(std::find_if(result.rbegin(), result.rend(), [](char c) {
            return c != ' ';
        }).base(), result.end());
    }
    
    return result;
}

std::string array_to_string(const ArrayString8& arr, bool preserve_spaces) {
    std::string result(arr.begin(), arr.end());
    
    // If preserve_spaces is false, trim trailing spaces
    if (!preserve_spaces) {
        result.erase(std::find_if(result.rbegin(), result.rend(), [](char c) {
            return c != ' ';
        }).base(), result.end());
    }
    
    return result;
}

bool char_to_bool(char c) {
    if (c == 'Y') return true;
    if (c == 'N') return false;
    throw std::runtime_error("Invalid boolean character: " + std::string(1, c));
}

std::optional<bool> maybe_char_to_bool(char c) {
    if (c == 'Y') return true;
    if (c == 'N') return false;
    if (c == ' ') return std::nullopt;
    throw std::runtime_error("Invalid maybe-boolean character: " + std::string(1, c));
}

} // namespace itch