#include "WorldTraversal.h"

#include <cmath>
#include <numeric>
#include <sstream>

namespace Birthright::Campaign
{
namespace
{
Result Ok() { return {true, "accepted", ""}; }
double Strength(const std::vector<Area::Influence>& values, Id domain)
{
    for (const auto& value : values) if (value.DomainId == domain) return value.Strength;
    return 0;
}
double DesiredPopulation(const Snapshot& state, const Area& area, Id culture)
{
    const auto origin = state.Cultures.at(culture).OriginDomainId;
    double total = state.Connectivity.LocalCultureWeight, strongest = 0;
    for (const auto& influence : area.Influences)
    {
        total += influence.Strength;
        strongest = std::max(strongest, influence.Strength);
    }
    const double weight = origin ? Strength(area.Influences, origin) : state.Connectivity.LocalCultureWeight;
    // This is an attraction estimate, never a source of new people or a change of cultural identity.
    return static_cast<double>(area.PopulationCapacity) * area.SettlementSuitability *
        state.Connectivity.SettlementFraction * strongest * weight / total;
}
std::int64_t Population(const Area& area, Id culture)
{
    for (const auto& cohort : area.Cohorts) if (cohort.CultureId == culture) return cohort.Population;
    return 0;
}
}
double EffectiveAreaDifficulty(double terrainCost, double development)
{
    return terrainCost * (1 - 0.75 * development);
}
double EffectiveConnectionDifficulty(double firstCost, double firstDevelopment,
    double secondCost, double secondDevelopment, const TraversalConnection* connection)
{
    const double terrain = (EffectiveAreaDifficulty(firstCost, firstDevelopment) +
        EffectiveAreaDifficulty(secondCost, secondDevelopment)) * 0.5;
    return terrain * (connection ? connection->DistanceScale * (1 - 0.75 * connection->RoadQuality) : 1);
}
double TraversalCost(const Snapshot& state, Id first, Id second)
{
    const auto a = state.Areas.find(first), b = state.Areas.find(second);
    if (a == state.Areas.end() || b == state.Areas.end() || a->second.IsWater || b->second.IsWater ||
        std::find(a->second.Neighbors.begin(), a->second.Neighbors.end(), second) == a->second.Neighbors.end())
        return std::numeric_limits<double>::infinity();
    const auto link = state.Connections.find({std::min(first, second), std::max(first, second)});
    return EffectiveConnectionDifficulty(a->second.DiffusionCost, a->second.Development,
        b->second.DiffusionCost, b->second.Development, link == state.Connections.end() ? nullptr : &link->second);
}
RegionTraversal InspectRegionTraversal(const Snapshot& state, Id region)
{
    RegionTraversal result;
    for (const auto& item : state.Areas)
    {
        const auto& area = item.second;
        if (area.IsWater || area.RegionId != region) continue;
        ++result.Areas;
        result.MeanAreaDifficulty += EffectiveAreaDifficulty(area.DiffusionCost, area.Development);
        for (const auto neighbor : area.Neighbors)
        {
            if (neighbor <= item.first || state.Areas.at(neighbor).RegionId != region) continue;
            ++result.InternalLinks;
            const auto link = state.Connections.find({item.first, neighbor});
            if (link != state.Connections.end() && link->second.RoadQuality > 0) ++result.RoadLinks;
            const auto cost = TraversalCost(state, item.first, neighbor);
            result.MeanLinkDifficulty += cost;
            result.MaximumLinkDifficulty = std::max(result.MaximumLinkDifficulty, cost);
        }
    }
    if (result.Areas) result.MeanAreaDifficulty /= static_cast<double>(result.Areas);
    if (result.InternalLinks) result.MeanLinkDifficulty /= static_cast<double>(result.InternalLinks);
    return result;
}
Result UpdateInfluenceTargets(Snapshot& state)
{
    auto result = Validate(state); if (!result) return result;
    for (const auto& item : state.Domains)
        if (item.second.CapitalAreaId && !item.second.InfluenceBudget)
            return {false, "influence_budget_missing", "Configure weighted reach for older empire " + std::to_string(item.first) + " before development."};
    for (auto& item : state.Areas) item.second.TargetInfluences.clear();
    for (const auto& item : state.Domains)
    {
        const auto& domain = item.second;
        if (!domain.CapitalAreaId) continue;
        const auto distances = TraversalDistances(state.Areas, state.Connections, domain.CapitalAreaId);
        for (auto& area : state.Areas)
        {
            const auto found = distances.find(area.first);
            const double distance = found == distances.end() ? std::numeric_limits<double>::infinity() : found->second;
            const double radial = std::max(0.0, 1 - distance / domain.InfluenceBudget);
            if (radial * radial >= 0.000000001) area.second.TargetInfluences.push_back({item.first, radial * radial});
        }
    }
    return Ok();
}
void AdvanceConnectivity(Snapshot& state)
{
    // Legacy snapshots have no recorded propagation recipe; retain their exact cached field until configured.
    for (const auto& domain : state.Domains)
        if (domain.second.CapitalAreaId && !domain.second.InfluenceBudget) return;
    bool enabled = false;
    for (const auto& domain : state.Domains) enabled = enabled || domain.second.InfluenceBudget > 0;
    if (!enabled) return;
    for (auto& item : state.Areas)
    {
        auto& area = item.second;
        if (area.IsWater) continue;
        std::vector<Area::Influence> current;
        for (const auto& domain : state.Domains)
        {
            const double old = Strength(area.Influences, domain.first), target = Strength(area.TargetInfluences, domain.first);
            const double value = old + (target - old) * state.Connectivity.InfluenceDailyResponse;
            if (value >= 0.000000001) current.push_back({domain.first, value});
        }
        area.Influences = std::move(current);
    }
    struct Flow { Id From; Id To; Id Culture; std::int64_t Count; };
    std::vector<Flow> flows;
    // Plan every source from the same population state; arrivals cannot migrate twice in one day.
    for (auto& item : state.Areas)
    {
        auto& area = item.second;
        if (area.IsWater) continue;
        auto neighbors = area.Neighbors; std::sort(neighbors.begin(), neighbors.end());
        for (auto& cohort : area.Cohorts)
        {
            if (!cohort.Population) continue;
            const double sourcePull = DesiredPopulation(state, area, cohort.CultureId) / (static_cast<double>(cohort.Population) + 1);
            std::vector<std::pair<Id, double>> choices;
            double weight = 0;
            for (const auto neighbor : neighbors)
            {
                const auto& destination = state.Areas.at(neighbor);
                const double pull = DesiredPopulation(state, destination, cohort.CultureId) /
                    (static_cast<double>(Population(destination, cohort.CultureId)) + 1);
                const double attraction = std::max(0.0, pull - sourcePull) / TraversalCost(state, item.first, neighbor);
                if (attraction > 0) { choices.emplace_back(neighbor, attraction); weight += attraction; }
            }
            if (choices.empty()) { cohort.MigrationRemainder = 0; continue; }
            const double exact = static_cast<double>(cohort.Population) * state.Connectivity.MigrationDailyFraction *
                std::min(1.0, weight) + cohort.MigrationRemainder;
            const auto leaving = std::min(cohort.Population, static_cast<std::int64_t>(std::floor(exact)));
            cohort.MigrationRemainder = exact - std::floor(exact);
            std::vector<std::pair<double, std::size_t>> remainders;
            std::vector<std::int64_t> counts(choices.size());
            std::int64_t assigned = 0;
            for (std::size_t n = 0; n < choices.size(); ++n)
            {
                const double share = static_cast<double>(leaving) * choices[n].second / weight;
                counts[n] = static_cast<std::int64_t>(std::floor(share)); assigned += counts[n];
                remainders.emplace_back(share - std::floor(share), n);
            }
            std::sort(remainders.begin(), remainders.end(), [](const auto& a, const auto& b) {
                return a.first == b.first ? a.second < b.second : a.first > b.first;
            });
            for (std::int64_t n = assigned; n < leaving; ++n)
                ++counts[remainders[static_cast<std::size_t>(n - assigned) % remainders.size()].second];
            for (std::size_t n = 0; n < choices.size(); ++n)
                if (counts[n]) flows.push_back({item.first, choices[n].first, cohort.CultureId, counts[n]});
        }
    }
    for (const auto& flow : flows)
    {
        auto& source = state.Areas.at(flow.From).Cohorts;
        for (auto& cohort : source) if (cohort.CultureId == flow.Culture) { cohort.Population -= flow.Count; break; }
        auto& destination = state.Areas.at(flow.To).Cohorts;
        const auto found = std::find_if(destination.begin(), destination.end(), [&](const Cohort& c) { return c.CultureId == flow.Culture; });
        if (found == destination.end()) destination.push_back({flow.Culture, flow.Count, 0, 0});
        else found->Population += flow.Count;
    }
}
Result World::DevelopArea(const std::string& operation, Id actor, Id area, double development)
{
    auto result = CheckOperation(operation); if (!result) return result;
    if (!StateData.Characters.count(actor) || !StateData.Characters.at(actor).Alive || !StateData.Areas.count(area) || StateData.Areas.at(area).IsWater ||
        !std::isfinite(development) || development < 0 || development > 1)
        return {false, "development_invalid", "Use a living authorized actor, existing area and development within 0..1."};
    auto next = StateData; const auto old = next.Areas.at(area).Development;
    next.Areas.at(area).Development = development;
    result = UpdateInfluenceTargets(next); if (!result) return result;
    std::ostringstream detail; detail << "Area " << area << " development " << old << " -> " << development << "; gradual response scheduled";
    next.Events.push_back({operation, "area_development", next.Day, actor, 0, detail.str()});
    return Commit(std::move(next));
}
Result World::DevelopConnection(const std::string& operation, Id actor, const TraversalConnection& connection)
{
    auto result = CheckOperation(operation); if (!result) return result;
    if (!StateData.Characters.count(actor) || !StateData.Characters.at(actor).Alive)
        return {false, "development_actor", "Use a living authorized actor."};
    auto next = StateData;
    next.Connections[{connection.FirstAreaId, connection.SecondAreaId}] = connection;
    result = UpdateInfluenceTargets(next); if (!result) return result;
    std::ostringstream detail; detail << "Link " << connection.FirstAreaId << ':' << connection.SecondAreaId
        << " road=" << connection.RoadQuality << " distanceScale=" << connection.DistanceScale << "; gradual response scheduled";
    next.Events.push_back({operation, "connection_development", next.Day, actor, 0, detail.str()});
    return Commit(std::move(next));
}
Result World::SetInfluenceBudget(const std::string& operation, Id actor, Id domain, double budget)
{
    auto result = CheckOperation(operation); if (!result) return result;
    if (!StateData.Characters.count(actor) || !StateData.Characters.at(actor).Alive ||
        !StateData.Domains.count(domain) || !StateData.Domains.at(domain).CapitalAreaId ||
        !std::isfinite(budget) || budget < 0.001 || budget > 4000000)
        return {false, "influence_budget_invalid", "Use a living actor, seeded empire and weighted reach within 0.001..4000000."};
    auto next = StateData; next.Domains.at(domain).InfluenceBudget = budget;
    // Configure old empires one at a time; enable refresh only once every missing budget is supplied.
    bool ready = true;
    for (const auto& item : next.Domains) if (item.second.CapitalAreaId && !item.second.InfluenceBudget) ready = false;
    if (ready) { result = UpdateInfluenceTargets(next); if (!result) return result; }
    next.Events.push_back({operation, "influence_budget", next.Day, actor, 0,
        "Empire " + std::to_string(domain) + " weighted reach=" + std::to_string(budget)});
    return Commit(std::move(next));
}
}
