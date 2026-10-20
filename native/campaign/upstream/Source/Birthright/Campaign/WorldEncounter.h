#pragma once
#include "World.h"
namespace Birthright::Campaign
{
// BR-050: local authoritative entry policy for prototype encounters.
inline bool CanEnterEncounter(const Snapshot& state, Id player, bool initialized, bool active)
{
    const auto character = state.Characters.find(player);
    if (!initialized || active || character == state.Characters.end() || !character->second.Alive) return false;
    const auto area = state.Areas.find(character->second.AreaId);
    return area != state.Areas.end() && !area->second.IsWater;
}
}
