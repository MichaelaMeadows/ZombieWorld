#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>
class ACombatGameMode;
namespace Birthright::Campaign
{
namespace Testing
{
struct CampCombatProbe;
}
struct BanditRules
{
    std::uint32_t Version = 2;
    bool Enabled = false;
    std::uint32_t SpawnDays = 30, RaidDays = 14, RecoveryDays = 180, RaidHops = 2, SpacingHops = 3, MaxCamps = 12;
    double InitialStrength = 3, MaximumStrength = 24, DailyGrowth = 0, RaidGrowth = 0.25;
    double RaidStockFraction = 0.02, FoodPerStrength = 5, MaximumDevelopment = 0.45;
    double FoodUpkeep = 0.05, GrowthFood = 20, StarvationLoss = 0.02, StoragePerStrength = 30;
    bool OwnerDefense = true;
    std::uint32_t ResponseDays = 30, DefenseBattleDays = 3, DefenseMaxRounds = 4, DefenseCooldownDays = 90;
    double DefenseStrengthRatio = 1.75, DefenseWealthReserve = 50, DefenseFoodReserveDays = 3;
};
struct BanditCamp
{
    std::uint64_t Key = 0, AreaId = 0;
    bool Active = true;
    std::int64_t BornDay = 0, LastRaidDay = 0, SuppressedUntil = 0, ClearedDay = 0;
    double Strength = 3, FoodRaided = 0;
    std::uint64_t Raids = 0;
    double FoodStored = 0, FoodConsumed = 0, FoodSpoiled = 0, FoodWasted = 0, FoodAbandoned = 0, LegacyFoodConsumed = 0;
    double LootGrowth = 0, StarvationLosses = 0;
    std::uint64_t LastTargetArea = 0;
    std::uint32_t Outcome = 0; // 0 active, 1 player clear, 2 owner clear, 3 dispersed by hunger.
};
enum class ExpeditionPhase : std::uint32_t { Outbound, Fighting, Returning, Finished };
struct CampExpedition
{
    std::uint64_t Key = 0, Camp = 0, Domain = 0, Region = 0, Lord = 0, Family = 0, HomeArea = 0;
    ExpeditionPhase Phase = ExpeditionPhase::Outbound;
    std::vector<std::uint64_t> Route;
    std::uint32_t Position = 0, Rounds = 0, Mustered = 0, Casualties = 0;
    std::int64_t StartedDay = 0, NextActionDay = 0, FinishedDay = 0;
    double Readiness = 1, Cost = 0, Food = 0;
};
struct CampEngagement
{
    std::string EncounterId;
    std::uint64_t CampId = 0, LeaderId = 0, AreaId = 0;
    std::int64_t Day = 0;
    std::uint32_t Enemies = 0;
};
struct BanditState
{
    BanditRules Policy;
    std::uint64_t NextId = 1;
    std::map<std::uint64_t, BanditCamp> Camps;
    CampEngagement Engagement;
    std::uint64_t NextExpedition = 1;
    std::map<std::uint64_t, CampExpedition> Expeditions;
    std::map<std::uint64_t, double> FoodTakenByArea;
};
// Only the combat host can certify casualties/victory, never a player command or UI.
class CampCombatEvidence
{
    friend class ::ACombatGameMode;
    friend struct Testing::CampCombatProbe;
    friend class World;
    CampCombatEvidence(std::string encounter, std::uint64_t camp, std::uint32_t enemies, std::uint32_t defeated,
                       bool heroAlive)
        : EncounterId(std::move(encounter)), CampId(camp), Enemies(enemies), Defeated(defeated), HeroAlive(heroAlive)
    {
    }
    std::string EncounterId;
    std::uint64_t CampId;
    std::uint32_t Enemies, Defeated;
    bool HeroAlive;
};
} // namespace Birthright::Campaign
