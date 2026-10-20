#include "World.h"
#include "PlayerProgression.h"
#include "WorldTraversal.h"
#include "WorldEvolution.h"
#include "WorldPolitics.h"
#include "WorldMarket.h"
#include "WorldBandits.h"
#include "WorldTravel.h"
#include <sstream>
#include "MapGeography.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace Birthright::Campaign
{
namespace
{
constexpr double MaximumPower = 1000000;
constexpr std::int64_t MaximumDay = 100000000;
constexpr std::int64_t MaximumPopulation = 1000000000000LL;
Result Ok() { return { true, "accepted", "" }; }
Result Reject(const std::string& code, const std::string& detail) { return { false, code, detail }; }
bool Defined(Deity deity) { return static_cast<std::size_t>(deity) < DeityCount; }
bool Bounded(double value, double low, double high) { return std::isfinite(value) && value >= low && value <= high; }
bool Text(const std::string& value) { return !value.empty() && value.size() <= 1024; }
bool Fraction(double value) { return Bounded(value, 0, 1); }
bool Ancestor(const Snapshot& state, Id possibleAncestor, Id descendant)
{
    std::vector<Id> pending{ descendant };
    std::set<Id> visited;
    while (!pending.empty())
    {
        const auto id = pending.back();
        pending.pop_back();
        if (!visited.insert(id).second) continue;
        const auto found = state.Characters.find(id);
        if (found == state.Characters.end()) continue;
        for (const auto parent : found->second.Parents)
        {
            if (parent == possibleAncestor) return true;
            pending.push_back(parent);
        }
    }
    return false;
}
bool Adult(const Character& character, const Snapshot& state)
{
    return character.Alive && state.Day - character.BornDay >= state.Policy.AdultAgeDays;
}
void CompleteBirthright(Snapshot& state, Id parent)
{
    for (auto& item : state.Characters)
        if (item.second.Alive)
            for (std::size_t d = 0; d < DeityCount; ++d)
            {
                auto& grant = item.second.Birthright[d];
                if (grant.SourceParentId == parent && !grant.Released)
                {
                    auto& affinity = item.second.Affinities[d];
                    const auto remainder = FavorCost(grant.FullPower, state.Policy) - grant.GrantedFavor;
                    affinity.BirthrightPower = PowerFromFavor(FavorCost(affinity.BirthrightPower, state.Policy) + remainder, state.Policy);
                    grant.GrantedFavor = FavorCost(grant.FullPower, state.Policy);
                    grant.Released = true;
                }
            }
}
void ApplyDeath(Snapshot& state, Id character)
{
    auto& dead = state.Characters.at(character);
    dead.Alive = false; dead.DiedDay = state.Day;
    if (dead.SpouseId) state.Characters.at(dead.SpouseId).SpouseId = 0;
    dead.SpouseId = 0;
    for (auto& item : state.Entities) if (item.second.HeadCharacterId == character) item.second.HeadCharacterId = 0;
    for (auto& item : state.Domains) if (item.second.HeadCharacterId == character) item.second.HeadCharacterId = 0;
    CompleteBirthright(state, character);
}
template<class T> bool Keys(const std::map<Id, T>& values)
{
    for (const auto& item : values) if (!item.first || item.first != item.second.Key) return false;
    return true;
}
}

const char* DeityName(Deity deity)
{
    static constexpr const char* Names[] = { "Strife", "Harmony", "Death", "Life", "Nature", "Order", "Chaos" };
    return Defined(deity) ? Names[static_cast<std::size_t>(deity)] : "Invalid";
}

double FavorCost(double power, const Rules& rules)
{
    if (!Bounded(power, 0, MaximumPower)) return std::numeric_limits<double>::quiet_NaN();
    const double level = std::floor(power);
    return rules.BasePointCost * level + rules.PointCostGrowth * level * (level - 1) / 2
        + (power - level) * (rules.BasePointCost + rules.PointCostGrowth * level);
}

double PowerFromFavor(double favor, const Rules& rules)
{
    if (!Bounded(favor, 0, FavorCost(MaximumPower, rules))) return std::numeric_limits<double>::quiet_NaN();
    std::uint32_t low = 0;
    std::uint32_t high = static_cast<std::uint32_t>(MaximumPower);
    while (low < high)
    {
        const auto middle = low + (high - low + 1) / 2;
        if (FavorCost(middle, rules) <= favor) low = middle;
        else high = middle - 1;
    }
    return low == MaximumPower ? low : low + (favor - FavorCost(low, rules)) /
        (rules.BasePointCost + rules.PointCostGrowth * low);
}

double Power(const Affinity& affinity, const Rules& rules)
{
    return PowerFromFavor(FavorCost(affinity.BirthrightPower, rules) + affinity.EarnedFavor + affinity.TransferredFavor, rules);
}

Result ValidateRules(const Rules& rules)
{
    if (rules.Version != 1 || !Bounded(rules.BasePointCost, 0.001, 1000000) ||
        !Bounded(rules.PointCostGrowth, 0.001, 1000000) ||
        !Fraction(rules.PrimaryInheritance) || !Fraction(rules.SecondaryInheritance) ||
        !Fraction(rules.OtherInheritance) || !Fraction(rules.ImmediateBirthrightFraction) || !Fraction(rules.StrifeTransferFraction) ||
        !Bounded(rules.DailyBirthRate, 0, 0.01) || !Bounded(rules.DailyDeathRate, 0, 0.01) ||
        !Bounded(rules.DailyStewardFavor, 0, 1000000) || rules.AdultAgeDays < 1 || rules.AdultAgeDays > 100000)
        return Reject("rules_invalid", "Version, point costs, inheritance fractions, age or daily rates are invalid.");
    return Ok();
}

Result Validate(const Snapshot& state)
{
    if (state.Version != 11 || !Text(state.WorldId) || state.Day < 0 || state.Day > MaximumDay)
        return Reject("snapshot_header", "Unsupported version, world ID or day.");
    if (!Bounded(state.DayFraction,0,std::nextafter(1.0,0.0)) || (state.Day == MaximumDay && state.DayFraction != 0) ||
        state.TravelPolicy.Version != 1 || !Bounded(state.TravelPolicy.DaysPerEffort,0.001,1000))
        return Reject("travel_clock_invalid", "Use travel policy v1, days per effort 0.001..1000 and a finite day fraction in [0,1).");
    auto result = ValidateRules(state.Policy);
    if (!result) return result;
    result = ValidateGeometry(state.Geometry); if (!result) return result;
    if (!state.Geometry.Cells.empty())
    {
        if (state.Geometry.Cells.size() != state.Areas.size()) return Reject("geometry_area", "Every source cell must have exactly one area.");
        for (const auto& cell : state.Geometry.Cells)
            if (!state.Areas.count(cell.first) || state.Areas.at(cell.first).IsWater != (cell.second.Height < 20))
                return Reject("geometry_area", "Missing area or area type differs from source geography.");
    }
    const auto& connectivity = state.Connectivity;
    if (connectivity.Version != 1 || !Bounded(connectivity.InfluenceDailyResponse, 0.001, 1) ||
        !Bounded(connectivity.MigrationDailyFraction, 0, 0.02) ||
        connectivity.MigrationDailyFraction >= connectivity.InfluenceDailyResponse ||
        !Bounded(connectivity.SettlementFraction, 0.001, 1) || !Bounded(connectivity.LocalCultureWeight, 0.000001, 1) ||
        state.Connections.size() > 1000000)
        return Reject("connectivity_policy", "Invalid version/rates/bounds; influence must respond faster than migration.");
    if (state.Areas.empty() || state.Areas.size() > 100000 || state.Regions.empty() || state.Regions.size() > 100000 ||
        state.Domains.empty() || state.Domains.size() > 100000 || state.Entities.empty() || state.Entities.size() > 100000 ||
        state.Cultures.empty() || state.Cultures.size() > 10000 || state.Characters.size() > 1000000 || state.Events.size() > 1000000 || state.Allocations.size() > 7000000)
        return Reject("snapshot_counts", "World collection counts exceed prototype bounds or required topology is empty.");
    if (!Keys(state.Areas) || !Keys(state.Regions) || !Keys(state.Domains) || !Keys(state.Entities) ||
        !Keys(state.Characters) || !Keys(state.Cultures)) return Reject("identity_invalid", "Record keys must match nonzero IDs.");
    const auto patronsValid = [](Deity primary, Deity secondary, double weight) {
        return Defined(primary) && ((secondary == Deity::Count && weight == 1) ||
            (Defined(secondary) && secondary != primary && Bounded(weight, 0.001, 0.999)));
    };
    for (const auto& item : state.Cultures)
    {
        const auto& culture = item.second;
        double sum = 0;
        for (const auto share : culture.Occupations.Roles) { if (!Fraction(share)) return Reject("society_invalid", std::to_string(item.first)); sum += share; }
        if (!Text(culture.Name) || !patronsValid(culture.Patron, culture.SecondaryPatron, culture.PrimaryPatronWeight) ||
            std::abs(sum - 1) > 0.000001 || (culture.OriginDomainId && !state.Domains.count(culture.OriginDomainId)))
            return Reject("culture_invalid", std::to_string(item.first));
    }
    const auto headValid = [&](Id id) { const auto c = state.Characters.find(id); return !id || (c != state.Characters.end() && c->second.Alive); };
    for (const auto& item : state.Domains)
    {
        const auto& domain = item.second;
        if (!Text(domain.Name) || !headValid(domain.HeadCharacterId) ||
            !Bounded(domain.InfluenceBudget, 0, 4000000) ||
            (domain.InfluenceBudget && (!domain.CapitalAreaId || domain.InfluenceBudget < 0.001)) ||
            (domain.PrimaryPatron == Deity::Count ? (domain.SecondaryPatron != Deity::Count || domain.PrimaryPatronWeight != 1) :
                !patronsValid(domain.PrimaryPatron, domain.SecondaryPatron, domain.PrimaryPatronWeight)))
            return Reject("domain_invalid", std::to_string(item.first));
        if (domain.CapitalAreaId)
        {
            const auto capital = state.Areas.find(domain.CapitalAreaId);
            if (capital == state.Areas.end() || !state.Regions.count(capital->second.RegionId) ||
                state.Regions.at(capital->second.RegionId).DomainId != item.first)
                return Reject("capital_invalid", std::to_string(item.first));
        }
    }
    for (const auto& item : state.Entities)
        if (!Text(item.second.Name) || !headValid(item.second.HeadCharacterId)) return Reject("entity_invalid", std::to_string(item.first));
    std::set<Id> usedRegions;
    std::set<Id> usedDomains;
    std::set<Id> usedEntities;
    std::map<Id, std::size_t> regionSizes;
    std::set<std::pair<std::int32_t, std::int32_t>> coordinates;
    for (const auto& item : state.Regions)
    {
        const auto& region = item.second;
        if (!state.Domains.count(region.DomainId) || !state.Entities.count(region.OwnerEntityId))
            return Reject("region_reference", std::to_string(item.first));
        usedDomains.insert(region.DomainId);
        usedEntities.insert(region.OwnerEntityId);
    }
    std::int64_t worldPopulation = 0;
    for (const auto& item : state.Areas)
    {
        const auto& area = item.second;
        if (area.IsWater)
        {
            if (state.Geometry.Cells.empty() || area.RegionId || !area.Neighbors.empty() || !area.Cohorts.empty() ||
                area.FoodCapacity || area.PopulationCapacity || area.SettlementSuitability != 0 || area.Development != 0 ||
                area.Prosperity != 0 || area.Unrest != 0 || area.Wilderness || !area.Claimants.empty() ||
                !area.Influences.empty() || !area.TargetInfluences.empty() || !Bounded(area.DiffusionCost,0.01,1000))
                return Reject("water_area_invalid", "Water cannot contain land population, ownership, influence or walking links.");
            continue;
        }
        if ((area.RegionId && !state.Regions.count(area.RegionId)) ||
            (state.Geometry.Cells.empty() && !coordinates.insert({ area.X, area.Y }).second) ||
            !Bounded(area.Prosperity, 0, 1) || !Fraction(area.Unrest) ||
            !Fraction(area.SettlementSuitability) || !Bounded(area.DiffusionCost, 0.01, 1000) ||
            !Fraction(area.Development) || area.PopulationCapacity < 1 || area.PopulationCapacity > 1000000000 ||
            area.FoodCapacity < (state.Evolution.Policy.Enabled ? 0 : 1) || area.FoodCapacity > MaximumPopulation || area.Cohorts.empty() ||
            area.Cohorts.size() > state.Cultures.size()) return Reject("area_invalid", std::to_string(item.first));
        if (area.RegionId) { usedRegions.insert(area.RegionId); ++regionSizes[area.RegionId]; }
        std::set<Id> claimants;
        for (const auto id : area.Claimants)
            if (!state.Domains.count(id) || !claimants.insert(id).second) return Reject("claim_invalid", std::to_string(item.first));
        if ((area.RegionId && claimants.size() >= 2) || (!area.RegionId && claimants.size() == 1) || (area.RegionId && !claimants.empty() &&
            !claimants.count(state.Regions.at(area.RegionId).DomainId))) return Reject("claim_invalid", std::to_string(item.first));
        std::set<Id> influences;
        for (const auto& influence : area.Influences)
            if (!state.Domains.count(influence.DomainId) || !Bounded(influence.Strength, 0.000000001, 1) ||
                !influences.insert(influence.DomainId).second) return Reject("influence_invalid", std::to_string(item.first));
        influences.clear();
        for (const auto& influence : area.TargetInfluences)
            if (!state.Domains.count(influence.DomainId) || !Bounded(influence.Strength, 0.000000001, 1) ||
                !influences.insert(influence.DomainId).second) return Reject("target_influence_invalid", std::to_string(item.first));
        std::set<Id> neighbors;
        for (const auto id : area.Neighbors)
        {
            const auto neighbor = state.Areas.find(id);
            if (id == item.first || !neighbors.insert(id).second || neighbor == state.Areas.end() || neighbor->second.IsWater ||
                std::find(neighbor->second.Neighbors.begin(), neighbor->second.Neighbors.end(), item.first) == neighbor->second.Neighbors.end())
                return Reject("adjacency_invalid", std::to_string(item.first));
        }
        std::set<Id> cultures;
        for (const auto& cohort : area.Cohorts)
        {
            if (!state.Cultures.count(cohort.CultureId) || !cultures.insert(cohort.CultureId).second ||
                cohort.Population < 0 || cohort.Population > MaximumPopulation ||
                !Bounded(cohort.BirthRemainder, 0, std::nextafter(1.0, 0.0)) ||
                !Bounded(cohort.DeathRemainder, 0, std::nextafter(1.0, 0.0)))
                return Reject("cohort_invalid", std::to_string(item.first));
            if (!Bounded(cohort.MigrationRemainder, 0, std::nextafter(1.0, 0.0)))
                return Reject("migration_remainder", std::to_string(item.first));
            worldPopulation += cohort.Population; // 100k areas * 10k cohorts * 1e12 would overflow; stop early.
            if (worldPopulation > MaximumPopulation) return Reject("population_limit", "Total population exceeds 1e12.");
        }
    }
    for (const auto& item : state.Connections)
    {
        const auto& link = item.second;
        if (!link.FirstAreaId || link.FirstAreaId >= link.SecondAreaId ||
            item.first != std::make_pair(link.FirstAreaId, link.SecondAreaId) ||
            !state.Areas.count(link.FirstAreaId) || !state.Areas.count(link.SecondAreaId) ||
            !Fraction(link.RoadQuality) || !Bounded(link.DistanceScale, 0.01, 1000))
            return Reject("traversal_connection", "Use canonical existing adjacent endpoints, road quality 0..1 and distance scale 0.01..1000.");
        const auto& neighbors = state.Areas.at(link.FirstAreaId).Neighbors;
        if (std::find(neighbors.begin(), neighbors.end(), link.SecondAreaId) == neighbors.end())
            return Reject("traversal_connection", "Roads/crossings must follow authored adjacency.");
    }
    if (usedRegions.size() != state.Regions.size() || usedDomains.size() + state.Politics.RetiredDomains.size() != state.Domains.size() || usedEntities.size() != state.Entities.size())
        return Reject("empty_ownership", "Every region, political domain and owning entity must own territory.");
    // Seeded empires require connected regions. Legacy worlds retain their original topology policy.
    std::set<Id> regionalVisited;
    for (const auto& item : state.Areas)
    {
        if (!item.second.RegionId || !state.Domains.at(state.Regions.at(item.second.RegionId).DomainId).CapitalAreaId ||
            regionalVisited.count(item.first)) continue;
        std::vector<Id> regionalPending{ item.first };
        std::size_t size = 0;
        while (!regionalPending.empty())
        {
            const auto id = regionalPending.back(); regionalPending.pop_back();
            if (!regionalVisited.insert(id).second) continue;
            ++size;
            for (const auto neighbor : state.Areas.at(id).Neighbors)
                if (state.Areas.at(neighbor).RegionId == item.second.RegionId) regionalPending.push_back(neighbor);
        }
        if (size != regionSizes.at(item.second.RegionId)) return Reject("region_disconnected", std::to_string(item.second.RegionId));
    }
    std::set<Id> reached;
    std::vector<Id> pending{ state.Areas.begin()->first };
    while (!pending.empty())
    {
        const auto id = pending.back(); pending.pop_back();
        if (!reached.insert(id).second) continue;
        for (const auto neighbor : state.Areas.at(id).Neighbors) pending.push_back(neighbor);
    }
    if (state.Geometry.Cells.empty() && reached.size() != state.Areas.size()) return Reject("map_disconnected", "Grid area graph must be connected.");
    for (const auto& item : state.Characters)
    {
        const auto& character = item.second;
        if (!Text(character.Name) || !state.Areas.count(character.AreaId) || state.Areas.at(character.AreaId).IsWater || !state.Cultures.count(character.CultureId) ||
            character.BornDay < -MaximumDay || character.BornDay > state.Day ||
            (!character.Alive && (character.DiedDay < character.BornDay || character.DiedDay > state.Day)) ||
            (character.Alive && character.DiedDay != 0) || character.Parents.size() > 2)
            return Reject("character_invalid", std::to_string(item.first));
        std::set<Id> parents;
        for (const auto parentId : character.Parents)
        {
            const auto parent = state.Characters.find(parentId);
            if (!parents.insert(parentId).second || parent == state.Characters.end() ||
                parent->second.BornDay >= character.BornDay)
                return Reject("lineage_invalid", std::to_string(item.first));
        }
        if (character.SpouseId)
        {
            const auto spouse = state.Characters.find(character.SpouseId);
            if (!Adult(character, state) || spouse == state.Characters.end() || !Adult(spouse->second, state) ||
                spouse->second.SpouseId != item.first || character.SpouseId == item.first ||
                Ancestor(state, character.SpouseId, item.first) || Ancestor(state, item.first, character.SpouseId))
                return Reject("marriage_invalid", std::to_string(item.first));
        }
        for (const auto& affinity : character.Affinities)
            if (!Bounded(affinity.BirthrightPower, 0, MaximumPower) || !Bounded(affinity.EarnedFavor, 0, FavorCost(MaximumPower, state.Policy)) ||
                !Bounded(affinity.TransferredFavor, 0, FavorCost(MaximumPower, state.Policy)) ||
                !Bounded(Power(affinity, state.Policy), 0, MaximumPower)) return Reject("affinity_invalid", std::to_string(item.first));
        for (std::size_t d = 0; d < DeityCount; ++d)
        {
            const auto& grant = character.Birthright[d];
            if (!Bounded(grant.FullPower, 0, MaximumPower) ||
                !Bounded(grant.GrantedFavor, 0, FavorCost(grant.FullPower, state.Policy) + 0.000001) ||
                (grant.SourceParentId && std::find(character.Parents.begin(), character.Parents.end(), grant.SourceParentId) == character.Parents.end()) ||
                (!grant.SourceParentId && (grant.FullPower != 0 || grant.Released || grant.GrantedFavor != 0)) ||
                (grant.Released && std::abs(grant.GrantedFavor - FavorCost(grant.FullPower, state.Policy)) > 0.000001) ||
                (grant.SourceParentId && FavorCost(character.Affinities[d].BirthrightPower, state.Policy) > grant.GrantedFavor + 0.000001) ||
                (character.Alive && grant.SourceParentId && !grant.Released && !state.Characters.at(grant.SourceParentId).Alive))
                return Reject("birthright_invalid", std::to_string(item.first));
        }
    }
    std::map<std::pair<Id, Deity>, std::pair<double, std::size_t>> grants;
    for (const auto& item : state.Characters)
        for (std::size_t d = 0; d < DeityCount; ++d)
        {
            const auto& grant = item.second.Birthright[d];
            if (!grant.SourceParentId) continue;
            auto& total = grants[{ grant.SourceParentId, static_cast<Deity>(d) }];
            total.first += FavorCost(grant.FullPower, state.Policy); ++total.second;
        }
    std::set<std::pair<Id, Deity>> allocations;
    for (const auto& allocation : state.Allocations)
    {
        const auto key = std::make_pair(allocation.ParentId, allocation.Path);
        const auto total = grants.find(key);
        if (!state.Characters.count(allocation.ParentId) || !Defined(allocation.Path) ||
            !Bounded(allocation.FavorBudget, 0, FavorCost(MaximumPower, state.Policy)) ||
            !allocations.insert(key).second || total == grants.end() ||
            total->second.first > allocation.FavorBudget + 0.000001 ||
            (!allocation.SplitAuthorized && total->second.second > 1))
            return Reject("allocation_invalid", "Duplicate parent-deity grant or overcommitted favor budget.");
    }
    if (allocations.size() != grants.size()) return Reject("allocation_missing", "Each birthright requires a parent-deity reservation.");
    std::set<std::string> operations;
    for (const auto& event : state.Events)
        if (!Text(event.OperationId) || !operations.insert(event.OperationId).second || !Text(event.Kind) ||
            event.Day < 0 || event.Day > state.Day || (!state.Characters.count(event.ActorId) && !((event.Kind == "evolution_enabled" || event.Kind == "politics_enabled" || event.Kind == "market_enabled" || event.Kind == "bandits_enabled" || event.Kind == "bandit_spawn" || event.Kind == "bandit_raid" || event.Kind == "bandit_dispersed" || event.Kind == "defense_deferred" || event.Kind == "war_decision" || event.Kind == "public_budget") && event.ActorId == 0 && event.OtherId == 0)) ||
            (event.OtherId && !state.Characters.count(event.OtherId)) || !Text(event.Detail))
            return Reject("event_invalid", event.OperationId);
    result = ValidateWarbands(state); if (!result) return result;
    result = ValidateEvolution(state); if (!result) return result;
    result = ValidatePolitics(state); if (!result) return result;
    result = ValidateMarket(state); if (!result) return result;
    result = ValidatePlayerProgression(state); if (!result) return result;
    return ValidateBandits(state);
}

Result Generate(const Generation& options, const Rules& rules, Snapshot& output)
{
    auto result = ValidateRules(rules);
    if (!result) return result;
    if (!Text(options.WorldId) || !options.Width || !options.Height ||
        static_cast<std::uint64_t>(options.Width) * options.Height > 100000 ||
        !options.RegionWidth || !options.RegionHeight || options.Width % options.RegionWidth ||
        options.Height % options.RegionHeight || !options.DomainCount || options.DomainCount > options.Width / options.RegionWidth)
        return Reject("generation_invalid", "Map must be bounded, tile whole regions and have one region column per domain.");
    Snapshot next;
    next.WorldId = options.WorldId; next.Seed = options.Seed; next.Policy = rules;
    // Fixed integer PRNG; no standard-library distribution implementation dependence.
    std::uint64_t random = options.Seed;
    const auto draw = [&random](std::uint32_t bound)
    {
        random = random * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>((random >> 32) % bound);
    };
    for (std::size_t d = 0; d < DeityCount; ++d)
        next.Cultures.emplace(d + 1, Culture{ d + 1, std::string(DeityName(static_cast<Deity>(d))) + " tradition", static_cast<Deity>(d) });
    const auto columns = options.Width / options.RegionWidth;
    const auto areaId = [&](std::uint32_t x, std::uint32_t y) -> Id { return static_cast<Id>(y) * options.Width + x + 1; };
    for (std::uint32_t ry = 0; ry < options.Height / options.RegionHeight; ++ry)
        for (std::uint32_t rx = 0; rx < columns; ++rx)
        {
            const Id id = static_cast<Id>(ry) * columns + rx + 1;
            const Id domain = rx * options.DomainCount / columns + 1;
            next.Regions.emplace(id, Region{ id, domain, id });
            next.Entities.emplace(id, PoliticalEntity{ id, "House " + std::to_string(id), id });
            if (!next.Domains.count(domain)) next.Domains.emplace(domain, PoliticalDomain{ domain, "Domain " + std::to_string(domain), id });
            Character character;
            character.Key = id; character.Name = "Steward " + std::to_string(id);
            character.AreaId = areaId(rx * options.RegionWidth, ry * options.RegionHeight);
            character.CultureId = id % DeityCount + 1;
            character.BornDay = -rules.AdultAgeDays - draw(7000) - 1;
            for (std::size_t d = 0; d < DeityCount; ++d)
                character.Affinities[d].EarnedFavor = FavorCost(d == character.CultureId - 1 ? 10 : 2, rules);
            next.Characters.emplace(id, character);
        }
    for (std::uint32_t y = 0; y < options.Height; ++y)
        for (std::uint32_t x = 0; x < options.Width; ++x)
        {
            Area area;
            area.Key = areaId(x, y); area.X = static_cast<std::int32_t>(x); area.Y = static_cast<std::int32_t>(y);
            area.RegionId = static_cast<Id>(y / options.RegionHeight) * columns + x / options.RegionWidth + 1;
            const Id culture = area.RegionId % DeityCount + 1;
            const std::int64_t population = 1000 + draw(9000);
            area.Cohorts = { { culture, population * 9 / 10, 0, 0 }, { culture % DeityCount + 1, population - population * 9 / 10, 0, 0 } };
            area.FoodCapacity = population + draw(static_cast<std::uint32_t>(population));
            if (x) area.Neighbors.push_back(areaId(x - 1, y));
            if (x + 1 < options.Width) area.Neighbors.push_back(areaId(x + 1, y));
            if (y) area.Neighbors.push_back(areaId(x, y - 1));
            if (y + 1 < options.Height) area.Neighbors.push_back(areaId(x, y + 1));
            next.Areas.emplace(area.Key, std::move(area));
        }
    result = Validate(next);
    if (result) output = std::move(next);
    return result;
}

Result World::Restore(const Snapshot& snapshot)
{
    auto result = Validate(snapshot);
    if (result) { StateData = snapshot; OperationIndex.clear(); IndexedEvents = 0; }
    return result;
}
Result World::Commit(Snapshot&& next)
{
    auto result = Batched ? Ok() : Validate(next);
    if (result) StateData = std::move(next);
    return result;
}
Result World::CheckOperation(const std::string& operation) const
{
    if (!Text(operation)) return Reject("operation_invalid", "Operation ID must contain 1..1024 bytes.");
    while (IndexedEvents < StateData.Events.size()) OperationIndex.insert(StateData.Events[IndexedEvents++].OperationId);
    if (OperationIndex.count(operation)) return Reject("operation_duplicate", operation);
    return Ok();
}
Snapshot World::BeginMutation()
{
    return Batched ? std::move(StateData) : StateData;
}
Metrics World::Inspect() const
{
    Metrics metrics;
    metrics.Day = StateData.Day; metrics.DayFraction = StateData.DayFraction; metrics.Areas = StateData.Areas.size();
    metrics.NamedCharacters = StateData.Characters.size(); metrics.Operations = StateData.Events.size();
    for (const auto& item : StateData.Areas)
    {
        if (item.second.IsWater) { ++metrics.WaterAreas; continue; }
        ++metrics.LandAreas;
        for (const auto& cohort : item.second.Cohorts) metrics.Population += cohort.Population;
        metrics.MeanUnrest += item.second.Unrest;
    }
    if (metrics.LandAreas) metrics.MeanUnrest /= static_cast<double>(metrics.LandAreas);
    return metrics;
}
std::vector<Id> World::ChildrenOf(Id parent) const
{
    std::vector<Id> children;
    for (const auto& item : StateData.Characters)
        if (std::find(item.second.Parents.begin(), item.second.Parents.end(), parent) != item.second.Parents.end()) children.push_back(item.first);
    return children;
}

Result World::Marry(const std::string& operation, Id first, Id second)
{
    auto result = CheckOperation(operation); if (!result) return result;
    const auto a = StateData.Characters.find(first), b = StateData.Characters.find(second);
    if (first == second || a == StateData.Characters.end() || b == StateData.Characters.end() ||
        !Adult(a->second, StateData) || !Adult(b->second, StateData) || a->second.SpouseId || b->second.SpouseId ||
        Ancestor(StateData, first, second) || Ancestor(StateData, second, first))
        return Reject("marriage_ineligible", "Requires distinct, living adult, unmarried characters without direct ancestry.");
    auto next = BeginMutation();
    next.Characters.at(first).SpouseId = second; next.Characters.at(second).SpouseId = first;
    next.Events.push_back({ operation, "marriage", next.Day, first, second, "Mutual marriage recorded" });
    return Commit(std::move(next));
}
Result World::Birth(const std::string& operation, Id child, const std::string& name,
    Id firstParent, Id secondParent, Deity primary, Deity secondary)
{
    auto result = CheckOperation(operation); if (!result) return result;
    const auto a = StateData.Characters.find(firstParent), b = StateData.Characters.find(secondParent);
    if (!child || StateData.Characters.count(child) || !Text(name) || !Defined(primary) || !Defined(secondary) || primary == secondary ||
        firstParent == secondParent || a == StateData.Characters.end() || b == StateData.Characters.end() ||
        !Adult(a->second, StateData) || !Adult(b->second, StateData) || a->second.SpouseId != secondParent || b->second.SpouseId != firstParent)
        return Reject("birth_ineligible", "Requires a new ID, two married living adult parents and two distinct deity priorities.");
    auto next = BeginMutation();
    Character character;
    character.Key = child; character.Name = name; character.AreaId = a->second.AreaId;
    character.CultureId = a->second.CultureId; character.BornDay = next.Day;
    character.Parents = { firstParent, secondParent };
    for (std::size_t d = 0; d < DeityCount; ++d)
    {
        const auto deity = static_cast<Deity>(d);
        const auto fraction = deity == primary ? next.Policy.PrimaryInheritance : deity == secondary ? next.Policy.SecondaryInheritance : next.Policy.OtherInheritance;
        const auto left = Power(a->second.Affinities[d], next.Policy), right = Power(b->second.Affinities[d], next.Policy);
        const auto reserved = [&](Id parent) { return std::any_of(next.Allocations.begin(), next.Allocations.end(), [&](const BirthrightAllocation& allocation)
            { return allocation.ParentId == parent && allocation.Path == deity; }); };
        Id source = left >= right ? firstParent : secondParent;
        if (reserved(source)) source = source == firstParent ? secondParent : firstParent;
        if (reserved(source)) continue; // Both parents already promised this deity to other children.
        const auto full = (source == firstParent ? left : right) * fraction;
        if (full == 0) continue;
        next.Allocations.push_back({ source, deity, FavorCost(full, next.Policy), false });
        character.Birthright[d] = { source, full, false };
        character.Affinities[d].BirthrightPower = full * next.Policy.ImmediateBirthrightFraction;
        character.Birthright[d].GrantedFavor = FavorCost(character.Affinities[d].BirthrightPower, next.Policy);
    }
    next.Characters.emplace(child, character);
    next.Events.push_back({ operation, "birth", next.Day, child, firstParent,
        "Second parent=" + std::to_string(secondParent) + "; primary=" + DeityName(primary) + "; secondary=" + DeityName(secondary) });
    return Commit(std::move(next));
}
Result World::AwardFavor(const std::string& operation, Id character, Deity deity, double favor, const std::string& reason)
{
    auto result = CheckOperation(operation); if (!result) return result;
    const auto found = StateData.Characters.find(character);
    if (found == StateData.Characters.end() || !found->second.Alive || !Defined(deity) ||
        !Bounded(favor, 0.001, FavorCost(MaximumPower, StateData.Policy)) || !Text(reason))
        return Reject("favor_ineligible", "Requires living character, defined deity, positive bounded favor and a reason.");
    auto next = BeginMutation();
    next.Characters.at(character).Affinities[static_cast<std::size_t>(deity)].EarnedFavor += favor;
    next.Events.push_back({ operation, "favor", next.Day, character, 0, std::string(DeityName(deity)) + ": " + reason });
    return Commit(std::move(next));
}
Result World::ResolveStrifeDuel(const std::string& operation, Id winner, Id loser)
{
    auto result = CheckOperation(operation); if (!result) return result;
    const auto a = StateData.Characters.find(winner), b = StateData.Characters.find(loser);
    if (winner == loser || a == StateData.Characters.end() || b == StateData.Characters.end() ||
        !Adult(a->second, StateData) || !Adult(b->second, StateData) || a->second.AreaId != b->second.AreaId)
        return Reject("duel_ineligible", "Requires two living adults in the same area; host must certify the sanctioned combat result.");
    auto next = BeginMutation();
    auto& victor = next.Characters.at(winner);
    auto& defeated = next.Characters.at(loser);
    auto& target = victor.Affinities[static_cast<std::size_t>(Deity::Strife)];
    auto& source = defeated.Affinities[static_cast<std::size_t>(Deity::Strife)];
    const auto current = Power(target, next.Policy);
    const auto gain = FavorCost(Power(source, next.Policy), next.Policy) * next.Policy.StrifeTransferFraction;
    if (FavorCost(current, next.Policy) + gain > FavorCost(MaximumPower, next.Policy))
        return Reject("power_limit", "Duel result would exceed the supported favor range.");
    target.TransferredFavor += gain;
    source = {};
    ApplyDeath(next, loser);
    next.Events.push_back({ operation, "strife_duel", next.Day, winner, loser, "Strife strength transferred; loser died; affected offices vacant" });
    return Commit(std::move(next));
}

Result World::RecordDeath(const std::string& operation, Id character, const std::string& cause)
{
    auto result = CheckOperation(operation); if (!result) return result;
    const auto found = StateData.Characters.find(character);
    if (found == StateData.Characters.end() || !found->second.Alive || !Text(cause))
        return Reject("death_ineligible", "Requires a living named character and a recorded cause.");
    auto next = BeginMutation();
    ApplyDeath(next, character);
    next.Events.push_back({ operation, "death", next.Day, character, 0, cause });
    return Commit(std::move(next));
}

Result World::ReleaseBirthright(const std::string& operation, Id parent, Id child)
{
    auto result = CheckOperation(operation); if (!result) return result;
    const auto donor = StateData.Characters.find(parent), recipient = StateData.Characters.find(child);
    if (donor == StateData.Characters.end() || recipient == StateData.Characters.end() ||
        !Adult(donor->second, StateData) || !recipient->second.Alive || donor->second.AreaId != recipient->second.AreaId)
        return Reject("ceremony_ineligible", "Living parent and child must be together; donor must be an adult.");
    auto next = BeginMutation();
    auto& source = next.Characters.at(parent);
    auto& target = next.Characters.at(child);
    bool released = false;
    for (std::size_t d = 0; d < DeityCount; ++d)
    {
        auto& grant = target.Birthright[d];
        if (grant.SourceParentId != parent || grant.Released) continue;
        auto& gift = target.Affinities[d]; auto& reserve = source.Affinities[d];
        const auto cost = FavorCost(grant.FullPower, next.Policy) - grant.GrantedFavor;
        const auto available = FavorCost(Power(reserve, next.Policy), next.Policy);
        if (available + 0.0000001 < cost) return Reject("ceremony_insufficient_power", "Parent lacks remaining reserve for deity " + std::string(DeityName(static_cast<Deity>(d))));
        // Spend transferred reserve, then earned favor, then inherited reserve.
        auto remaining = std::max(0.0, cost);
        const auto transferred = std::min(reserve.TransferredFavor, remaining);
        reserve.TransferredFavor -= transferred; remaining -= transferred;
        const auto earned = std::min(reserve.EarnedFavor, remaining);
        reserve.EarnedFavor -= earned; remaining -= earned;
        reserve.BirthrightPower = PowerFromFavor(std::max(0.0, FavorCost(reserve.BirthrightPower, next.Policy) - remaining), next.Policy);
        gift.BirthrightPower = PowerFromFavor(FavorCost(gift.BirthrightPower, next.Policy) + cost, next.Policy);
        grant.GrantedFavor = FavorCost(grant.FullPower, next.Policy);
        grant.Released = true; released = true;
    }
    if (!released) return Reject("ceremony_already_released", "No pending birthright from this parent to this child.");
    next.Events.push_back({ operation, "birthright_ceremony", next.Day, parent, child, "Pending birthright released; donor reserve consumed" });
    return Commit(std::move(next));
}

Result World::SplitBirthright(const std::string& operation, Id parent, Deity deity,
    const std::vector<BirthrightShare>& shares)
{
    auto result = CheckOperation(operation); if (!result) return result;
    const auto donor = StateData.Characters.find(parent);
    if (!Defined(deity) || donor == StateData.Characters.end() || !Adult(donor->second, StateData) || shares.size() < 2 || shares.size() > 1000)
        return Reject("split_ineligible", "Requires a living adult parent, defined deity and 2..1000 named children.");
    auto next = BeginMutation();
    auto allocation = std::find_if(next.Allocations.begin(), next.Allocations.end(), [&](const BirthrightAllocation& a) { return a.ParentId == parent && a.Path == deity; });
    if (allocation == next.Allocations.end()) return Reject("split_no_allocation", "Parent has no reserved birthright for this deity.");
    const auto d = static_cast<std::size_t>(deity);
    std::set<Id> included;
    double fractions = 0;
    for (const auto& share : shares)
    {
        const auto child = next.Characters.find(share.ChildId);
        if (!included.insert(share.ChildId).second || !Bounded(share.Fraction, 0.000001, 1) || child == next.Characters.end() ||
            !child->second.Alive || child->second.AreaId != donor->second.AreaId ||
            std::find(child->second.Parents.begin(), child->second.Parents.end(), parent) == child->second.Parents.end())
            return Reject("split_child_invalid", "Recipients must be distinct living children present at the ceremony.");
        auto& grant = child->second.Birthright[d];
        const auto power = PowerFromFavor(allocation->FavorBudget * share.Fraction, next.Policy);
        if ((grant.SourceParentId && grant.SourceParentId != parent) || grant.Released ||
            FavorCost(power, next.Policy) + 0.0000001 < grant.GrantedFavor)
            return Reject("split_vested", "Cannot overwrite another parent's grant or reclaim already active/released birthright.");
        grant = { parent, power, false, grant.GrantedFavor }; fractions += share.Fraction;
    }
    if (std::abs(fractions - 1) > 0.0000001) return Reject("split_fractions", "Shares must sum to 1; fractions divide favor-equivalent reserve.");
    for (const auto& item : next.Characters)
        if (item.second.Birthright[d].SourceParentId == parent && !included.count(item.first))
            return Reject("split_recipient_missing", "Include every existing recipient so no reservation is silently lost.");
    allocation->SplitAuthorized = true;
    next.Events.push_back({ operation, "birthright_split", next.Day, parent, 0, "Reserved favor budget split among " + std::to_string(shares.size()) + " children" });
    return Commit(std::move(next));
}

Result World::AdvanceDays(std::uint32_t days)
{
    if (!StateData.Bandits.Engagement.EncounterId.empty()) return Reject("bandit_engaged", "Finish the active camp encounter before advancing campaign time.");
    if (!days || days > 36500 || (StateData.Day > MaximumDay - days || (StateData.Day == MaximumDay - days && StateData.DayFraction > 0))) return Reject("time_invalid", "Advance 1..36500 days inside the supported clock range.");
    World candidate;
    candidate.StateData = StateData;
    candidate.Batched = true;
    auto& next = candidate.StateData;
    for (std::uint32_t day = 0; day < days; ++day)
    {
        ++next.Day;
        const auto shortages = next.Evolution.Policy.Enabled ? AdvanceEconomy(next) : std::vector<double>{};
        std::size_t landIndex = 0;
        for (auto& item : next.Areas)
        {
            auto& area = item.second;
            if (area.IsWater) continue;
            std::int64_t population = 0;
            for (const auto& cohort : area.Cohorts) population += cohort.Population;
            const double shortage = next.Evolution.Policy.Enabled ? shortages[landIndex++] :
                population ? std::max(0.0, 1.0 - static_cast<double>(area.FoodCapacity) / static_cast<double>(population)) : 0;
            area.Unrest = std::clamp(area.Unrest + (shortage > 0 ? shortage * 0.01 : -0.001), 0.0, 1.0);
            if (next.Evolution.Policy.Enabled)
            {
                const double target = std::clamp(0.2 + 0.5 * (1 - shortage) + 0.3 * area.Development - 0.2 * area.Unrest, 0.0, 1.0);
                area.Prosperity += (target - area.Prosperity) * 0.005;
            }
            else area.Prosperity = std::clamp(area.Prosperity + (shortage > 0 ? -shortage * 0.01 : 0.0005), 0.0, 1.0);
            const bool nutritionModel=next.Market.Policy.Enabled && next.Market.Policy.PersistentNutrition;
            const double nutrition=nutritionModel?next.Market.Areas.at(item.first).Nutrition:1-shortage;
            const double threshold=next.Market.Policy.StarvationThreshold;
            const double mortalityPressure=nutritionModel?std::pow(std::max(0.0,threshold-nutrition)/threshold,2):shortage;
            for (auto& cohort : area.Cohorts)
            {
                const double births = static_cast<double>(cohort.Population) * next.Policy.DailyBirthRate * nutrition + cohort.BirthRemainder;
                const double deaths = static_cast<double>(cohort.Population) * (next.Policy.DailyDeathRate + mortalityPressure * 0.001) + cohort.DeathRemainder;
                const auto born = static_cast<std::int64_t>(std::floor(births));
                const auto died = std::min(cohort.Population, static_cast<std::int64_t>(std::floor(deaths)));
                cohort.BirthRemainder = births - std::floor(births); cohort.DeathRemainder = deaths - std::floor(deaths);
                if(next.Market.Policy.Enabled) {
                    auto& diagnostic=next.Market.Areas.at(item.first);
                    diagnostic.DemographicBirths+=static_cast<std::uint64_t>(born);
                    diagnostic.DemographicDeaths+=static_cast<std::uint64_t>(died);
                    diagnostic.ExpectedNutritionDeaths+=static_cast<double>(cohort.Population)*mortalityPressure*0.001;
                }
                cohort.Population += born - died;
                if (cohort.Population > MaximumPopulation) return Reject("population_limit", "Daily simulation exceeded population range; advance rejected atomically.");
            }
        }
        AdvanceConnectivity(next);
        // Illustrative deterministic behavior: each adult steward practices its culture's patron path.
        if (!next.Evolution.Policy.Enabled) for (auto& item : next.Characters)
            if (Adult(item.second, next) && !next.Players.count(item.first))
            {
                const auto deity = next.Cultures.at(item.second.CultureId).Patron;
                auto& affinity = item.second.Affinities[static_cast<std::size_t>(deity)];
                const auto room = FavorCost(MaximumPower, next.Policy) - FavorCost(affinity.BirthrightPower, next.Policy) - affinity.EarnedFavor - affinity.TransferredFavor;
                affinity.EarnedFavor += std::max(0.0, std::min(room, next.Policy.DailyStewardFavor));
            }
        if (next.Evolution.Policy.Enabled)
        {
            const auto evolved = AdvanceEvolution(candidate);
            if (!evolved) return evolved;
        }
        const auto political = AdvancePolitics(candidate); if (!political) return political;
        const auto estates = AdvanceEstates(candidate); if (!estates) return estates;
        const auto bandits = AdvanceBandits(candidate); if (!bandits) return bandits;
        // Catch total growth limits per day, rather than after potentially unsafe fast-forward.
        std::int64_t total = 0;
        for (const auto& item : next.Areas)
            for (const auto& cohort : item.second.Cohorts)
            {
                total += cohort.Population;
                if (total > MaximumPopulation) return Reject("population_limit", "World growth exceeded 1e12; advance rejected atomically.");
            }
    }
    return Commit(std::move(next));
}

Result World::Travel(const std::string& operation, Id character, Id destinationArea)
{
    if (!StateData.Bandits.Engagement.EncounterId.empty()) return Reject("bandit_engaged", "Finish the active camp encounter before travel.");
    auto result = CheckOperation(operation); if (!result) return result;
    const auto found = StateData.Characters.find(character);
    if (found == StateData.Characters.end() || !found->second.Alive || !StateData.Areas.count(destinationArea))
        return Reject("travel_ineligible", "Requires a living character and an existing destination area.");
    const auto sourceArea = found->second.AreaId;
    const auto& neighbors = StateData.Areas.at(sourceArea).Neighbors;
    if (std::find(neighbors.begin(), neighbors.end(), destinationArea) == neighbors.end())
        return Reject("travel_not_adjacent", "Travel must cross one authoritative area link.");
    const double days = TravelDurationDays(StateData,sourceArea,destinationArea);
    if (!std::isfinite(days) || days <= 0 || days > 36500 ||
        days > static_cast<double>(MaximumDay-StateData.Day)-StateData.DayFraction)
        return Reject("travel_time_invalid", "Walking effort must fit a crossing of at most 36500 days inside the supported clock.");
    double exact = StateData.DayFraction + days;
    const double nearest = std::round(exact);
    if (std::abs(exact-nearest) <= 1e-10) exact = nearest; // Avoid losing a daily tick to floating-point addition.
    const auto whole = static_cast<std::uint32_t>(std::floor(exact));
    // Movement, fractional time, daily updates and receipt commit together.
    World candidate = *this;
    candidate.StateData.DayFraction = 0;
    if (whole) { result = candidate.AdvanceDays(whole); if (!result) return result; }
    candidate.StateData.DayFraction = exact-static_cast<double>(whole);
    candidate.StateData.Characters.at(character).AreaId = destinationArea;
    std::ostringstream detail; detail.precision(17);
    detail << "source=" << sourceArea << "; destination=" << destinationArea << "; days=" << days
        << "; effort=" << TraversalCost(StateData,sourceArea,destinationArea) << "; day_fraction=" << candidate.StateData.DayFraction;
    candidate.StateData.Events.push_back({ operation, "travel", candidate.StateData.Day, character, 0,detail.str() });
    return Commit(std::move(candidate.StateData));
}
}
