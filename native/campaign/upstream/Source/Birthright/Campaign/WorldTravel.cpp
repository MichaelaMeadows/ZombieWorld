#include "WorldTravel.h"
#include "WorldTraversal.h"

#include <algorithm>
#include <cmath>

namespace Birthright::Campaign
{
double TravelDurationDays(const Snapshot& state, Id sourceArea, Id destinationArea)
{
    return TraversalCost(state,sourceArea,destinationArea) * state.TravelPolicy.DaysPerEffort;
}
double RouteDurationDays(const Snapshot& state, Id sourceArea, const std::vector<Id>& route)
{
    double days = 0;
    for (const auto destination : route)
    {
        days += TravelDurationDays(state,sourceArea,destination);
        sourceArea = destination;
    }
    return days;
}
Result FindRoute(const Snapshot& state, Id sourceArea, Id destinationArea, std::vector<Id>& output)
{
    if (!state.Areas.count(sourceArea) || !state.Areas.count(destinationArea) ||
        state.Areas.at(sourceArea).IsWater || state.Areas.at(destinationArea).IsWater)
        return { false, "route_endpoint", "Walking endpoints must be existing land areas." };
    using Entry = std::pair<double,Id>;
    std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> queue;
    std::map<Id,double> distance{{sourceArea,0}};
    std::map<Id,Id> predecessor{{sourceArea,0}};
    queue.emplace(0,sourceArea);
    while (!queue.empty())
    {
        const auto entry = queue.top(); queue.pop();
        if (entry.first != distance.at(entry.second)) continue;
        if (entry.second == destinationArea) break;
        auto neighbors = state.Areas.at(entry.second).Neighbors;
        std::sort(neighbors.begin(), neighbors.end());
        for (const auto neighbor : neighbors)
        {
            if (!state.Areas.count(neighbor) || state.Areas.at(neighbor).IsWater)
                return { false,"route_link","Walking route contains a missing or water area." };
            const double cost = TravelDurationDays(state,entry.second,neighbor);
            if (!std::isfinite(cost) || cost <= 0)
                return { false,"route_cost","Walking route contains invalid terrain/road effort." };
            const double candidate = entry.first + cost;
            const auto found = distance.find(neighbor);
            if (found == distance.end() || candidate < found->second)
            {
                distance[neighbor] = candidate; predecessor[neighbor] = entry.second;
                queue.emplace(candidate,neighbor);
            }
        }
    }
    if (!predecessor.count(destinationArea)) return { false, "route_unreachable", "Destination has no connected route." };
    std::vector<Id> route;
    for (auto area = destinationArea; area != sourceArea; area = predecessor.at(area)) route.push_back(area);
    std::reverse(route.begin(), route.end());
    output = std::move(route);
    return { true, "accepted", "" };
}
}
