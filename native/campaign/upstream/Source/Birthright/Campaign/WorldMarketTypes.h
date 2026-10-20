#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <vector>

namespace Birthright::Campaign
{
// Food stays in AreaEconomy. These are additional durable regional inventories.
enum class MarketGood : std::uint32_t { Materials, Tools, Luxuries, Count };
constexpr std::size_t MarketGoodCount = 3;
struct ProductionRecipe
{
    double OutputPerWorker = 0;
    std::array<double, MarketGoodCount> Inputs{};
};
struct MarketRules
{
    std::uint32_t Version = 4;
    bool Enabled = false;
    bool ResponsiveBudgets = true, PersistentNutrition = true, DemandProduction = true;
    double ProducerReserveDays = 30, TreasuryReserveDays = 180;
    double BudgetReleaseDays = 90, GoodsTargetDays = 60;
    bool ReceiptBudgets = true;
    double ServiceIncomeShare = 0.9, IncomeSmoothingDays = 30, EmergencyReleaseDays = 30;
    double MilitaryReserveDays = 30;
    double NutritionLossDays = 30, NutritionRecoveryDays = 90;
    double StarvationThreshold = 0.9;
    double InitialCashPerPerson = 2;
    double DailyWage = 0.002;
    double TaxRate = 0.18;
    double FoodBasePrice = 0.002;
    double SubsistenceShare = 0.8;
    double TradeDailyFraction = 0.12;
    double ReserveDays = 15;
    double InvestmentFraction = 0.05;
    std::uint32_t ConstructionDays = 90;
    std::array<ProductionRecipe, MarketGoodCount> Recipes{{{0.004,{}},{0.001,{1,0,0}},{0.0015,{1,0.1,0}}}};
};
struct RegionalMarket
{
    double HouseholdCash = 0, ProducerCash = 0;
    double FoodPrice = 0.002, FoodCoverage = 1, WageCoverage = 1;
    double ArmySupply = 1;
    double UnpaidWages = 0, ServiceReceipts = 0, FoodPurchases = 0;
    std::array<double, MarketGoodCount> DailyUse{}, LastObservedUse{};
    double PublicCapital = 0;
    double HarvestFactor = 1; // Scenario/weather input, expires at ShockUntil.
    std::int64_t ShockUntil = 0, TradeBlockedUntil = 0;
    std::array<double, MarketGoodCount> Stock{}, Produced{}, Used{}, Wasted{};
};
struct MarketArea
{
    double LaborCapacity = 1;
    double Nutrition = 1;
    std::uint64_t ShortageDays = 0;
    // Integrated exposure, not a count of unique people. One person missing half a day's food = 0.5.
    double SupplyShortagePersonDays = 0, CashShortagePersonDays = 0, NutritionDeficitPersonDays = 0;
    std::uint64_t DemographicBirths = 0, DemographicDeaths = 0;
    double ExpectedNutritionDeaths = 0; // Continuous mortality contribution before cohort rounding.
    double FoodImported = 0, FoodExported = 0;
};
struct EstateAsset
{
    double Capital = 0, PendingCapital = 0;
    std::int64_t ReadyDay = 0, LastInvestmentDay = -1000000;
};
struct TradeLink { std::uint64_t First = 0, Second = 0; std::uint32_t Distance = 1; };
struct CharacterWants
{
    double Income = 0.5, Status = 0.5, Security = 0.5, Autonomy = 0.5;
};
// Diagnostic history starts at activation/migration; no historical receipts are invented.
// Targets earmark existing treasury cash, not separate money accounts.
struct PublicBudget
{
    std::uint64_t ObservedDays = 0;
    double TaxReceipts = 0, ServicePaid = 0, EmergencyReleased = 0, SurplusReleased = 0, ServiceShortfall = 0;
    double ReceiptsToday = 0, SmoothedReceipts = 0, ServicesToday = 0, DesiredServices = 0;
    double MilitaryDaily = 0, MilitaryReserve = 0, ReserveTarget = 0, AvailableBeforeServices = 0;
};
struct MarketState
{
    MarketRules Policy;
    std::int64_t DiagnosticsStartedDay = 0;
    std::int64_t BudgetStartedDay = 0;
    double BudgetOpeningTaxes = 0;
    std::map<std::uint64_t, PublicBudget> Budgets; // Includes retired realms for cumulative accounting.
    std::map<std::uint64_t, RegionalMarket> Regions;
    std::map<std::uint64_t, MarketArea> Areas;
    std::map<std::pair<std::uint64_t,std::uint64_t>, EstateAsset> Estates; // Family, region.
    std::map<std::uint64_t, CharacterWants> Wants;
    std::map<std::pair<std::uint64_t,std::uint64_t>, double> Readiness; // War, domain.
    std::vector<TradeLink> Links;
    double InitialMoney = 0, FoodTraded = 0, Taxes = 0, Wages = 0, Rents = 0;
    double Investment = 0, Inherited = 0, ArmyFood = 0;
};
}
