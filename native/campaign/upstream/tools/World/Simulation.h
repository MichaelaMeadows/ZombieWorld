#pragma once

#include "World.h"
#include <functional>

namespace Birthright::Campaign::Testing
{
// Tool-only observation contracts. Gameplay still runs exclusively through World.
struct Condition
{
    std::string Id, Scope = "every", Metric, Operator = "ge";
    double Value = 0;
};
struct ScheduledAction
{
    std::uint32_t Day = 0; // Elapsed days; actions at day zero run before the baseline.
    std::string Id;
    std::function<Result(World&)> Apply;
};
struct Scenario
{
    std::uint32_t Version = 1;
    std::string Id = "world-observation";
    std::uint32_t Days = 3650, ReportEveryDays = 365;
    std::vector<Condition> Conditions;
    std::vector<ScheduledAction> Actions;
};
struct Observation
{
    std::int64_t Day = 0;
    std::uint32_t ElapsedDays = 0;
    bool Final = false;
    std::map<std::string, double> Metrics;
    std::vector<Id> CreatedEmpires, DestroyedEmpires;
    std::vector<Id> NewBirthrightNpcs, LostBirthrightNpcs, ChangedBirthrightNpcs;
};
using Observer = std::function<Result(const Observation&, const Snapshot&)>;

Result SeedBirthrightFamilies(World& world, const std::string& scenarioId);
// Streams a baseline, every exact report boundary, and a final partial interval.
// Callback and declarative conditions run at checkpoints; failures return a nonzero CLI exit.
Result Simulate(World& world, const Scenario& scenario, std::ostream& jsonLines,
    const Observer& observer = {});
void AddSimulationTests(std::vector<std::pair<std::string, std::function<void()>>>& cases);
void AddPoliticsTests(std::vector<std::pair<std::string, std::function<void()>>>& cases);
void AddBanditTests(std::vector<std::pair<std::string, std::function<void()>>>& cases);
void AddMarketTests(std::vector<std::pair<std::string, std::function<void()>>>& cases);
int RunSimulationCli(int argc, char** argv);
}
