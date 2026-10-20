#pragma once
#include "World.h"

namespace Birthright::Campaign
{
double DesiredHoldings(const Snapshot& state, Id character);
std::size_t HeldRegions(const Snapshot& state, Id character);
Result ValidatePolitics(const Snapshot& state);
Result AdvancePolitics(World& world);
void WritePolitics(const Snapshot& state, std::ostream& output);
Result ReadPolitics(std::istream& input, Snapshot& state);
void ExportPolitics(const Snapshot& state, std::ostream& output);
// Shared strategic force authority: both wars and local defense use these ledgers.
std::uint64_t ForceStrength(const Snapshot& state, Id war, Id domain, Id expedition = 0);
void ApplyForceLosses(Snapshot& state, Id war, Id domain, std::uint64_t loss, bool deaths, Id expedition = 0);
std::uint64_t StrategicLosses(std::uint64_t own, double enemy, double ownReadiness, double enemyReadiness, double fraction);
void EnlistContingent(Snapshot& state, Id war, Id expedition, Id domain, Id lord, Id homeArea, Id culture, std::uint32_t count);
}
