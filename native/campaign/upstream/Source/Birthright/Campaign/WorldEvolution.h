#pragma once

#include "World.h"

namespace Birthright::Campaign
{
// Policy proposes intents; the authoritative command rechecks cost, office, life and cooldown.
enum class CampaignActivity : std::uint32_t { Practice, Govern, Cultivate };
struct ActivityIntent
{
    std::uint32_t Version = 1;
    Id Actor = 0;
    Id Area = 0;
    CampaignActivity Kind = CampaignActivity::Practice;
};
struct ActivityQuote
{
    Result Eligibility;
    Id PayingDomain = 0;
    double Cost = 0;
    double Favor = 0;
    Deity Path = Deity::Count;
};
ActivityQuote QuoteActivity(const Snapshot& state, const ActivityIntent& intent);
std::vector<ActivityIntent> PlanActivities(const Snapshot& state);
Result ValidateEvolution(const Snapshot& state);
Result ReadEvolution(std::istream& input, Snapshot& state);
void WriteEvolution(const Snapshot& state, std::ostream& output);
void ExportEvolution(const Snapshot& state, std::ostream& output);
// Called only on the private candidate used by AdvanceDays; failure discards the entire advance.
Result AdvanceEvolution(World& world);
// Shortages in ascending land-area ID order, matching validated economic ledgers.
// Keep fractional daily flow in the transaction; integer FoodCapacity is an inspection/legacy field.
std::vector<double> AdvanceEconomy(Snapshot& state);
// One canonical daily step on opening stocks; no transfer or report calls this.
void DecayFoodReserves(Snapshot& state);
}
