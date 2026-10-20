#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace Birthright::Campaign
{
struct PoliticsRules
{
    std::uint32_t Version = 2;
    bool Enabled = false;
    std::uint32_t TickDays = 30;
    double PowerPerHolding = 20;
    double FriendshipDecay = 0.015;
    double LoyaltyResponse = 0.15;
    double FactionThreshold = 0.35;
    double ReconcileThreshold = 0.60;
    std::uint32_t GrievanceTicks = 6;
    std::uint32_t RebellionTicks = 12;
    double RebellionShare = 0.25;
    double FamilyIncomeShare = 0.2;
    double LevyFraction = 0.02;
    double MusterCost = 0.1;
    double SoldierUpkeep = 0.05;
    double CasualtyFraction = 0.12;
    std::uint32_t WarRounds = 6;
    std::uint32_t PeaceDays = 1825;
    double AnnualWarChance = 0.15;
    bool RequireCurrentGrievance = true;
};
struct FamilyEstate
{
    std::uint64_t Founder = 0;
    double Wealth = 0, Received = 0, Spent = 0;
    double StandardOfLiving = 0;
    std::int64_t LastPurchaseDay = -1000000;
};
struct LordRelationship
{
    std::uint64_t Domain = 0, Liege = 0;
    double Friendship = 0.35, Loyalty = 0.65;
    std::uint32_t Grievance = 0;
};
enum class FactionStatus : std::uint32_t { Organizing, Rebelled, Resolved };
struct PoliticalFaction
{
    std::uint64_t Key = 0, Domain = 0, Leader = 0;
    std::vector<std::uint64_t> Members;
    std::int64_t CreatedDay = 0;
    FactionStatus Status = FactionStatus::Organizing;
};
struct ArmyContingent
{
    std::uint64_t Key = 0, War = 0, Domain = 0, Lord = 0, HomeArea = 0, Culture = 0;
    std::uint32_t Soldiers = 0;
    std::uint64_t Expedition = 0; // Exactly one mission: War or Expedition.
};
struct CampaignWar
{
    std::uint64_t Key = 0, Attacker = 0, Defender = 0, TargetRegion = 0;
    std::int64_t StartedDay = 0, LastBattleDay = 0;
    std::uint32_t Rounds = 0;
    bool Active = true, Rebellion = false;
};
struct PoliticsState
{
    PoliticsRules Policy;
    std::map<std::uint64_t, FamilyEstate> Families;
    std::map<std::uint64_t, std::uint64_t> CharacterFamilies;
    std::map<std::uint64_t, LordRelationship> Lords;
    std::map<std::uint64_t, PoliticalFaction> Factions;
    std::map<std::uint64_t, ArmyContingent> Armies;
    std::map<std::uint64_t, CampaignWar> Wars;
    std::map<std::uint64_t, std::int64_t> LastAttention, PeaceUntil;
    std::set<std::uint64_t> RetiredDomains;
    std::uint64_t NextFaction = 1, NextArmy = 1, NextWar = 1;
    std::uint64_t Levied = 0, Casualties = 0, Returned = 0;
};
}
