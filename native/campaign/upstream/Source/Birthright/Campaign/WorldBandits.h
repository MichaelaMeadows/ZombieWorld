#pragma once
#include "BanditEncounter.h"
#include "World.h"
namespace Birthright::Campaign
{
Result ValidateBandits(const Snapshot &state);
Result AdvanceBandits(World &world);
Result ReadBandits(std::istream &input, Snapshot &state);
void WriteBandits(const Snapshot &state, std::ostream &output);
void ExportBandits(const Snapshot &state, std::ostream &output);
std::vector<Id> BanditReach(const Snapshot &state, Id area, unsigned hops);
double AreaSecurity(const Snapshot &state, Id area);
BanditEncounter CampEncounter(const Snapshot &state, Id camp);
Result AdvanceDefense(World& world);
Result ValidateDefense(const Snapshot& state);
void CloseBanditCamp(Snapshot& state, Id camp, unsigned outcome);
} // namespace Birthright::Campaign
