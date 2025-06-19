#pragma once

#include <string>
#include <array>
#include <optional>
#include <cstdint>

namespace itch {

// Fixed-size string types
using ArrayString4 = std::array<char, 4>;
using ArrayString8 = std::array<char, 8>;

// Event codes
enum class EventCode {
    StartOfMessages,
    StartOfSystemHours,
    StartOfMarketHours,
    EndOfMarketHours,
    EndOfSystemHours,
    EndOfMessages
};

// Market categories
enum class MarketCategory {
    NasdaqGlobalSelect,
    NasdaqGlobalMarket,
    NasdaqCapitalMarket,
    Nyse,
    NyseMkt,
    NyseArca,
    BatsZExchange,
    InvestorsExchange,
    Unavailable
};

// Financial status indicators
enum class FinancialStatus {
    Normal,
    Deficient,
    Delinquent,
    Bankrupt,
    Suspended,
    DeficientBankrupt,
    DeficientDelinquent,
    DelinquentBankrupt,
    DeficientDelinquentBankrupt,
    EtpSuspended,
    Unavailable
};

// Issue classifications
enum class IssueClassification {
    AmericanDepositaryShare,
    Bond,
    CommonStock,
    DepositoryReceipt,
    A144,
    LimitedPartnership,
    Notes,
    OrdinaryShare,
    PreferredStock,
    OtherSecurities,
    Right,
    SharesOfBeneficialInterest,
    ConvertibleDebenture,
    Unit,
    UnitsPerBenifInt,
    Warrant
};

// Issue subtypes
enum class IssueSubType {
    PreferredTrustSecurities,
    AlphaIndexETNs,
    IndexBasedDerivative,
    CommonShares,
    CommodityBasedTrustShares,
    CommodityFuturesTrustShares,
    CommodityLinkedSecurities,
    CommodityIndexTrustShares,
    CollateralizedMortgageObligation,
    CurrencyTrustShares,
    CommodityCurrencyLinkedSecurities,
    CurrencyWarrants,
    GlobalDepositaryShares,
    ETFPortfolioDepositaryReceipt,
    EquityGoldShares,
    ETNEquityIndexLinkedSecurities,
    ExchangeTradedManagedFunds,
    ExchangeTradedNotes,
    EquityUnits,
    Holdrs,
    ETNFixedIncomeLinkedSecurities,
    ETNFuturesLinkedSecurities,
    GlobalShares,
    ETFIndexFundShares,
    InterestRate,
    IndexWarrant,
    IndexLinkedExchangeableNotes,
    CorporateBackedTrustSecurity,
    ContingentLitigationRight,
    Llc,
    EquityBasedDerivative,
    ManagedFundShares,
    ETNMultiFactorIndexLinkedSecurities,
    ManagedTrustSecurities,
    NYRegistryShares,
    OpenEndedMutualFund,
    PrivatelyHeldSecurity,
    PoisonPill,
    PartnershipUnits,
    ClosedEndFunds,
    RegS,
    CommodityRedeemableCommodityLinkedSecurities,
    ETNRedeemableFuturesLinkedSecurities,
    REIT,
    CommodityRedeemableCurrencyLinkedSecurities,
    Seed,
    SpotRateClosing,
    SpotRateIntraday,
    TrackingStock,
    TrustCertificates,
    TrustUnits,
    Portal,
    ContingentValueRight,
    TrustIssuedReceipts,
    WorldCurrencyOption,
    Trust,
    Other,
    NotApplicable
};

// LULD reference price tier
enum class LuldRefPriceTier {
    Tier1,
    Tier2,
    Na
};

// Market maker modes
enum class MarketMakerMode {
    Normal,
    Passive,
    Syndicate,
    Presyndicate,
    Penalty
};

// Market participant states
enum class MarketParticipantState {
    Active,
    Excused,
    Withdrawn,
    Suspended,
    Deleted
};

// Reg SHO actions
enum class RegShoAction {
    None,
    Intraday,
    Extant
};

// Trading states
enum class TradingState {
    Halted,
    Paused,
    QuotationOnly,
    Trading
};

// Side (buy/sell)
enum class Side {
    Buy,
    Sell
};

// Imbalance directions
enum class ImbalanceDirection {
    Buy,
    Sell,
    NoImbalance,
    InsufficientOrders
};

// Cross types
enum class CrossType {
    Opening,
    Closing,
    IpoOrHalted,
    Intraday,
    ExtendedTradingClose
};

// IPO release qualifiers
enum class IpoReleaseQualifier {
    Anticipated,
    Cancelled
};

// MWCB level breached
enum class LevelBreached {
    L1,
    L2,
    L3
};

// Interest flags for retail price improvements
enum class InterestFlag {
    RPIAvailableBuySide,
    RPIAvailableSellSide,
    RPIAvailableBothSides,
    RPINoneAvailable
};

// Helper functions for parsing enums from bytes
IssueClassification parse_issue_classification(char value);
IssueSubType parse_issue_subtype(const char* value);

// String conversion functions
std::string to_string(EventCode code);
std::string to_string(MarketCategory category);
std::string to_string(FinancialStatus status);
std::string to_string(IssueClassification classification);
std::string to_string(IssueSubType subtype);
std::string to_string(LuldRefPriceTier tier);
std::string to_string(MarketMakerMode mode);
std::string to_string(MarketParticipantState state);
std::string to_string(RegShoAction action);
std::string to_string(TradingState state);
std::string to_string(Side side);
std::string to_string(ImbalanceDirection direction);
std::string to_string(CrossType type);
std::string to_string(IpoReleaseQualifier qualifier);
std::string to_string(LevelBreached level);
std::string to_string(InterestFlag flag);

// Utility functions
std::string array_to_string(const ArrayString4& arr, bool preserve_spaces = false);
std::string array_to_string(const ArrayString8& arr, bool preserve_spaces = false);
bool char_to_bool(char c);
std::optional<bool> maybe_char_to_bool(char c);

} // namespace itch