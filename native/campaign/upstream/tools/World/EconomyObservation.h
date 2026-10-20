#pragma once
#include "World.h"
#include <ostream>
namespace Birthright::Campaign::Testing {
void ObserveEconomy(const Snapshot& now, const Snapshot& before, bool baseline, std::map<std::string,double>& metrics);
void WriteEconomyRegions(std::ostream& output, const Snapshot& now, const Snapshot& before, bool baseline);
}
