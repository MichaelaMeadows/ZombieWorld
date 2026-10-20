#pragma once
#include "../Campaign/PlayerProgression.h"
#include <sstream>

namespace Birthright::Campaign::Testing
{
// Shared production-policy checks run both without Unreal and in engine automation.
inline bool CheckPlayerProgression(std::string& failure)
{
#define BR013_CHECK(value, message) if (!(value)) { failure = message; return false; }
    Snapshot s; Generation g; g.Width = 10; g.Height = 4; g.DomainCount = 2;
    BR013_CHECK(Generate(g, {}, s), "fixture generation");
    Character player; player.Key = 10000; player.Name = "Player"; player.AreaId = 1;
    player.CultureId = s.Areas.at(1).Cohorts.front().CultureId; player.BornDay = -6570;
    s.Characters.emplace(player.Key, player);
    auto encoded = [](const Snapshot& state) { std::ostringstream o; Save(state, o); return o.str(); };
    for (const auto& origin : PlayerOrigins())
    {
        World w; BR013_CHECK(w.Restore(s), "restore");
        const auto before = encoded(w.State());
        BR013_CHECK(!w.ChooseOrigin("invalid", player.Key, "invented") && encoded(w.State()) == before, "invalid origin must reject atomically");
        BR013_CHECK(w.AwardFavor("legacy-favor", player.Key, Deity::Nature, 37, "existing play"), "legacy favor");
        BR013_CHECK(w.ChooseOrigin("origin", player.Key, origin.Key), "origin choice");
        const auto& c = w.State().Characters.at(player.Key); const auto path = static_cast<std::size_t>(origin.Path);
        BR013_CHECK(c.Parents.size() == 1 && c.Birthright[path].Released && c.Birthright[path].SourceParentId == c.Parents.front(), "recorded released ancestry");
        BR013_CHECK(c.Affinities[path].BirthrightPower == 1 && c.Affinities[path].EarnedFavor == 0 && c.Affinities[4].EarnedFavor == 37, "birthright distinct from earned favor");
        const auto chosen = encoded(w.State());
        BR013_CHECK(!w.ChooseOrigin("origin", player.Key, origin.Key) && !w.ChooseOrigin("origin-again", player.Key, PlayerOrigins()[0].Key) && encoded(w.State()) == chosen, "origin replay and exclusivity");
        const auto first = origin.Path == Deity::Strife ? "strife_sunburst" : "harmony_rally";
        const auto second = origin.Path == Deity::Strife ? "strife_lance" : "harmony_mend";
        BR013_CHECK(!w.LearnDivineNode("too-early", player.Key, second), "threshold");
        BR013_CHECK(!w.LearnDivineNode("duplicate", player.Key, first), "duplicate node");
        BR013_CHECK(w.AwardFavor("earned", player.Key, origin.Path, 150, "certified test reward"), "earned reward");
        BR013_CHECK(DivinePointBudget(w.State(), player.Key, origin.Path) == 2, "power point threshold");
        BR013_CHECK(w.LearnDivineNode("second", player.Key, second), "learn next node");
        BR013_CHECK(w.AssignDivineNode("assign", player.Key, 1, second), "assign learned ability");
        BR013_CHECK(!w.AssignDivineNode("invalid-slot", player.Key, 4, second) && !w.AssignDivineNode("unknown", player.Key, 1, "unknown"), "slot bounds and ownership");
        auto build = BuildPlayerCombat(w.State(), player.Key);
        BR013_CHECK(build.Version == 2 && build.Equipment[0] == "heir_sword" && build.Equipment[1] == "travel_coat" && build.Equipment[2] == "lineage_signet", "BR080 visible items share authoritative battle build");
        BR013_CHECK(!build.Abilities[0].empty() && !build.Abilities[1].empty() && build.BonusHealth == 20 && build.BonusEssence == 20 && build.BonusDamage == 4, "combat snapshot");
        BR013_CHECK(w.AssignDivineNode("move", player.Key, 3, second) && w.State().Players.at(player.Key).AbilitySlots[1].empty(), "moving assignment clears previous slot");
        const auto preRespec = w.State().Characters.at(player.Key);
        BR013_CHECK(w.RespecDivineNodes("respec", player.Key), "free respec");
        BR013_CHECK(w.State().Players.at(player.Key).Learned.empty() && BuildPlayerCombat(w.State(), player.Key).Abilities[3].empty() &&
            w.State().Characters.at(player.Key).Affinities[path].EarnedFavor == preRespec.Affinities[path].EarnedFavor && w.State().Characters.at(player.Key).Parents == preRespec.Parents, "respec preserves favor and ancestry");
        BR013_CHECK(!w.LearnDivineNode("missing-prerequisite", player.Key, second), "prerequisite");
        BR013_CHECK(w.LearnDivineNode("relearn", player.Key, first), "relearn after respec");
        BR013_CHECK(w.EquipPlayerItem("unequip", player.Key, EquipmentSlot::Armor, 0) && BuildPlayerCombat(w.State(), player.Key).BonusHealth == 0, "unequip affects build");
        BR013_CHECK(BuildPlayerCombat(w.State(), player.Key).Equipment[1].empty() && BuildPlayerCombat(w.State(), player.Key).Equipment[0] == "heir_sword", "BR080 unequip clears only its visible slot");
        BR013_CHECK(!w.EquipPlayerItem("wrong-slot", player.Key, EquipmentSlot::Armor, 1) && !w.EquipPlayerItem("not-owned", player.Key, EquipmentSlot::Armor, 999), "equipment ownership and compatibility");
        BR013_CHECK(w.EquipPlayerItem("reequip", player.Key, EquipmentSlot::Armor, 2), "reequip");
        BR013_CHECK(BuildPlayerCombat(w.State(), player.Key).Equipment[1] == "travel_coat", "BR080 reequip restores visual key");
        const auto offers = LocalRecruitment(w.State(),player.Key);
        Id culture=0; for (const auto& o:offers) if (o.Available>=2) { culture=o.CultureId; break; }
        BR013_CHECK(culture, "BR079 willing fixture recruits");
        const double favorBefore = w.State().Characters.at(player.Key).Affinities[path].EarnedFavor;
        BR013_CHECK(!w.TendWarband("empty",player.Key,1), "BR079 empty care rejected");
        BR013_CHECK(w.RecruitPeasants("recruit",player.Key,1,culture,2), "BR079 recruit");
        const Id troop=w.State().Troops.begin()->first;
        const auto day=w.State().Day;
        BR013_CHECK(w.TendWarband("train",player.Key,1,troop), "BR079 training");
        BR013_CHECK(IsTroopTrained(w.State(),troop) && w.State().Day==day+2, "BR079 stable training and time cost");
        const auto trained=encoded(w.State());
        BR013_CHECK(!w.TendWarband("train",player.Key,1) && !w.TendWarband("repeat",player.Key,1,troop) &&
            !w.TendWarband("remote",player.Key,2) && !w.TendWarband("foreign",player.Key,1,999999) && encoded(w.State())==trained,
            "BR079 rejection atomicity replay training ownership location");
        BR013_CHECK(w.TendWarband("care",player.Key,1) && w.State().Day==day+3, "BR079 care time");
        BR013_CHECK(w.State().Characters.at(player.Key).Affinities[path].EarnedFavor==favorBefore+(origin.Path==Deity::Harmony?24:0), "BR079 path-specific earning");
        const auto saved = encoded(w.State()); Snapshot loaded; std::istringstream in(saved);
        BR013_CHECK(Load(in, loaded) && encoded(loaded) == saved, "exact persistence");
        BR013_CHECK(IsTroopTrained(loaded,troop), "BR079 saved training");
        for (int corrupt = 0; corrupt < 5; ++corrupt)
        {
            auto invalid = w.State(); auto& p = invalid.Players.at(player.Key);
            if (corrupt == 0) p.ContentVersion = 99;
            if (corrupt == 1) p.Learned.push_back(first);
            if (corrupt == 2) p.Equipment[0] = 999;
            if (corrupt == 3) p.Ancestor = player.Key;
            if (corrupt == 4) p.AbilitySlots[0] = second;
            BR013_CHECK(!Validate(invalid), "corrupt progression rejected");
        }
        std::istringstream truncated(saved.substr(0, saved.size() - 5)); const auto oldOutput = encoded(loaded);
        BR013_CHECK(!Load(truncated, loaded) && encoded(loaded) == oldOutput, "corrupt load atomicity");
        auto dead = w.State(); dead.Characters.at(player.Key).Alive = false; dead.Characters.at(player.Key).DiedDay = dead.Day;
        BR013_CHECK(w.Restore(dead) && !w.RespecDivineNodes("dead", player.Key) && !w.EquipPlayerItem("dead-equip", player.Key, EquipmentSlot::Armor, 0), "life state boundary");
    }
    auto legacy = encoded(s); legacy.erase(legacy.find("PLAYER_PROGRESSION")); legacy.replace(0, 19, "BIRTHRIGHT_WORLD 10");
    Snapshot migrated; std::istringstream old(legacy);
    BR013_CHECK(Load(old, migrated) && migrated.Version == 11 && migrated.Players.empty() && migrated.Characters.size() == s.Characters.size(), "v10 migration preserves characters and awaits choice");
#undef BR013_CHECK
    return true;
}
}
