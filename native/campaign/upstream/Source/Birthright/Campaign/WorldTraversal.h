#pragma once

#include "World.h"
#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <set>

namespace Birthright::Campaign
{
double EffectiveAreaDifficulty(double terrainCost, double development);
double EffectiveConnectionDifficulty(double firstCost, double firstDevelopment,
    double secondCost, double secondDevelopment, const TraversalConnection* connection);
double TraversalCost(const Snapshot& state, Id first, Id second);
struct RegionTraversal
{
    std::size_t Areas = 0;
    std::size_t InternalLinks = 0;
    std::size_t RoadLinks = 0;
    double MeanAreaDifficulty = 0;
    double MeanLinkDifficulty = 0;
    double MaximumLinkDifficulty = 0;
};
RegionTraversal InspectRegionTraversal(const Snapshot& state, Id region);
Result UpdateInfluenceTargets(Snapshot& state);
void AdvanceConnectivity(Snapshot& state);

// Used by both authoring and live simulation. Ordered queue/strict improvements give stable ties.
template<class AreaType>
std::map<Id, double> TraversalDistances(const std::map<Id, AreaType>& graph,
    const std::map<std::pair<Id, Id>, TraversalConnection>& connections, Id source,
    const std::set<Id>& allowed = {})
{
    std::map<Id, double> distance{{source, 0}};
    using Entry = std::pair<double, Id>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    queue.emplace(0, source);
    while (!queue.empty())
    {
        const auto entry = queue.top(); queue.pop();
        if (entry.first != distance.at(entry.second)) continue;
        const auto& area = graph.at(entry.second);
        for (const auto neighbor : area.Neighbors)
        {
            if (!allowed.empty() && !allowed.count(neighbor)) continue;
            const auto& other = graph.at(neighbor);
            const auto link = connections.find({std::min(entry.second, neighbor), std::max(entry.second, neighbor)});
            const double candidate = entry.first + EffectiveConnectionDifficulty(area.DiffusionCost, area.Development,
                other.DiffusionCost, other.Development, link == connections.end() ? nullptr : &link->second);
            const auto found = distance.find(neighbor);
            if (found == distance.end() || candidate < found->second)
            {
                distance[neighbor] = candidate;
                queue.emplace(candidate, neighbor);
            }
        }
    }
    return distance;
}
}
