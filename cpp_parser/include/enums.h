#pragma once

#include <string>
#include <array>
#include <optional>
#include <cstdint>

namespace itch {

using ArrayString4 = std::array<char, 4>;
using ArrayString8 = std::array<char, 8>;

enum class EventCode {
    StartOfMessages,
    StartOfSystemHours,
    StartOfMarketHours,
    EndOfMarketHours,
    EndOfSystemHours,
    EndOfMessages
};

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

enum class LuldRefPriceTier {
    Tier1,
    Tier2,
    Na
};

enum class MarketMakerMode {
    Normal,
    Passive,
    Syndicate,
    Presyndicate,
    Penalty
};

enum class MarketParticipantState {
    Active,
    Excused,
    Withdrawn,
    Suspended,
    Deleted
};

enum class RegShoAction {
    None,
    Intraday,
    Extant
};

enum class TradingState {
    Halted,
    Paused,
    QuotationOnly,
    Trading
};


enum class Side {
    Buy,
    Sell
};


enum class ImbalanceDirection {
    Buy,
    Sell,
    NoImbalance,
    InsufficientOrders
};


enum class CrossType {
    Opening,
    Closing,
    IpoOrHalted,
    Intraday,
    ExtendedTradingClose
};


enum class IpoReleaseQualifier {
    Anticipated,
    Cancelled
};


enum class LevelBreached {
    L1,
    L2,
    L3
};

enum class InterestFlag {
    RPIAvailableBuySide,
    RPIAvailableSellSide,
    RPIAvailableBothSides,
    RPINoneAvailable
};

IssueClassification parse_issue_classification(char value);
IssueSubType parse_issue_subtype(const char* value);

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

std::string array_to_string(const ArrayString4& arr, bool preserve_spaces = false);
std::string array_to_string(const ArrayString8& arr, bool preserve_spaces = false);
bool char_to_bool(char c);
std::optional<bool> maybe_char_to_bool(char c);

}
