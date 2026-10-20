#pragma once

#include "World.h"

namespace Birthright::Campaign
{
// Shared quote for the campaign authority, fastest routing and presentation. Invalid/non-walking links return infinity.
double TravelDurationDays(const Snapshot& state, Id sourceArea, Id destinationArea);
double RouteDurationDays(const Snapshot& state, Id sourceArea, const std::vector<Id>& route);
// Deterministic least-time route through declared links. Output excludes the starting area.
Result FindRoute(const Snapshot& state, Id sourceArea, Id destinationArea, std::vector<Id>& output);
}
