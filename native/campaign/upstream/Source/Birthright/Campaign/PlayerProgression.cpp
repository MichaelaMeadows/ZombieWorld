#include "PlayerProgression.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace Birthright::Campaign
{
const std::vector<OriginDefinition>& PlayerOrigins()
{
    static const std::vector<OriginDefinition> definitions = {
        {"ember_heir", "Heir of the Ember Oath", "Aren of the Ember Oath", Deity::Strife, 1, "Break frontier threats. Earn 75 favor for a certified camp clearance."},
        {"concord_heir", "Heir of the Concord", "Mira of the Concord", Deity::Harmony, 1, "Recruit and sustain followers: 5 favor per recruit, 2 per troop tended, 10 per troop trained."}
    };
    return definitions;
}
const std::vector<DivineNodeDefinition>& DivineNodes()
{
    static const std::vector<DivineNodeDefinition> definitions = {
        {"strife_sunburst", "Sunburst", Deity::Strife, 1, 1, "", "Burst"},
        {"strife_lance", "Lance", Deity::Strife, 2, 1, "strife_sunburst", "Bolt"},
        {"harmony_rally", "Divine rally", Deity::Harmony, 1, 1, "", "Rally"},
        {"harmony_mend", "Mend", Deity::Harmony, 2, 1, "harmony_rally", "Mend"}
    };
    return definitions;
}
const std::vector<ItemDefinition>& PlayerItems()
{
    static const std::vector<ItemDefinition> definitions = {
        {"heir_sword", "Heir's sword", EquipmentSlot::Weapon, 0, 0, 4},
        {"travel_coat", "Travel coat", EquipmentSlot::Armor, 20, 0, 0},
        {"lineage_signet", "Lineage signet", EquipmentSlot::Charm, 0, 20, 0}
    };
    return definitions;
}
const OriginDefinition* FindOrigin(const std::string& key) { for (const auto& d : PlayerOrigins()) if (key == d.Key) return &d; return nullptr; }
const DivineNodeDefinition* FindDivineNode(const std::string& key) { for (const auto& d : DivineNodes()) if (key == d.Key) return &d; return nullptr; }
const ItemDefinition* FindPlayerItem(const std::string& key) { for (const auto& d : PlayerItems()) if (key == d.Key) return &d; return nullptr; }
std::uint32_t DivinePointBudget(const Snapshot& s, Id player, Deity path)
{
    const auto c = s.Characters.find(player);
    if (c == s.Characters.end() || static_cast<std::size_t>(path) >= DeityCount) return 0;
    const double power = Power(c->second.Affinities[static_cast<std::size_t>(path)], s.Policy);
    return std::isfinite(power) ? static_cast<std::uint32_t>(std::clamp(std::floor(power + 1.e-8), 0.0, 1000000.0)) : 0;
}
std::uint32_t DivinePointsSpent(const PlayerProgression& p, Deity path)
{
    std::uint32_t spent = 0;
    for (const auto& key : p.Learned) if (const auto* d = FindDivineNode(key); d && d->Path == path) spent += d->Cost;
    return spent;
}
bool CanUseDivineNode(const Snapshot& s, Id player, const std::string& key)
{
    const auto p = s.Players.find(player); const auto c = s.Characters.find(player); const auto* d = FindDivineNode(key);
    if (!d || p == s.Players.end() || c == s.Characters.end() || !c->second.Alive) return false;
    return std::find(p->second.Learned.begin(), p->second.Learned.end(), key) != p->second.Learned.end() &&
        Power(c->second.Affinities[static_cast<std::size_t>(d->Path)], s.Policy) + 1.e-8 >= d->RequiredPower &&
        DivinePointsSpent(p->second, d->Path) <= DivinePointBudget(s, player, d->Path);
}
namespace
{
Result Bad(const char* code, Id player) { return {false, code, "player=" + std::to_string(player)}; }
bool Editable(const Snapshot& s, Id id)
{
    const auto c = s.Characters.find(id);
    return c != s.Characters.end() && c->second.Alive && s.Players.count(id) &&
        (s.Bandits.Engagement.EncounterId.empty() || s.Bandits.Engagement.LeaderId != id);
}
}
Result ValidatePlayerProgression(const Snapshot& s)
{
    for (const auto& entry : s.Players)
    {
        const auto id = entry.first; const auto& p = entry.second;
        const auto c = s.Characters.find(id); const auto* origin = FindOrigin(p.Origin);
        if (c == s.Characters.end() || !origin || p.ContentVersion != 1 || !p.Ancestor ||
            std::find(c->second.Parents.begin(), c->second.Parents.end(), p.Ancestor) == c->second.Parents.end() ||
            c->second.Birthright[static_cast<std::size_t>(origin->Path)].SourceParentId != p.Ancestor ||
            p.Items.size() > 256 || p.Learned.size() > DivineNodes().size() || !p.NextItemId)
            return Bad("player_record_invalid", id);
        std::set<std::string> learned, assigned;
        for (const auto& key : p.Learned) if (!FindDivineNode(key) || !learned.insert(key).second) return Bad("player_node_invalid", id);
        for (const auto& key : p.Learned) { const auto* d = FindDivineNode(key); if (*d->Prerequisite && !learned.count(d->Prerequisite)) return Bad("player_prerequisite_missing", id); }
        for (const auto& key : p.AbilitySlots) if (!key.empty() && (!learned.count(key) || !assigned.insert(key).second)) return Bad("player_slot_invalid", id);
        for (const auto& item : p.Items) if (!item.first || item.first != item.second.Key || item.first >= p.NextItemId || !FindPlayerItem(item.second.Definition)) return Bad("player_item_invalid", id);
        for (std::size_t slot = 0; slot < p.Equipment.size(); ++slot)
        {
            const auto item = p.Items.find(p.Equipment[slot]);
            if (p.Equipment[slot] && (item == p.Items.end() || static_cast<std::size_t>(FindPlayerItem(item->second.Definition)->Slot) != slot)) return Bad("player_equipment_invalid", id);
        }
    }
    return {true, "accepted", ""};
}
Result World::ChooseOrigin(const std::string& operation, Id player, const std::string& key)
{
    const auto checked = CheckOperation(operation); if (!checked) return checked;
    const auto* d = FindOrigin(key); const auto c = StateData.Characters.find(player);
    if (!d || c == StateData.Characters.end() || !c->second.Alive || StateData.Players.count(player) ||
        !c->second.Parents.empty() || !StateData.Bandits.Engagement.EncounterId.empty()) return Bad("origin_ineligible", player);
    // Legacy founders retain all earned/transferred favor. Never overwrite existing inherited provenance.
    for (const auto& a : c->second.Affinities) if (a.BirthrightPower != 0) return Bad("origin_existing_birthright", player);
    if (StateData.Characters.rbegin()->first == std::numeric_limits<Id>::max()) return Bad("origin_id_exhausted", player);
    auto next = StateData; auto& character = next.Characters.at(player);
    Character parent; parent.Key = next.Characters.rbegin()->first + 1; parent.Name = d->AncestorName;
    parent.AreaId = character.AreaId; parent.CultureId = character.CultureId;
    parent.BornDay = character.BornDay - next.Policy.AdultAgeDays; parent.Alive = false; parent.DiedDay = next.Day;
    const auto path = static_cast<std::size_t>(d->Path);
    parent.Affinities[path].BirthrightPower = d->Birthright;
    next.Characters.emplace(parent.Key, parent); character.Parents.push_back(parent.Key);
    character.Affinities[path].BirthrightPower = d->Birthright;
    character.Birthright[path] = {parent.Key, d->Birthright, true, FavorCost(d->Birthright, next.Policy)};
    next.Allocations.push_back({parent.Key, d->Path, FavorCost(d->Birthright, next.Policy), false});
    PlayerProgression profile; profile.Origin = key; profile.Ancestor = parent.Key;
    for (const auto& item : PlayerItems()) { const auto id = profile.NextItemId++; profile.Items.emplace(id, CarriedItem{id, item.Key}); profile.Equipment[static_cast<std::size_t>(item.Slot)] = id; }
    for (const auto& node : DivineNodes()) if (node.Path == d->Path && !*node.Prerequisite) { profile.Learned.push_back(node.Key); profile.AbilitySlots[0] = node.Key; break; }
    next.Players.emplace(player, std::move(profile));
    next.Events.push_back({operation, "player_origin", next.Day, player, parent.Key, key});
    return Commit(std::move(next));
}
Result World::LearnDivineNode(const std::string& operation, Id player, const std::string& key)
{
    const auto checked = CheckOperation(operation); if (!checked) return checked;
    const auto* d = FindDivineNode(key);
    if (!Editable(StateData, player) || !d) return Bad("node_ineligible", player);
    const auto& p = StateData.Players.at(player);
    const auto has = [&](const std::string& k) { return std::find(p.Learned.begin(), p.Learned.end(), k) != p.Learned.end(); };
    if (has(key) || (*d->Prerequisite && !has(d->Prerequisite)) ||
        Power(StateData.Characters.at(player).Affinities[static_cast<std::size_t>(d->Path)], StateData.Policy) + 1.e-8 < d->RequiredPower ||
        DivinePointsSpent(p, d->Path) + d->Cost > DivinePointBudget(StateData, player, d->Path)) return Bad("node_locked", player);
    auto next = StateData; next.Players.at(player).Learned.push_back(key);
    next.Events.push_back({operation, "player_learn", next.Day, player, 0, key}); return Commit(std::move(next));
}
Result World::RespecDivineNodes(const std::string& operation, Id player)
{
    const auto checked = CheckOperation(operation); if (!checked) return checked;
    if (!Editable(StateData, player)) return Bad("respec_ineligible", player);
    auto next = StateData; next.Players.at(player).Learned.clear(); next.Players.at(player).AbilitySlots = {};
    next.Events.push_back({operation, "player_respec", next.Day, player, 0, "Free ability respec; ancestry and favor preserved"}); return Commit(std::move(next));
}
Result World::AssignDivineNode(const std::string& operation, Id player, std::uint32_t slot, const std::string& key)
{
    const auto checked = CheckOperation(operation); if (!checked) return checked;
    if (!Editable(StateData, player) || slot >= 4 || (!key.empty() && !CanUseDivineNode(StateData, player, key))) return Bad("ability_slot_ineligible", player);
    auto next = StateData; auto& slots = next.Players.at(player).AbilitySlots;
    for (auto& existing : slots) if (existing == key) existing.clear(); slots[slot] = key;
    next.Events.push_back({operation, "player_assign", next.Day, player, 0, "slot=" + std::to_string(slot) + ";node=" + key}); return Commit(std::move(next));
}
Result World::EquipPlayerItem(const std::string& operation, Id player, EquipmentSlot slot, Id item)
{
    const auto checked = CheckOperation(operation); if (!checked) return checked;
    const auto index = static_cast<std::size_t>(slot);
    if (!Editable(StateData, player) || index >= 3) return Bad("equipment_ineligible", player);
    const auto& items = StateData.Players.at(player).Items; const auto found = items.find(item);
    if (item && (found == items.end() || FindPlayerItem(found->second.Definition)->Slot != slot)) return Bad("equipment_not_owned_or_wrong_slot", player);
    auto next = StateData; next.Players.at(player).Equipment[index] = item;
    next.Events.push_back({operation, "player_equip", next.Day, player, 0, "slot=" + std::to_string(index) + ";item=" + std::to_string(item)}); return Commit(std::move(next));
}
PlayerCombatBuild BuildPlayerCombat(const Snapshot& s, Id player)
{
    PlayerCombatBuild build; build.Character = player; const auto p = s.Players.find(player);
    if (p == s.Players.end()) return build;
    for (std::size_t i = 0; i < build.Abilities.size(); ++i) if (CanUseDivineNode(s, player, p->second.AbilitySlots[i])) build.Abilities[i] = FindDivineNode(p->second.AbilitySlots[i])->CombatAbility;
    for (std::size_t slot = 0; slot < p->second.Equipment.size(); ++slot)
        if (const auto id = p->second.Equipment[slot]) {
            const auto& key = p->second.Items.at(id).Definition;
            const auto* d = FindPlayerItem(key);
            build.Equipment[slot] = key;
            build.BonusHealth += d->Health; build.BonusEssence += d->Essence; build.BonusDamage += d->Damage;
        }
    return build;
}
bool IsTroopTrained(const Snapshot& s, Id troop)
{
    return std::any_of(s.Events.begin(), s.Events.end(), [troop](const auto& e) {
        return e.Kind == "player_train_v1" && e.OtherId == troop;
    });
}
void RewardPlayerHarmony(Snapshot& s, Id player, double favor)
{
    const auto p = s.Players.find(player);
    if (p == s.Players.end() || FindOrigin(p->second.Origin)->Path != Deity::Harmony) return;
    auto& affinity = s.Characters.at(player).Affinities[static_cast<std::size_t>(Deity::Harmony)];
    const auto room = FavorCost(1000000, s.Policy) - FavorCost(Power(affinity, s.Policy), s.Policy);
    affinity.EarnedFavor += std::clamp(room, 0.0, favor);
}
Result World::TendWarband(const std::string& operation, Id leader, Id expectedArea, Id trainTroop)
{
    const auto checked = CheckOperation(operation); if (!checked) return checked;
    if (!Editable(StateData, leader) || StateData.Characters.at(leader).AreaId != expectedArea || !TroopCount(StateData, leader))
        return Bad("warband_activity_ineligible", leader);
    if (trainTroop)
    {
        const auto t = StateData.Troops.find(trainTroop);
        if (t == StateData.Troops.end() || t->second.LeaderId != leader || IsTroopTrained(StateData, trainTroop))
            return Bad("training_ineligible", leader);
    }
    // BR-079: campaign time is the prototype cost; stage all changes atomically.
    World candidate; auto result = candidate.Restore(StateData); if (!result) return result;
    result = candidate.AdvanceDays(trainTroop ? 2 : 1); if (!result) return result;
    auto next = candidate.State();
    if (!next.Characters.at(leader).Alive) return Bad("warband_activity_interrupted", leader);
    RewardPlayerHarmony(next, leader, trainTroop ? 10 : 2 * TroopCount(next, leader));
    next.Events.push_back({operation, trainTroop ? "player_train_v1" : "player_tend_v1", next.Day, leader, trainTroop,
        "area=" + std::to_string(expectedArea) + ";days=" + (trainTroop ? "2" : "1")});
    return Commit(std::move(next));
}
void RewardPlayerCampClear(Snapshot& s, Id player, Id camp)
{
    const auto p = s.Players.find(player); if (p == s.Players.end()) return;
    const auto* origin = FindOrigin(p->second.Origin);
    if (origin->Path != Deity::Strife) return;
    // Camp IDs are persistent and never reused. Retry or repeated evidence cannot farm rewards.
    const std::string operation = "player-camp-favor/" + std::to_string(camp);
    for (const auto& event : s.Events) if (event.OperationId == operation) return;
    auto& affinity = s.Characters.at(player).Affinities[static_cast<std::size_t>(origin->Path)];
    const auto room = FavorCost(1000000, s.Policy) - FavorCost(Power(affinity, s.Policy), s.Policy);
    affinity.EarnedFavor += std::clamp(room, 0.0, 75.0);
    s.Events.push_back({operation, "player_camp_favor", s.Day, player, 0, "camp=" + std::to_string(camp) + ";path=" + DeityName(origin->Path)});
}
}
