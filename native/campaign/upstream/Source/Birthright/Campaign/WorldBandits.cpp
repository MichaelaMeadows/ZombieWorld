#include "WorldBandits.h"
#include "PlayerProgression.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace Birthright::Campaign
{
namespace
{
Result Ok()
{
    return {true, "accepted", ""};
}
Result Bad(const std::string &why)
{
    return {false, "bandit_invalid", why};
}
bool Range(double v, double lo, double hi)
{
    return std::isfinite(v) && v >= lo && v <= hi;
}
double Population(const Area &a)
{
    double n = 0;
    for (const auto &c : a.Cohorts)
        n += static_cast<double>(c.Population);
    return n;
}
std::uint64_t Draw(const Snapshot &s, Id id)
{
    auto x =
        id ^ (static_cast<std::uint64_t>(s.Seed) << 32) ^ (static_cast<std::uint64_t>(s.Day) * 0x9e3779b97f4a7c15ULL);
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
std::vector<Id> Targets(const Snapshot &s, Id area)
{
    std::vector<Id> result;
    for (Id id : BanditReach(s, area, s.Bandits.Policy.RaidHops))
    {
        const auto &a = s.Areas.at(id);
        if (a.RegionId && !a.Wilderness && Population(a) > 0)
            result.push_back(id);
    }
    return result;
}
} // namespace
std::vector<Id> BanditReach(const Snapshot &s, Id area, unsigned hops)
{
    if (!s.Areas.count(area) || s.Areas.at(area).IsWater)
        return {};
    std::set<Id> seen{area};
    std::vector<Id> wave{area};
    for (unsigned step = 0; step < hops; ++step)
    {
        std::vector<Id> next;
        for (Id id : wave)
            for (Id n : s.Areas.at(id).Neighbors)
                if (s.Areas.count(n) && !s.Areas.at(n).IsWater && seen.insert(n).second)
                    next.push_back(n);
        wave = std::move(next);
    }
    return {seen.begin(), seen.end()};
}
double AreaSecurity(const Snapshot &s, Id id)
{
    const auto &a = s.Areas.at(id);
    double fighters = 0, n = Population(a);
    for (const auto &c : a.Cohorts)
        fighters += static_cast<double>(c.Population) * s.Cultures.at(c.CultureId).Occupations.Roles[0];
    return std::clamp(a.Development * 0.6 + (a.RegionId ? 0.25 : 0) + (n > 0 ? fighters / n : 0) * 0.5 - a.Unrest * 0.4,
                      0.0, 1.0);
}
Result ValidateBandits(const Snapshot &s)
{
    const auto &b = s.Bandits;
    const auto &p = b.Policy;
    if (p.Version != 2 || !p.SpawnDays || p.SpawnDays > 3650 || !p.RaidDays || p.RaidDays > 365 || !p.RecoveryDays ||
        p.RecoveryDays > 3650 || !p.RaidHops || p.RaidHops > 4 || !p.SpacingHops || p.SpacingHops > 8 || !p.MaxCamps ||
        p.MaxCamps > 64 || !Range(p.InitialStrength, 3, 24) || !Range(p.MaximumStrength, p.InitialStrength, 24) ||
        p.DailyGrowth != 0 || !Range(p.RaidGrowth, 0, 1) || !Range(p.RaidStockFraction, 0, 0.1) ||
        !Range(p.FoodPerStrength, 0, 100) || !Range(p.MaximumDevelopment, 0, 1))
        return Bad("Policy bounds");
    if(!Range(p.FoodUpkeep,0.001,10) || !Range(p.GrowthFood,1,10000) || !Range(p.StarvationLoss,0.001,1) || !Range(p.StoragePerStrength,1,365) || !p.ResponseDays || p.ResponseDays>365 || !p.DefenseBattleDays || p.DefenseBattleDays>30 || !p.DefenseMaxRounds || p.DefenseMaxRounds>12 || !p.DefenseCooldownDays || p.DefenseCooldownDays>3650 || !Range(p.DefenseStrengthRatio,1,4) || !Range(p.DefenseWealthReserve,0,100000) || !Range(p.DefenseFoodReserveDays,0,90)) return Bad("Resource/response policy bounds");
    if (!b.NextId || b.NextId > 1000000000ULL || b.Camps.size() > 100000 ||
        (!p.Enabled && (!b.Camps.empty() || !b.Engagement.EncounterId.empty() || !b.Expeditions.empty() || !b.FoodTakenByArea.empty())) ||
        (p.Enabled && !s.Evolution.Policy.Enabled))
        return Bad("State requires evolution and bounded identity history");
    unsigned active = 0;
    std::set<Id> locations;
    for (const auto &row : b.Camps)
    {
        const auto &c = row.second;
        if (c.Active)
            ++active;
        if (!c.Key || c.Key != row.first || c.Key >= b.NextId || !s.Areas.count(c.AreaId) ||
            s.Areas.at(c.AreaId).IsWater || c.BornDay < 0 || c.BornDay > s.Day || c.LastRaidDay < c.BornDay ||
            c.LastRaidDay > s.Day || c.ClearedDay < 0 || c.ClearedDay > s.Day || c.SuppressedUntil < 0 ||
            c.SuppressedUntil > s.Day + 3650 || !Range(c.Strength, 0, p.MaximumStrength) ||
            !Range(c.FoodRaided, 0, 1e22) || c.Raids > 1000000000ULL || (!c.Active && c.ClearedDay < c.BornDay) ||
            (c.Active && (c.Strength <= 0 || !locations.insert(c.AreaId).second)))
            return Bad("Camp=" + std::to_string(row.first));
        for(double value:{c.FoodStored,c.FoodConsumed,c.FoodSpoiled,c.FoodWasted,c.FoodAbandoned,c.LegacyFoodConsumed,c.LootGrowth,c.StarvationLosses}) if(!Range(value,0,1e22)) return Bad("Camp resource bounds");
        if(c.FoodSpoiled+c.FoodAbandoned>c.FoodWasted+1e-8*std::max(1.0,c.FoodWasted) || c.Outcome>3 || (c.Active!=(c.Outcome==0)) || (!c.Active && c.FoodStored!=0) || (c.LastTargetArea && !s.Evolution.Areas.count(c.LastTargetArea)) || std::abs(c.FoodRaided-c.LegacyFoodConsumed-c.FoodStored-c.FoodConsumed-c.FoodWasted)>1e-8*std::max(1.0,c.FoodRaided)) return Bad("Camp food ledger="+std::to_string(c.Key));
    }
    if (active > p.MaxCamps)
        return Bad("Active camp cap");
    double captured=0, legacy=0, consumed=0, exports=0;
    for(const auto& c:b.Camps) { captured+=c.second.FoodRaided-c.second.LegacyFoodConsumed; legacy+=c.second.LegacyFoodConsumed; }
    for(const auto& a:s.Evolution.Areas) consumed+=a.second.Consumed;
    for(const auto& a:b.FoodTakenByArea) { if(!s.Evolution.Areas.count(a.first) || !Range(a.second,0,1e22)) return Bad("Invalid raided area ledger"); exports+=a.second; }
    if(std::abs(captured-exports)>1e-8*std::max(1.0,exports) || legacy>consumed+1e-7*std::max(1.0,consumed)) return Bad("Captured food does not reconcile with victim transfers");
    const auto defense=ValidateDefense(s); if(!defense) return defense;
    const auto &e = b.Engagement;
    if (!e.EncounterId.empty())
    {
        const auto c = b.Camps.find(e.CampId), end = b.Camps.end();
        const auto leader = s.Characters.find(e.LeaderId);
        if (e.EncounterId.size() > 128 || c == end || !c->second.Active || leader == s.Characters.end() ||
            !leader->second.Alive || leader->second.AreaId != e.AreaId || c->second.AreaId != e.AreaId ||
            e.Day != s.Day || e.Enemies < 3 || e.Enemies > 24 || e.Enemies != CampEncounter(s, e.CampId).Enemies.size())
            return Bad("Engagement=" + e.EncounterId);
    }
    else if (e.CampId || e.LeaderId || e.AreaId || e.Day || e.Enemies)
        return Bad("Empty engagement has data");
    return Ok();
}
Result World::ConfigureBandits(const std::string &operation, const BanditRules &p)
{
    auto checked = CheckOperation(operation);
    if (!checked)
        return checked;
    if (!p.Enabled || StateData.Bandits.Policy.Enabled || !StateData.Evolution.Policy.Enabled)
        return Bad("Enable once after evolution");
    auto next = BeginMutation();
    next.Bandits.Policy = p;
    next.Events.push_back({operation, "bandits_enabled", next.Day, 0, 0, "frontier camps enabled"});
    return Commit(std::move(next));
}
Result AdvanceBandits(World &world)
{
    auto &s = world.StateData;
    auto &b = s.Bandits;
    const auto &p = b.Policy;
    if (!p.Enabled)
        return Ok();
    for (auto &row : b.Camps)
    {
        auto &c = row.second;
        if (!c.Active) continue;
        const double spoiled=c.FoodStored*(-std::expm1(std::log1p(-s.Evolution.Policy.AnnualFoodSpoilage)/365));
        c.FoodStored-=spoiled; c.FoodSpoiled+=spoiled; c.FoodWasted+=spoiled;
        const double need=c.Strength*p.FoodUpkeep, eaten=std::min(c.FoodStored,need);
        c.FoodStored-=eaten; c.FoodConsumed+=eaten;
        if(eaten<need) { const double lost=std::min(c.Strength,p.StarvationLoss*(1-eaten/need)); c.Strength-=lost;c.StarvationLosses+=lost; }
        if(c.Strength<0.5) { CloseBanditCamp(s,c.Key,3); s.Events.push_back({"bandits/"+std::to_string(s.Day)+"/disperse/"+std::to_string(c.Key),"bandit_dispersed",s.Day,0,0,"camp="+std::to_string(c.Key)+"; reason=food_exhausted"});continue; }
        if(s.Day<c.SuppressedUntil) continue;
        if (s.Day - c.LastRaidDay < static_cast<std::int64_t>(p.RaidDays))
            continue;
        c.LastRaidDay = s.Day;
        Id target = 0;
        double best = 0;
        for (Id id : Targets(s, c.AreaId))
        {
            const double score = s.Evolution.Areas.at(id).FoodStored * (1 - AreaSecurity(s, id) * 0.75);
            if (score > best)
            {
                best = score;
                target = id;
            }
        }
        if (!target)
            continue;
        auto &food = s.Evolution.Areas.at(target);
        const double stolen = std::min(food.FoodStored * p.RaidStockFraction, c.Strength * p.FoodPerStrength) *
                              (1 - AreaSecurity(s, target) * 0.75);
        if (stolen <= 0)
            continue;
        food.FoodStored -= stolen;
        b.FoodTakenByArea[target] += stolen;
        c.FoodStored += stolen; c.LastTargetArea=target;
        c.FoodRaided += stolen;
        ++c.Raids;
        const double reserve=c.Strength*p.FoodUpkeep*p.RaidDays;
        const double growth=std::min({p.RaidGrowth,p.MaximumStrength-c.Strength,std::max(0.0,c.FoodStored-reserve)/p.GrowthFood});
        c.Strength+=growth;c.LootGrowth+=growth;c.FoodStored-=growth*p.GrowthFood;c.FoodConsumed+=growth*p.GrowthFood;
        const double excess=std::max(0.0,c.FoodStored-c.Strength*p.StoragePerStrength);c.FoodStored-=excess;c.FoodWasted+=excess;
        auto &area = s.Areas.at(target);
        area.Unrest = std::min(1.0, area.Unrest + 0.005);
        s.Events.push_back({"bandits/" + std::to_string(s.Day) + "/raid/" + std::to_string(c.Key), "bandit_raid", s.Day,
                            0, 0,
                            "camp=" + std::to_string(c.Key) + " area=" + std::to_string(target) +
                                " food_captured=" + std::to_string(stolen)+" growth="+std::to_string(growth)+" stored="+std::to_string(c.FoodStored)});
    }
    if (s.Day % p.SpawnDays)
        return AdvanceDefense(world);
    unsigned active = 0;
    for (const auto &c : b.Camps)
        if (c.second.Active)
            ++active;
    if (active >= p.MaxCamps || b.Camps.size() >= 100000)
        return AdvanceDefense(world);
    Id chosen = 0;
    double best = -1;
    for (const auto &row : s.Areas)
    {
        const auto &a = row.second;
        if (a.IsWater || a.Development > p.MaximumDevelopment ||
            (!a.Wilderness && a.RegionId && a.Claimants.size() < 2))
            continue;
        bool blocked = false;
        const auto near = BanditReach(s, a.Key, p.SpacingHops);
        for (const auto &c : b.Camps)
            if ((c.second.Active || s.Day < c.second.SuppressedUntil) &&
                std::binary_search(near.begin(), near.end(), c.second.AreaId))
            {
                blocked = true;
                break;
            }
        if (blocked || Targets(s, a.Key).empty())
            continue;
        const double jitter = static_cast<double>(Draw(s, a.Key) >> 11) / 9007199254740992.0;
        const double score = (1 - a.Development) + (1 - AreaSecurity(s, a.Key)) + (a.RegionId ? 0 : 1) + jitter;
        if (score > best)
        {
            best = score;
            chosen = a.Key;
        }
    }
    if (chosen)
    {
        BanditCamp c;
        c.Key = b.NextId++;
        c.AreaId = chosen;
        c.BornDay = c.LastRaidDay = s.Day;
        c.Strength = p.InitialStrength;
        b.Camps.emplace(c.Key, c);
        s.Events.push_back({"bandits/" + std::to_string(s.Day) + "/spawn/" + std::to_string(c.Key), "bandit_spawn",
                            s.Day, 0, 0, "camp=" + std::to_string(c.Key) + " area=" + std::to_string(chosen)});
    }
    return AdvanceDefense(world);
}
void CloseBanditCamp(Snapshot& s,Id camp,unsigned outcome)
{
    auto& c=s.Bandits.Camps.at(camp);c.Active=false;c.Strength=0;c.Outcome=outcome;
    c.ClearedDay=s.Day;c.SuppressedUntil=s.Day+s.Bandits.Policy.RecoveryDays;c.LastRaidDay=s.Day;
    c.FoodWasted+=c.FoodStored;c.FoodAbandoned+=c.FoodStored;c.FoodStored=0;
}
BanditEncounter CampEncounter(const Snapshot &s, Id camp)
{
    BanditEncounter result;
    const auto c = s.Bandits.Camps.find(camp);
    if (c == s.Bandits.Camps.end() || !c->second.Active)
        return result;
    result = MakeBanditEncounter(s.Seed ^ static_cast<std::uint32_t>(camp));
    result.Version = 2;
    const auto count = static_cast<std::size_t>(std::clamp(std::ceil(c->second.Strength), 3.0, 24.0));
    result.Enemies.resize(count, BanditKind::Raider);
    return result;
}
Result World::BeginCampAssault(const std::string &operation, Id leader, Id camp)
{
    auto checked = CheckOperation(operation);
    if (!checked)
        return checked;
    const auto &s = StateData;
    const auto c = s.Bandits.Camps.find(camp);
    const auto actor = s.Characters.find(leader);
    if (!s.Bandits.Policy.Enabled || !s.Bandits.Engagement.EncounterId.empty() || operation.size() > 128 ||
        c == s.Bandits.Camps.end() || !c->second.Active || actor == s.Characters.end() || !actor->second.Alive ||
        actor->second.AreaId != c->second.AreaId)
        return Bad("Assault requires living leader at active camp=" + std::to_string(camp));
    auto next = BeginMutation();
    next.Bandits.Engagement = {operation, camp,
                               leader,    actor->second.AreaId,
                               s.Day,     static_cast<std::uint32_t>(CampEncounter(s, camp).Enemies.size())};
    next.Events.push_back({operation, "bandit_assault", s.Day, leader, 0, "camp=" + std::to_string(camp)});
    return Commit(std::move(next));
}
Result World::AbandonCampAssault(const std::string &operation, Id leader)
{
    auto checked = CheckOperation(operation);
    if (!checked)
        return checked;
    const auto &e = StateData.Bandits.Engagement;
    if (e.EncounterId.empty() || e.LeaderId != leader)
        return Bad("No matching engagement to abandon");
    auto next = BeginMutation();
    next.Bandits.Engagement = {};
    next.Events.push_back(
        {operation, "bandit_assault_abandoned", next.Day, leader, 0, "No campaign victory or suppression awarded"});
    return Commit(std::move(next));
}
Result World::ApplyCampCombat(const CampCombatEvidence &proof)
{
    const auto &e = StateData.Bandits.Engagement;
    if (e.EncounterId.empty() || proof.EncounterId != e.EncounterId || proof.CampId != e.CampId ||
        proof.Enemies != e.Enemies || proof.Defeated > e.Enemies)
        return Bad("Combat evidence context mismatch");
    auto valid = ValidateBandits(StateData);
    if (!valid)
        return valid;
    const auto operation = e.EncounterId + "/result";
    auto checked = CheckOperation(operation);
    if (!checked)
        return checked;
    auto next = BeginMutation();
    auto &c = next.Bandits.Camps.at(e.CampId);
    const bool cleared = proof.HeroAlive && proof.Defeated == proof.Enemies;
    c.Strength = cleared ? 0 : std::max(1.0, c.Strength - proof.Defeated);
    c.Active = !cleared;
    c.ClearedDay = cleared ? next.Day : 0;
    if (cleared) RewardPlayerCampClear(next, e.LeaderId, c.Key);
    if(cleared) CloseBanditCamp(next,c.Key,1);
    if (cleared || proof.Defeated)
    {
        c.SuppressedUntil = next.Day + (cleared ? next.Bandits.Policy.RecoveryDays : next.Bandits.Policy.RaidDays);
        c.LastRaidDay = next.Day;
    }
    next.Events.push_back({operation, cleared ? "bandit_cleared" : "bandit_suppressed", next.Day, e.LeaderId, 0,
                           "camp=" + std::to_string(c.Key) + " defeated=" + std::to_string(proof.Defeated)});
    next.Bandits.Engagement = {};
    return Commit(std::move(next));
}
} // namespace Birthright::Campaign
