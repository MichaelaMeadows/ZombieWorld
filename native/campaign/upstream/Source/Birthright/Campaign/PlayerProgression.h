#pragma once
#include "World.h"

namespace Birthright::Campaign
{
struct OriginDefinition { const char* Key; const char* Name; const char* AncestorName; Deity Path; double Birthright; const char* Obligation; };
struct DivineNodeDefinition { const char* Key; const char* Name; Deity Path; double RequiredPower; std::uint32_t Cost; const char* Prerequisite; const char* CombatAbility; };
struct ItemDefinition { const char* Key; const char* Name; EquipmentSlot Slot; double Health; double Essence; double Damage; };
const std::vector<OriginDefinition>& PlayerOrigins();
const std::vector<DivineNodeDefinition>& DivineNodes();
const std::vector<ItemDefinition>& PlayerItems();
const OriginDefinition* FindOrigin(const std::string& key);
const DivineNodeDefinition* FindDivineNode(const std::string& key);
const ItemDefinition* FindPlayerItem(const std::string& key);
std::uint32_t DivinePointBudget(const Snapshot& state, Id player, Deity path);
std::uint32_t DivinePointsSpent(const PlayerProgression& player, Deity path);
bool CanUseDivineNode(const Snapshot& state, Id player, const std::string& node);
Result ValidatePlayerProgression(const Snapshot& state);
void WritePlayerProgression(const Snapshot& state, std::ostream& output);
Result ReadPlayerProgression(std::istream& input, Snapshot& state);
void ExportPlayerProgression(const Snapshot& state, std::ostream& output);
// Immutable battle input derived at entry/reset, schema and content versioned.
struct PlayerCombatBuild
{
    std::uint32_t Version = 2, ContentVersion = 1;
    Id Character = 0;
    std::array<std::string, 4> Abilities{};
    std::array<std::string, 3> Equipment{};
    double BonusHealth = 0, BonusEssence = 0, BonusDamage = 0;
};
PlayerCombatBuild BuildPlayerCombat(const Snapshot& state, Id player);
// Called only by certified persistent camp completion, atomically with that result.
bool IsTroopTrained(const Snapshot& state, Id troop);
void RewardPlayerHarmony(Snapshot& state, Id player, double favor);
void RewardPlayerCampClear(Snapshot& state, Id player, Id camp);
}
