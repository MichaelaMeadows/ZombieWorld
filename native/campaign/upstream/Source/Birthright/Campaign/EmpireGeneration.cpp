#include "EmpireGeneration.h"
#include "WorldTraversal.h"
#include "MapGeography.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numeric>
#include <ostream>
#include <queue>
#include <set>
#include <sstream>
#include <tuple>

namespace Birthright::Campaign
{
namespace
{
using Geography = std::map<Id, GeographicArea>;
using Distances = std::map<Id, double>;
constexpr double Infinity = std::numeric_limits<double>::infinity();
Result Ok() { return { true, "accepted", "" }; }
Result Reject(const std::string& code, const std::string& detail) { return { false, code, detail }; }
bool Bounded(double n, double low, double high) { return std::isfinite(n) && n >= low && n <= high; }
bool Defined(Deity d) { return static_cast<std::size_t>(d) < DeityCount; }
struct Random
{
    std::uint64_t State;
    std::uint32_t Draw(std::uint32_t bound)
    {
        State = State * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<std::uint32_t>((State >> 32) % bound);
    }
};
bool SocietyValid(const Society& society)
{
    double total = 0;
    for (const auto share : society.Roles) { if (!Bounded(share, 0, 1)) return false; total += share; }
    return std::abs(total - 1) <= 0.000001;
}
double At(const Distances& distance, Id area)
{
    const auto found = distance.find(area);
    return found == distance.end() ? Infinity : found->second;
}
// Stable largest-remainder apportionment conserves people even for tiny groups.
std::vector<std::int64_t> Apportion(std::int64_t total, const std::vector<double>& weights)
{
    std::vector<std::int64_t> counts(weights.size());
    const double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    if (total == 0 || sum == 0) return counts;
    std::vector<std::pair<double, std::size_t>> remainder;
    std::int64_t assigned = 0;
    for (std::size_t i = 0; i < weights.size(); ++i)
    {
        const double exact = static_cast<double>(total) * weights[i] / sum;
        counts[i] = static_cast<std::int64_t>(std::floor(exact));
        assigned += counts[i];
        remainder.emplace_back(exact - std::floor(exact), i);
    }
    std::sort(remainder.begin(), remainder.end(), [](const auto& a, const auto& b) {
        return a.first == b.first ? a.second < b.second : a.first > b.first;
    });
    for (std::int64_t i = assigned; i < total; ++i)
        ++counts[remainder[static_cast<std::size_t>(i - assigned) % remainder.size()].second];
    return counts;
}
std::string Xml(const std::string& value)
{
    std::string output;
    for (const auto c : value)
    {
        if (c == '&') output += "&amp;";
        else if (c == '<') output += "&lt;";
        else if (c == '>') output += "&gt;";
        else if (c == '"') output += "&quot;";
        else output += c;
    }
    return output;
}
}

Result MakeGridGeography(std::uint32_t width, std::uint32_t height, std::uint32_t seed,
    std::vector<GeographicArea>& output)
{
    if (!width || !height || static_cast<std::uint64_t>(width) * height > 10000)
        return Reject("geography_size", "Use a nonempty grid with at most 10000 areas.");
    std::vector<GeographicArea> next;
    Random random{ seed };
    const auto key = [width](std::uint32_t x, std::uint32_t y) -> Id { return static_cast<Id>(y) * width + x + 1; };
    for (std::uint32_t y = 0; y < height; ++y)
        for (std::uint32_t x = 0; x < width; ++x)
        {
            GeographicArea area;
            area.Key = key(x, y); area.X = static_cast<std::int32_t>(x); area.Y = static_cast<std::int32_t>(y);
            // Placeholder ecology only; imported geometry supplies authored values instead.
            area.SettlementSuitability = 0.3 + static_cast<double>(random.Draw(701)) / 1000;
            area.PopulationCapacity = 6000 + random.Draw(6001);
            if (x) area.Neighbors.push_back(key(x - 1, y));
            if (x + 1 < width) area.Neighbors.push_back(key(x + 1, y));
            if (y) area.Neighbors.push_back(key(x, y - 1));
            if (y + 1 < height) area.Neighbors.push_back(key(x, y + 1));
            next.push_back(std::move(area));
        }
    output = std::move(next); return Ok();
}

Result GenerateEmpires(const std::vector<GeographicArea>& geography, const EmpireGeneration& options,
    const Rules& rules, Snapshot& output)
{
    auto result = ValidateRules(rules); if (!result) return result;
    result = ValidateGeometry(options.Geometry); if (!result) return result;
    if (options.AllowDisconnected && options.Geometry.Cells.empty())
        return Reject("geography_missing", "Disconnected imports require explicit polygon geometry.");
    if (options.Version != 1 || options.WorldId.empty() || options.WorldId.size() > 1024 ||
        !options.EmpireCount || options.EmpireCount > 64 || !options.RegionsPerEmpire || options.RegionsPerEmpire > 256 ||
        geography.empty() || geography.size() > 10000 ||
        static_cast<std::uint64_t>(options.EmpireCount) * geography.size() > 1000000 ||
        static_cast<std::uint64_t>(options.EmpireCount) * options.RegionsPerEmpire > geography.size() ||
        (!options.Seeds.empty() && options.Seeds.size() != options.EmpireCount) ||
        !Bounded(options.ClaimRadius, 0.01, 1000000) || !Bounded(options.ContestMargin, 0, 1000000) ||
        !Bounded(options.InfluenceDailyResponse, 0.001, 1) || !Bounded(options.MigrationDailyFraction, 0, 0.02) ||
        options.MigrationDailyFraction >= options.InfluenceDailyResponse || options.Connections.size() > 1000000 ||
        !Bounded(options.InfluenceRadius, 0.01, 1000000) || !Bounded(options.WildernessThreshold, 0, 1) ||
        options.WildernessPopulation < 0 || options.WildernessPopulation > 1000 ||
        !Bounded(options.SettlementFraction, 0.001, 1) || !Bounded(options.LocalCultureWeight, 0.000001, 1))
        return Reject("empire_generation_invalid", "Invalid N/K, budgets, profile count, version, radii or population policy.");
    for (const auto& profile : options.Profiles)
        if (!SocietyValid(profile)) return Reject("society_invalid", "Each deity role profile must contain finite nonnegative shares summing to 1.");
    Geography graph;
    std::set<std::pair<std::int32_t, std::int32_t>> coordinates;
    for (const auto& area : geography)
    {
        if (!area.Key || (options.Geometry.Cells.empty() && !coordinates.emplace(area.X, area.Y).second) || !graph.emplace(area.Key, area).second ||
            !Bounded(area.Development, 0, 1) ||
            !Bounded(area.SettlementSuitability, 0, 1) || !Bounded(area.DiffusionCost, 0.01, 1000) ||
            area.PopulationCapacity < 1 || area.PopulationCapacity > 1000000000)
            return Reject("geography_invalid", "Duplicate IDs/coordinates, invalid suitability, cost or population capacity.");
    }
    for (const auto& item : graph)
    {
        std::set<Id> seen;
        for (const auto neighbor : item.second.Neighbors)
        {
            const auto found = graph.find(neighbor);
            if (neighbor == item.first || !seen.insert(neighbor).second || found == graph.end() ||
                std::find(found->second.Neighbors.begin(), found->second.Neighbors.end(), item.first) == found->second.Neighbors.end())
                return Reject("geography_adjacency", "Links must be unique, symmetric and reference existing areas.");
        }
    }
    std::map<std::pair<Id, Id>, TraversalConnection> connections;
    for (const auto& link : options.Connections)
    {
        if (!link.FirstAreaId || link.FirstAreaId >= link.SecondAreaId || !graph.count(link.FirstAreaId) || !graph.count(link.SecondAreaId) ||
            !Bounded(link.RoadQuality, 0, 1) || !Bounded(link.DistanceScale, 0.01, 1000) ||
            !connections.emplace(std::make_pair(link.FirstAreaId, link.SecondAreaId), link).second)
            return Reject("traversal_connection", "Canonical unique adjacent endpoints, road 0..1 and distance scale 0.01..1000 required.");
        const auto& neighbors = graph.at(link.FirstAreaId).Neighbors;
        if (std::find(neighbors.begin(), neighbors.end(), link.SecondAreaId) == neighbors.end())
            return Reject("traversal_connection", "Roads must follow geographic adjacency.");
    }
    const auto DistanceFrom = [&](const Geography& input, Id source, const std::set<Id>& allowed = std::set<Id>{}) {
        return TraversalDistances(input, connections, source, allowed);
    };
    if (!options.AllowDisconnected && DistanceFrom(graph, graph.begin()->first).size() != graph.size())
        return Reject("geography_disconnected", "Connect the navigable graph explicitly, including any sea links.");
    std::vector<EmpireSeed> seeds = options.Seeds;
    std::map<Id, std::size_t> componentSizes;
    for (const auto& item : graph)
    {
        if (componentSizes.count(item.first)) continue;
        const auto component = DistanceFrom(graph,item.first);
        for (const auto& member : component) componentSizes[member.first] = component.size();
    }
    if (seeds.empty()) seeds.resize(options.EmpireCount);
    Random random{ options.Seed };
    std::set<Id> capitals;
    std::map<Id, Distances> capitalDistances;
    std::vector<Distances> distances;
    // Reserve explicit capitals first; auto-placement cannot steal a later explicit choice.
    for (const auto& seed : seeds)
        if (seed.CapitalAreaId && (!graph.count(seed.CapitalAreaId) ||
            graph.at(seed.CapitalAreaId).SettlementSuitability < 0.3 || !capitals.insert(seed.CapitalAreaId).second))
            return Reject("capital_seed_invalid", "Capitals must be distinct existing areas with suitability at least 0.3.");
    for (const auto capital : capitals) capitalDistances.emplace(capital, DistanceFrom(graph, capital));
    for (std::size_t index = 0; index < seeds.size(); ++index)
    {
        auto& seed = seeds[index];
        if (!Bounded(seed.InfluenceReach, 0.1, 4) || !Bounded(seed.PrimaryWeight, 0.001, 1))
            return Reject("empire_profile_invalid", "Influence reach must be within 0.1..4.");
        if (seed.PrimaryPatron == Deity::Count)
        {
            if (seed.SecondaryPatron != Deity::Count) return Reject("empire_profile_invalid", "Specify a primary patron before a secondary.");
            seed.PrimaryPatron = static_cast<Deity>(random.Draw(static_cast<std::uint32_t>(DeityCount)));
            if (random.Draw(3))
                seed.SecondaryPatron = static_cast<Deity>((static_cast<std::size_t>(seed.PrimaryPatron) + 1 + random.Draw(6)) % DeityCount);
        }
        if (!Defined(seed.PrimaryPatron) || (seed.SecondaryPatron != Deity::Count &&
            (!Defined(seed.SecondaryPatron) || seed.SecondaryPatron == seed.PrimaryPatron || !Bounded(seed.PrimaryWeight, 0.001, 0.999))))
            return Reject("empire_profile_invalid", "Use one patron or two distinct patrons with a weight strictly between 0 and 1.");
        if (seed.SecondaryPatron == Deity::Count) seed.PrimaryWeight = 1;
        if (!seed.CapitalAreaId)
        {
            std::vector<Id> candidates;
            double farthest = -1;
            for (const auto& item : graph)
            {
                if (capitals.count(item.first) || item.second.SettlementSuitability < 0.3 ||
                    componentSizes.at(item.first) < options.RegionsPerEmpire) continue;
                double nearest = Infinity;
                // Explicit future capitals also affect spacing.
                for (const auto capital : capitals)
                    nearest = std::min(nearest, At(capitalDistances.at(capital), item.first));
                if (nearest > farthest) { farthest = nearest; candidates.clear(); }
                if (nearest == farthest) candidates.push_back(item.first);
            }
            if (candidates.empty()) return Reject("capital_seed_invalid", "Not enough suitable areas for N capitals.");
            seed.CapitalAreaId = candidates[random.Draw(static_cast<std::uint32_t>(candidates.size()))];
            capitals.insert(seed.CapitalAreaId);
            capitalDistances.emplace(seed.CapitalAreaId, DistanceFrom(graph, seed.CapitalAreaId));
        }
        distances.push_back(capitalDistances.at(seed.CapitalAreaId));
    }
    Snapshot next;
    next.WorldId = options.WorldId; next.Seed = options.Seed; next.Policy = rules;
    next.Geometry = options.Geometry;
    next.Connections = connections;
    next.Connectivity = {1, options.InfluenceDailyResponse, options.MigrationDailyFraction, options.SettlementFraction, options.LocalCultureWeight};
    const Id frontierCulture = options.EmpireCount + 1;
    next.Cultures.emplace(frontierCulture, Culture{ frontierCulture, "Frontier communities", Deity::Harmony });
    for (std::size_t d = 0; d < seeds.size(); ++d)
    {
        const auto& seed = seeds[d]; const Id domain = d + 1;
        Culture culture{ domain, "Empire " + std::to_string(domain) + " tradition", seed.PrimaryPatron };
        culture.SecondaryPatron = seed.SecondaryPatron; culture.PrimaryPatronWeight = seed.PrimaryWeight; culture.OriginDomainId = domain;
        const auto& primary = options.Profiles[static_cast<std::size_t>(seed.PrimaryPatron)];
        for (std::size_t role = 0; role < PopulationRoleCount; ++role)
            culture.Occupations.Roles[role] = primary.Roles[role] * seed.PrimaryWeight +
                (seed.SecondaryPatron == Deity::Count ? 0 : options.Profiles[static_cast<std::size_t>(seed.SecondaryPatron)].Roles[role] * (1 - seed.PrimaryWeight));
        next.Cultures.emplace(domain, culture);
        PoliticalDomain empire{ domain, "Empire " + std::to_string(domain), 0 };
        empire.InfluenceBudget = options.InfluenceRadius * seed.InfluenceReach;
        empire.CapitalAreaId = seed.CapitalAreaId; empire.PrimaryPatron = seed.PrimaryPatron;
        empire.SecondaryPatron = seed.SecondaryPatron; empire.PrimaryPatronWeight = seed.PrimaryWeight;
        next.Domains.emplace(domain, empire);
    }
    std::map<Id, std::set<Id>> holdings;
    for (const auto& item : graph)
    {
        const auto& terrain = item.second;
        Area area;
        area.Key = terrain.Key; area.X = terrain.X; area.Y = terrain.Y; area.Neighbors = terrain.Neighbors;
        area.Development = terrain.Development; area.PopulationCapacity = terrain.PopulationCapacity;
        std::sort(area.Neighbors.begin(), area.Neighbors.end());
        area.SettlementSuitability = terrain.SettlementSuitability; area.DiffusionCost = terrain.DiffusionCost;
        std::vector<std::pair<double, Id>> claims;
        std::vector<double> weights(seeds.size() + 1, 0);
        weights.back() = options.LocalCultureWeight;
        double strongest = 0;
        for (std::size_t d = 0; d < seeds.size(); ++d)
        {
            const double distance = At(distances[d], area.Key);
            const double radial = std::max(0.0, 1 - distance / (options.InfluenceRadius * seeds[d].InfluenceReach));
            const double influence = radial * radial;
            if (influence >= 0.000000001) area.Influences.push_back({ d + 1, influence });
            weights[d] = influence; strongest = std::max(strongest, influence);
            if (distance <= options.ClaimRadius) claims.emplace_back(distance, d + 1);
        }
        std::sort(claims.begin(), claims.end());
        // Capitals stay recognized; contesting a seat of government belongs to later conquest rules.
        const auto capital = std::find_if(seeds.begin(), seeds.end(), [&](const EmpireSeed& seed) { return seed.CapitalAreaId == area.Key; });
        if (capital != seeds.end()) holdings[static_cast<Id>(capital - seeds.begin()) + 1].insert(area.Key);
        else if (!claims.empty())
        {
            for (const auto& claim : claims)
                if (claim.first - claims.front().first <= options.ContestMargin) area.Claimants.push_back(claim.second);
            std::sort(area.Claimants.begin(), area.Claimants.end());
            if (area.Claimants.size() == 1) { holdings[area.Claimants.front()].insert(area.Key); area.Claimants.clear(); }
            // Multiple claimants deliberately leave RegionId=0: there is no recognized owner.
        }
        area.Wilderness = strongest <= options.WildernessThreshold || terrain.SettlementSuitability <= 0.05;
        const std::int64_t population = area.Wilderness ? options.WildernessPopulation :
            options.WildernessPopulation + static_cast<std::int64_t>(std::floor(static_cast<double>(terrain.PopulationCapacity) *
                terrain.SettlementSuitability * options.SettlementFraction * strongest));
        const auto counts = Apportion(population, weights);
        double farmingShare = 0;
        for (std::size_t d = 0; d < counts.size(); ++d)
        {
            const Id culture = d == seeds.size() ? frontierCulture : d + 1;
            if (counts[d]) area.Cohorts.push_back({ culture, counts[d], 0, 0 });
            farmingShare += static_cast<double>(counts[d]) * next.Cultures.at(culture).Occupations.Roles[1];
        }
        if (area.Cohorts.empty()) area.Cohorts.push_back({ frontierCulture, 0, 0, 0 });
        farmingShare = population ? farmingShare / static_cast<double>(population) : 0.6;
        area.FoodCapacity = std::max<std::int64_t>(1, static_cast<std::int64_t>(std::floor(
            static_cast<double>(terrain.PopulationCapacity) * terrain.SettlementSuitability * (0.4 + farmingShare * 0.8))));
        area.Prosperity = 0.2 + 0.6 * strongest;
        area.Unrest = area.Claimants.empty() ? 0.05 : 0.25;
        area.TargetInfluences = area.Influences;
        next.Areas.emplace(area.Key, std::move(area));
    }
    Id nextCharacter = 1;
    for (std::size_t d = 0; d < seeds.size(); ++d)
    {
        const Id domain = d + 1;
        const auto& owned = holdings[domain];
        if (owned.size() < options.RegionsPerEmpire)
            return Reject("empire_region_capacity", "Empire " + std::to_string(domain) + " has " + std::to_string(owned.size()) +
                " recognized areas for K=" + std::to_string(options.RegionsPerEmpire) + "; increase claim radius, reduce contest margin or K, or move capitals.");
        std::vector<Id> regionSeeds{ seeds[d].CapitalAreaId };
        std::vector<Distances> regionalDistances{ DistanceFrom(graph, regionSeeds.front(), owned) };
        while (regionSeeds.size() < options.RegionsPerEmpire)
        {
            Id choice = 0; double farthest = -1;
            for (const auto id : owned)
            {
                if (std::find(regionSeeds.begin(), regionSeeds.end(), id) != regionSeeds.end()) continue;
                double nearest = Infinity;
                for (const auto& distance : regionalDistances) nearest = std::min(nearest, At(distance, id));
                if (nearest > farthest) { farthest = nearest; choice = id; }
            }
            if (!choice) return Reject("empire_region_capacity", "Could not select K distinct region seeds.");
            regionSeeds.push_back(choice); regionalDistances.push_back(DistanceFrom(graph, choice, owned));
        }
        // Multi-source growth assigns connected territories. Stable (distance, region seed, area) ties.
        using Entry = std::tuple<double, std::size_t, Id>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
        std::map<Id, std::pair<double, std::size_t>> labels;
        for (std::size_t r = 0; r < regionSeeds.size(); ++r) { labels[regionSeeds[r]] = {0, r}; queue.emplace(0, r, regionSeeds[r]); }
        while (!queue.empty())
        {
            const auto [distance, region, id] = queue.top(); queue.pop();
            if (labels.at(id) != std::make_pair(distance, region)) continue;
            for (const auto neighbor : graph.at(id).Neighbors)
            {
                if (!owned.count(neighbor)) continue;
                const auto link = connections.find({std::min(id, neighbor), std::max(id, neighbor)});
                const auto& first = graph.at(id); const auto& second = graph.at(neighbor);
                const auto candidate = std::make_pair(distance + EffectiveConnectionDifficulty(first.DiffusionCost, first.Development,
                    second.DiffusionCost, second.Development, link == connections.end() ? nullptr : &link->second), region);
                const auto found = labels.find(neighbor);
                if (found == labels.end() || candidate < found->second)
                {
                    labels[neighbor] = candidate; queue.emplace(candidate.first, candidate.second, neighbor);
                }
            }
        }
        if (labels.size() != owned.size())
            return Reject("empire_region_components", "Empire " + std::to_string(domain) + " has more disconnected holdings than K; adjust seed/border settings.");
        for (const auto& label : labels)
            next.Areas.at(label.first).RegionId = static_cast<Id>(d) * options.RegionsPerEmpire + label.second.second + 1;
        const auto makeHead = [&](Id area, const std::string& name) {
            Character character;
            character.Key = nextCharacter++; character.Name = name; character.AreaId = area; character.CultureId = domain;
            character.BornDay = -rules.AdultAgeDays - 1 - random.Draw(10000);
            character.Affinities[static_cast<std::size_t>(seeds[d].PrimaryPatron)].EarnedFavor = FavorCost(20, rules);
            if (seeds[d].SecondaryPatron != Deity::Count)
                character.Affinities[static_cast<std::size_t>(seeds[d].SecondaryPatron)].EarnedFavor = FavorCost(10, rules);
            next.Characters.emplace(character.Key, character); return character.Key;
        };
        next.Domains.at(domain).HeadCharacterId = makeHead(seeds[d].CapitalAreaId, "Emperor " + std::to_string(domain));
        for (std::size_t r = 0; r < regionSeeds.size(); ++r)
        {
            const Id region = static_cast<Id>(d) * options.RegionsPerEmpire + r + 1;
            const Id head = makeHead(regionSeeds[r], "Steward " + std::to_string(region));
            next.Entities.emplace(region, PoliticalEntity{ region, "House " + std::to_string(region), head });
            next.Regions.emplace(region, Region{ region, domain, region });
        }
    }
    MaterializeWaterAreas(next);
    result = Validate(next);
    if (result) output = std::move(next);
    return result;
}

std::array<std::int64_t, PopulationRoleCount> EstimatePopulationRoles(const Snapshot& state, Id area)
{
    const auto& record = state.Areas.at(area);
    std::array<std::int64_t, PopulationRoleCount> roles{};
    // Per-cohort apportionment preserves totals; infants/age-specific labor are future policy.
    for (const auto& cohort : record.Cohorts)
    {
        const auto& weights = state.Cultures.at(cohort.CultureId).Occupations.Roles;
        const auto counts = Apportion(cohort.Population, std::vector<double>(weights.begin(), weights.end()));
        for (std::size_t r = 0; r < PopulationRoleCount; ++r) roles[r] += counts[r];
    }
    return roles;
}
WorldHierarchy InspectHierarchy(const Snapshot& state)
{
    WorldHierarchy hierarchy;
    for (const auto& item : state.Domains) hierarchy.Domains.emplace(item.first,TerritorySummary{});
    for (const auto& item : state.Regions)
    {
        hierarchy.Regions.emplace(item.first,TerritorySummary{});
        hierarchy.Domains.at(item.second.DomainId).RegionIds.push_back(item.first);
    }
    for (const auto& item : state.Areas)
    {
        const auto& area = item.second;
        if (area.IsWater || !area.RegionId) continue;
        auto& region = hierarchy.Regions.at(area.RegionId);
        auto& domain = hierarchy.Domains.at(state.Regions.at(area.RegionId).DomainId);
        region.AreaIds.push_back(item.first); domain.AreaIds.push_back(item.first);
        std::int64_t population = 0;
        for (const auto& cohort : area.Cohorts) population += cohort.Population;
        region.Population += population; domain.Population += population;
        const auto roles = EstimatePopulationRoles(state,item.first);
        for (std::size_t n = 0; n < PopulationRoleCount; ++n) { region.Roles[n] += roles[n]; domain.Roles[n] += roles[n]; }
    }
    return hierarchy;
}
EmpireMetrics InspectEmpires(const Snapshot& state)
{
    EmpireMetrics metrics;
    for (const auto& item : state.Areas)
    {
        const auto& area = item.second;
        if (area.IsWater) continue;
        if (area.RegionId) ++metrics.ClaimedAreas;
        else if (area.Claimants.size() >= 2) ++metrics.ContestedAreas;
        else ++metrics.UnclaimedAreas;
        if (area.Wilderness) ++metrics.WildernessAreas;
        for (const auto& cohort : area.Cohorts) metrics.Population += cohort.Population;
        const auto roles = EstimatePopulationRoles(state, item.first);
        for (std::size_t r = 0; r < PopulationRoleCount; ++r) metrics.Roles[r] += roles[r];
    }
    return metrics;
}
Result ExportEmpireSvg(const Snapshot& state, std::ostream& output)
{
    if (!state.Geometry.Cells.empty()) return ExportGeographySvg(state, output);
    const auto result = Validate(state); if (!result) return result;
    static constexpr const char* colors[] = { "#518ba5", "#76a967", "#b6935a", "#a47eb8", "#7794cd", "#b87582" };
    std::int32_t minX = state.Areas.begin()->second.X, minY = state.Areas.begin()->second.Y, maxX = minX, maxY = minY;
    for (const auto& item : state.Areas)
    {
        minX = std::min(minX, item.second.X); minY = std::min(minY, item.second.Y);
        maxX = std::max(maxX, item.second.X); maxY = std::max(maxY, item.second.Y);
    }
    const double width = (static_cast<double>(maxX) - minX + 1) * 24 + 48;
    const double height = (static_cast<double>(maxY) - minY + 1) * 24 + 120;
    const auto metrics = InspectEmpires(state);
    output << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << width << ' ' << height << "\">\n"
        << "<rect width=\"100%\" height=\"100%\" fill=\"#101923\"/><g font-family=\"sans-serif\" fill=\"#e8ebed\">"
        << "<text x=\"24\" y=\"26\" font-size=\"18\">" << Xml(state.WorldId) << " / seeded empires</text>"
        << "<text x=\"24\" y=\"48\" font-size=\"12\">Claimed " << metrics.ClaimedAreas << " | Contested " << metrics.ContestedAreas
        << " | Unclaimed " << metrics.UnclaimedAreas << " | Wilderness " << metrics.WildernessAreas << " | Population " << metrics.Population << "</text>";
    for (const auto& item : state.Areas)
    {
        const auto& a = item.second;
        const double x = (static_cast<double>(a.X) - minX) * 24 + 24, y = (static_cast<double>(a.Y) - minY) * 24 + 68;
        const Id domain = a.RegionId ? state.Regions.at(a.RegionId).DomainId : 0;
        const char* color = domain ? colors[(domain - 1) % 6] : (a.Wilderness ? "#23392a" : "#626969");
        std::int64_t population = 0;
        for (const auto& cohort : a.Cohorts) population += cohort.Population;
        const double opacity = a.Wilderness ? 0.7 : std::min(1.0, 0.3 + static_cast<double>(population) / 5000);
        output << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"23\" height=\"23\" fill=\"" << color << "\" fill-opacity=\"" << opacity
            << "\" stroke=\"" << (a.Claimants.size() >= 2 ? "#ffbb58" : "#101923") << "\" stroke-width=\"" << (a.Claimants.size() >= 2 ? 2 : 0.5) << "\">"
            << "<title>Area " << a.Key << "; region " << a.RegionId << "; empire " << domain << "; population " << population
            << "; terrain effort " << a.DiffusionCost << "; effective effort " << EffectiveAreaDifficulty(a.DiffusionCost, a.Development)
            << "; development " << a.Development
            << "; " << (a.Wilderness ? "wilderness" : "settled/frontier") << "; claimants";
        for (const auto claimant : a.Claimants) output << ' ' << claimant;
        output << "; cultural influence";
        for (const auto& influence : a.Influences) output << ' ' << influence.DomainId << ':' << influence.Strength;
        output << "; target influence";
        for (const auto& influence : a.TargetInfluences) output << ' ' << influence.DomainId << ':' << influence.Strength;
        const auto roles = EstimatePopulationRoles(state, a.Key);
        output << "; fighters " << roles[0] << "; farmers " << roles[1] << "; industry " << roles[2] << "</title></rect>\n";
        for (const auto neighbor : a.Neighbors)
        {
            const auto& other = state.Areas.at(neighbor);
            if (a.RegionId == other.RegionId) continue;
            if (other.X == a.X + 1 && other.Y == a.Y)
                output << "<path d=\"M" << x + 24 << ',' << y << "v24\" stroke=\"#d3d8d8\" stroke-width=\"1\"/>";
            if (other.Y == a.Y + 1 && other.X == a.X)
                output << "<path d=\"M" << x << ',' << y + 24 << "h24\" stroke=\"#d3d8d8\" stroke-width=\"1\"/>";
        }
    }
    for (const auto& item : state.Connections)
        if (item.second.RoadQuality > 0)
        {
            const auto& a = state.Areas.at(item.first.first); const auto& b = state.Areas.at(item.first.second);
            output << "<path d=\"M" << (static_cast<double>(a.X) - minX) * 24 + 36 << ',' << (static_cast<double>(a.Y) - minY) * 24 + 80
                << "L" << (static_cast<double>(b.X) - minX) * 24 + 36 << ',' << (static_cast<double>(b.Y) - minY) * 24 + 80
                << "\" stroke=\"#e7d4a2\" stroke-width=\"2\"><title>Road " << item.first.first << ':' << item.first.second
                << "; quality " << item.second.RoadQuality << "; effort " << TraversalCost(state, item.first.first, item.first.second) << "</title></path>";
        }
    for (const auto& item : state.Domains)
        if (item.second.CapitalAreaId)
        {
            const auto& a = state.Areas.at(item.second.CapitalAreaId);
            output << "<circle cx=\"" << (static_cast<double>(a.X) - minX) * 24 + 36 << "\" cy=\"" << (static_cast<double>(a.Y) - minY) * 24 + 80
                << "\" r=\"5\" fill=\"#fff2ba\"><title>Capital of empire " << item.first << "</title></circle>";
        }
    output << "<text x=\"24\" y=\"" << height - 18 << "\" font-size=\"12\">Empire colors | Gray: unclaimed | Gold outline: contested, no owner | Dark green: wilderness | Hover for data</text></g></svg>\n";
    return output ? Ok() : Reject("svg_write_failed", "Output stream rejected the overview.");
}
}
