#pragma once
#include "World.h"
namespace Birthright::Campaign
{
Result ValidateMarket(const Snapshot& state);
std::vector<double> AdvanceMarket(Snapshot& state);
Result AdvanceEstates(World& world);
void CreditMarketServices(Snapshot& state, Id domain, double amount);
// Pure affordable quote. Callers own payments, receipt history and treasury authority.
struct PublicBudgetQuote { double Services = 0, Desired = 0, Emergency = 0, Surplus = 0, ReserveTarget = 0; };
PublicBudgetQuote QuotePublicBudget(const MarketRules& policy, double treasury, double receipts,
    double smoothedReceipts, double militaryDaily, double militaryReserve);
void AdvancePublicBudgets(Snapshot& state);
void ExportPublicBudgets(const Snapshot& state, std::ostream& output);
double MarketMoney(const Snapshot& state);
double EconomicSatisfaction(const Snapshot& state, Id character);
bool CanBuyComforts(const Snapshot& state, Id character, double cost);
void BuyComforts(Snapshot& state, Id character, double cost);
double PurchaseDefenseFood(Snapshot& state, Id region, Id family, double soldiers, double& cost);
void WriteMarket(const Snapshot& state, std::ostream& output);
Result ReadMarket(std::istream& input, Snapshot& state);
void ExportMarket(const Snapshot& state, std::ostream& output);
}
