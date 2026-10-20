#include "../../Source/Birthright/Tests/PlayerProgressionChecks.h"
#include "World.h"
#include "WorldTravel.h"
#include "EmpireGeneration.h"
#include "WorldTraversal.h"
#include "MapGeography.h"
#include "Simulation.h"
#include "WorldCulture.h"

#include <charconv>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace Birthright::Campaign;

namespace
{
void Require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}
void Accepted(const Result& result) { Require(result.Accepted, result.Code + ": " + result.Detail); }
void Near(double actual, double expected) { Require(std::abs(actual - expected) < 0.000001, "Unexpected power or reserve value"); }
Snapshot Fixture()
{
    Snapshot state; Accepted(Generate({}, {}, state));
    // Explicit fixture: two eligible parents, distinct strongest deities, same location.
    auto& a = state.Characters.at(1); auto& b = state.Characters.at(2);
    b.AreaId = a.AreaId;
    for (std::size_t d = 0; d < DeityCount; ++d)
    {
        a.Affinities[d] = { 0, FavorCost(d % 2 ? 30 : 50, state.Policy), 0 };
        b.Affinities[d] = { 0, FavorCost(d % 2 ? 50 : 30, state.Policy), 0 };
    }
    return state;
}
std::string Serialized(const Snapshot& state) { std::ostringstream out; Accepted(Save(state, out)); return out.str(); }
World Family()
{
    World world; Accepted(world.Restore(Fixture())); Accepted(world.Marry("marriage-1", 1, 2));
    Accepted(world.Birth("birth-1", 1000, "Heir \"First\"", 1, 2, Deity::Strife, Deity::Harmony));
    return world;
}
Snapshot ConnectivityFixture()
{
    std::vector<GeographicArea> graph; Accepted(MakeGridGeography(12, 1, 42, graph));
    for (auto& area : graph) { area.SettlementSuitability = 1; area.PopulationCapacity = 10000; }
    graph[3].DiffusionCost = 30;
    EmpireGeneration options; options.EmpireCount = 1; options.RegionsPerEmpire = 1;
    options.ClaimRadius = 100; options.InfluenceRadius = 20; options.Seeds = {{1, Deity::Harmony}};
    Rules rules; rules.DailyBirthRate = 0; rules.DailyDeathRate = 0; rules.DailyStewardFavor = 0;
    Snapshot state; Accepted(GenerateEmpires(graph, options, rules, state));
    for (auto& area : state.Areas) area.second.FoodCapacity = 1000000;
    return state;
}
double Influence(const std::vector<Area::Influence>& values, Id domain = 1)
{
    for (const auto& value : values) if (value.DomainId == domain) return value.Strength;
    return 0;
}
std::int64_t AreaPopulation(const Snapshot& state, Id area)
{
    std::int64_t total = 0;
    for (const auto& cohort : state.Areas.at(area).Cohorts) total += cohort.Population;
    return total;
}
GeographyDocument PolygonFixture()
{
    GeographyDocument document;
    auto& map = document.Geometry; map.Name = "Archipelago"; map.SourceVersion = "1.154.0";
    map.SourceSeed = "fixture"; map.DistanceUnit = "mi"; map.Width = 4; map.Height = 1;
    for (Id id = 1; id <= 4; ++id)
    {
        const double x = static_cast<double>(id-1);
        map.Cells.emplace(id,MapCell{id,{x+0.5,0.5},{{x,0},{x+1,0},{x+1,1},{x,1}},id == 3 ? 10u : 30u,4});
        if (id != 3) { GeographicArea area; area.Key = id; area.X = static_cast<std::int32_t>(id); document.Areas.push_back(area); }
    }
    document.Areas[0].Neighbors = {2}; document.Areas[1].Neighbors = {1}; document.Connections = {{1,2,0.35,1}};
    return document;
}
Snapshot ImportedFixture()
{
    const auto document = PolygonFixture(); EmpireGeneration options;
    options.EmpireCount = 1; options.RegionsPerEmpire = 1; options.ClaimRadius = 20; options.Seeds = {{1,Deity::Harmony}};
    Snapshot state; Accepted(GenerateImportedWorld(document,options,{},state)); return state;
}
int Tests(const std::string& filter = "")
{
    std::vector<std::pair<std::string, std::function<void()>>> cases = {
        { "600 connected areas, 60 regions, separate political domains", [] {
            Snapshot s; Accepted(Generate({}, {}, s)); Accepted(Validate(s));
            Require(s.Areas.size() == 600 && s.Regions.size() == 60 && s.Domains.size() == 6 && s.Characters.size() == 60, "Wrong topology");
            Generation options; options.DomainCount = 3; Accepted(Generate(options, {}, s));
            Require(s.Domains.size() == 3 && s.Cultures.size() == 7, "Politics must not determine pantheon size");
        } },
        { "seed repeatability and seed divergence", [] {
            Snapshot a, b; Accepted(Generate({}, {}, a)); Accepted(Generate({}, {}, b));
            Require(Serialized(a) == Serialized(b), "Seed changed output");
            Generation options; options.Seed = 43; Accepted(Generate(options, {}, b));
            Require(a.Areas.at(1).Cohorts[0].Population != b.Areas.at(1).Cohorts[0].Population, "Different seeds must vary population");
        } },
        { "invalid generation is atomic", [] {
            auto s = Fixture(); const auto before = Serialized(s); Generation bad; bad.RegionWidth = 0;
            Require(!Generate(bad, {}, s) && Serialized(s) == before, "Bad generation changed output");
            bad.Width = std::numeric_limits<std::uint32_t>::max(); Require(!Generate(bad, {}, s), "Oversized map accepted");
        } },
        { "nonlinear progression boundaries and inverse", [] {
            Rules r; Near(PowerFromFavor(0, r), 0); Near(PowerFromFavor(100, r), 1); Near(PowerFromFavor(250, r), 2);
            for (double power : { 0.1, 2.5, 50.0, 100.0, 999999.5, 1000000.0 }) Near(PowerFromFavor(FavorCost(power, r), r), power);
            Require(FavorCost(51, r) - FavorCost(50, r) > FavorCost(2, r) - FavorCost(1, r), "Marginal costs must grow");
        } },
        { "20 percent immediate; 100,80,30 priorities use stronger parent", [] {
            const auto w = Family(); const auto& c = w.State().Characters.at(1000);
            Near(c.Birthright[0].FullPower, 50); Near(c.Affinities[0].BirthrightPower, 10);
            Near(c.Birthright[1].FullPower, 40); Near(c.Affinities[1].BirthrightPower, 8);
            Near(c.Birthright[6].FullPower, 15); Near(c.Affinities[6].BirthrightPower, 3);
            Require(c.Birthright[0].SourceParentId == 1 && c.Birthright[1].SourceParentId == 2, "Wrong stronger parent");
            Require(w.ChildrenOf(1) == std::vector<Id>{1000} && c.Affinities[0].EarnedFavor == 0, "Genealogy or favor provenance wrong");
        } },
        { "marriage, ancestry, priority and duplicate rejection", [] {
            auto w = Family(); const auto before = Serialized(w.State());
            Require(!w.Marry("same", 1, 1) && !w.Marry("child-marriage", 1, 1000), "Ineligible marriage accepted");
            Require(!w.Birth("bad-priority", 1001, "Heir", 1, 2, Deity::Strife, Deity::Strife), "Duplicate priorities accepted");
            Require(!w.Birth("birth-1", 1001, "Heir", 1, 2, Deity::Strife, Deity::Life), "Duplicate operation accepted");
            Require(Serialized(w.State()) == before, "Rejected command mutated state");
        } },
        { "ceremony consumes reserve and cannot be repeated", [] {
            auto w = Family(); const auto before = FavorCost(Power(w.State().Characters.at(1).Affinities[0], w.State().Policy), w.State().Policy);
            Accepted(w.ReleaseBirthright("release-1", 1, 1000));
            const auto& c = w.State().Characters.at(1000); const auto& a = w.State().Characters.at(1).Affinities[0];
            Near(c.Affinities[0].BirthrightPower, 50);
            Near(FavorCost(Power(a, w.State().Policy), w.State().Policy), before - (FavorCost(50, w.State().Policy) - FavorCost(10, w.State().Policy)));
            Require(c.Birthright[0].Released && !c.Birthright[1].Released, "Ceremony released the other parent's grant");
            Require(!w.ReleaseBirthright("release-again", 1, 1000), "Ceremony repeated");
        } },
        { "insufficient ceremony is atomic", [] {
            auto w = Family(); auto s = w.State(); s.Characters.at(1).Affinities[0] = {}; Accepted(w.Restore(s));
            const auto before = Serialized(w.State()); Require(!w.ReleaseBirthright("poor-parent", 1, 1000), "Empty donor accepted");
            Require(Serialized(w.State()) == before, "Partial release survived failure");
        } },
        { "parent deity has one ordinary recipient; fallback and split are explicit", [] {
            auto w = Family(); Accepted(w.Birth("birth-2", 1001, "Second heir", 1, 2, Deity::Strife, Deity::Harmony));
            const auto& second = w.State().Characters.at(1001);
            Require(second.Birthright[0].SourceParentId == 2, "Must fall back to available weaker parent"); Near(second.Birthright[0].FullPower, 30);
            Accepted(w.Birth("birth-3", 1002, "Third heir", 1, 2, Deity::Strife, Deity::Harmony));
            Require(w.State().Characters.at(1002).Birthright[0].SourceParentId == 0, "Already promised power cloned to third child");
            Accepted(w.SplitBirthright("split-1", 1, Deity::Strife, {{1000, 0.5}, {1002, 0.5}}));
            const auto& first = w.State().Characters.at(1000); const auto& third = w.State().Characters.at(1002);
            Near(FavorCost(first.Birthright[0].FullPower, w.State().Policy) + FavorCost(third.Birthright[0].FullPower, w.State().Policy), FavorCost(50, w.State().Policy));
            Near(first.Affinities[0].BirthrightPower, 10); Near(third.Affinities[0].BirthrightPower, 0);
            Accepted(w.ReleaseBirthright("release-third", 1, 1002));
            const auto before = Serialized(w.State());
            Require(!w.SplitBirthright("resplit-vested", 1, Deity::Strife, {{1000, 0.8}, {1002, 0.2}}), "Released power reallocated");
            Require(Serialized(w.State()) == before, "Rejected split mutated state");
        } },
        { "spent immediate birthright is never granted again at source death", [] {
            auto w = Family(); auto s = w.State(); auto& heir = s.Characters.at(1000);
            heir.Affinities[0].BirthrightPower = 0; // Simulate consuming the immediate reserve in a later ceremony.
            Accepted(w.Restore(s)); Accepted(w.RecordDeath("source-death", 1, "test death"));
            const auto& grant = w.State().Characters.at(1000).Birthright[0];
            Near(FavorCost(w.State().Characters.at(1000).Affinities[0].BirthrightPower, s.Policy), FavorCost(50, s.Policy) - FavorCost(10, s.Policy));
            Near(grant.GrantedFavor, FavorCost(50, s.Policy)); Require(grant.Released, "Vesting receipt missing");
        } },
        { "death releases only source parent's grants and vacates offices", [] {
            auto w = Family(); Accepted(w.RecordDeath("death-1", 1, "test death"));
            Near(w.State().Characters.at(1000).Affinities[0].BirthrightPower, 50);
            Near(w.State().Characters.at(1000).Affinities[1].BirthrightPower, 8);
            Require(!w.State().Characters.at(1).Alive && !w.State().Characters.at(2).SpouseId && !w.State().Entities.at(1).HeadCharacterId, "Death references remain live");
            Require(!w.RecordDeath("death-again", 1, "repeat"), "Second death accepted");
            Accepted(w.RecordDeath("death-2", 2, "test death")); Near(w.State().Characters.at(1000).Affinities[1].BirthrightPower, 40);
        } },
        { "Strife 50 plus 50 produces about 71, transfers reserve once", [] {
            auto s = Fixture(); s.Characters.at(2).Affinities[0].EarnedFavor = FavorCost(50, s.Policy);
            World w; Accepted(w.Restore(s)); const auto harmony = Power(s.Characters.at(1).Affinities[1], s.Policy);
            Accepted(w.ResolveStrifeDuel("duel-1", 1, 2)); const auto& a = w.State().Characters.at(1).Affinities[0];
            Near(FavorCost(Power(a, s.Policy), s.Policy), 2 * FavorCost(50, s.Policy));
            Require(Power(a, s.Policy) > 70 && Power(a, s.Policy) < 72, "Duel incorrectly adds power points");
            Near(Power(w.State().Characters.at(1).Affinities[1], s.Policy), harmony);
            Near(Power(w.State().Characters.at(2).Affinities[0], s.Policy), 0);
            Require(!w.ResolveStrifeDuel("duel-1", 1, 2) && !w.ResolveStrifeDuel("duel-2", 1, 2), "Duplicate duel accepted");
        } },
        { "favor validation, replay and malformed affinity", [] {
            World w; Accepted(w.Restore(Fixture())); const auto before = Serialized(w.State());
            Require(!w.AwardFavor("nan", 1, Deity::Life, std::numeric_limits<double>::quiet_NaN(), "bad"), "NaN accepted");
            Require(!w.AwardFavor("negative", 1, Deity::Life, -1, "bad") && !w.AwardFavor("bad-deity", 1, Deity::Count, 1, "bad"), "Invalid favor accepted");
            Require(Serialized(w.State()) == before, "Rejected awards mutate state");
            Accepted(w.AwardFavor("favor-1", 1, Deity::Life, 100, "healing prototype"));
            Require(!w.AwardFavor("favor-1", 1, Deity::Life, 100, "healing prototype"), "Favor replay accepted");
        } },
        { "snapshot validation rejects broken graph, ownership, genealogy and numeric state", [] {
            const auto base = Fixture(); auto s = base; s.Version = 99; Require(!Validate(s), "Version accepted");
            s = base; s.Areas.at(1).Neighbors.clear(); Require(!Validate(s), "Asymmetric map accepted");
            s = base; s.Regions.at(1).OwnerEntityId = 9999; Require(!Validate(s), "Missing owner accepted");
            s = base; s.Characters.at(1).Parents = {1}; Require(!Validate(s), "Lineage cycle accepted");
            s = base; s.Characters.at(1).Affinities[0].EarnedFavor = std::numeric_limits<double>::infinity(); Require(!Validate(s), "Infinite favor accepted");
            s = base; s.Areas.at(1).Cohorts[0].Population = -1; Require(!Validate(s), "Negative population accepted");
        } },
        { "save resume matches uninterrupted clock and preserves operation receipts", [] {
            auto a = Family(); Accepted(a.AdvanceDays(180)); std::istringstream input(Serialized(a.State()));
            Snapshot s; Accepted(Load(input, s)); World b; Accepted(b.Restore(s));
            Accepted(a.AdvanceDays(185)); Accepted(b.AdvanceDays(185)); Require(Serialized(a.State()) == Serialized(b.State()), "Resume diverged");
            Require(!b.Marry("marriage-1", 3, 4), "Saved operation receipt lost");
            auto c = Family(); Accepted(c.AdvanceDays(365)); Require(Serialized(a.State()) == Serialized(c.State()), "Batch size changed behavior");
        } },
        { "malformed saves fail without replacing valid output", [] {
            auto s = Fixture(); const auto before = Serialized(s);
            for (const auto& data : { std::string("BIRTHRIGHT_WORLD 119"), before.substr(0, before.size() / 2), before + " garbage" })
            {
                std::istringstream input(data); Require(!Load(input, s), "Corrupt save accepted"); Require(Serialized(s) == before, "Corruption replaced world");
            }
        } },
        { "ten years of cohorts remain bounded without individual materialization", [] {
            Snapshot s; Accepted(Generate({}, {}, s)); World w; Accepted(w.Restore(s)); Accepted(w.AdvanceDays(3650)); Accepted(Validate(w.State()));
            Require(w.Inspect().Population > 0 && w.State().Characters.size() == 60 && w.State().Areas.size() == 600, "Cohorts became individual characters");
            Require(!w.AdvanceDays(0) && !w.AdvanceDays(36501), "Invalid fast-forward accepted");
        } },
        { "food shortage raises unrest and loss; tiny cohorts retain fractional births", [] {
            auto s = Fixture(); s.Policy.DailyDeathRate = 0; s.Policy.DailyBirthRate = 0.01; s.Policy.DailyStewardFavor = 0;
            for (auto& item : s.Areas) { item.second.Cohorts = {{1, 1, 0, 0}}; item.second.FoodCapacity = 100; }
            World w; Accepted(w.Restore(s)); Accepted(w.AdvanceDays(100)); Require(w.State().Areas.at(1).Cohorts[0].Population >= 2, "Fractional births were lost");
            s = Fixture(); s.Areas.at(1).FoodCapacity = 1; Accepted(w.Restore(s)); const auto original = s.Areas.at(1).Cohorts[0].Population;
            Accepted(w.AdvanceDays(30)); Require(w.State().Areas.at(1).Unrest > s.Areas.at(1).Unrest && w.State().Areas.at(1).Cohorts[0].Population < original, "Shortage had no consequence");
        } },
        { "travel follows shortest connected route and costs one day per edge", [] {
            World w; Accepted(w.Restore(Fixture())); std::vector<Id> route;
            Accepted(FindRoute(w.State(), 1, 63, route)); Require(route.size() == 4, "Wrong shortest route length");
            std::vector<Id> repeated; Accepted(FindRoute(w.State(), 1, 63, repeated)); Require(route == repeated, "Routing changed across repeated requests");
            std::size_t step = 0;
            for (const auto area : route) Accepted(w.Travel("journey-" + std::to_string(++step), 1, area));
            Require(w.State().Characters.at(1).AreaId == 63 && w.State().Day == 4, "Travel state or time wrong");
            Accepted(FindRoute(w.State(), 63, 63, route)); Require(route.empty(), "Already at destination must require no travel");
        } },
        { "invalid, dead and replayed travel cannot mutate state", [] {
            World w; Accepted(w.Restore(Fixture())); const auto before = Serialized(w.State());
            Require(!w.Travel("jump", 1, 600) && !w.Travel("stay", 1, 1) && !w.Travel("missing", 1, 9999), "Invalid movement accepted");
            Require(Serialized(w.State()) == before, "Invalid movement changed clock or position");
            Accepted(w.Travel("edge-1", 1, 2)); const auto after = Serialized(w.State());
            Require(!w.Travel("edge-1", 1, 3) && Serialized(w.State()) == after, "Replay moved or advanced clock");
            Accepted(w.RecordDeath("dead-traveler", 1, "test")); Require(!w.Travel("dead-edge", 1, 3), "Dead traveler moved");
        } },
        { "travel survives save/reload and rolls back when campaign clock is exhausted", [] {
            World w; Accepted(w.Restore(Fixture())); Accepted(w.Travel("saved-edge", 1, 2));
            std::istringstream input(Serialized(w.State())); Snapshot s; Accepted(Load(input, s)); World resumed; Accepted(resumed.Restore(s));
            Require(!resumed.Travel("saved-edge", 1, 3), "Travel receipt lost after reload");
            Accepted(resumed.Travel("new-edge", 1, 3)); Require(resumed.State().Day == 2, "Resumed clock wrong");
            s = resumed.State(); s.Day = 100000000; Accepted(resumed.Restore(s)); const auto before = Serialized(resumed.State());
            Require(!resumed.Travel("exhausted", 1, 4) && Serialized(resumed.State()) == before, "Clock failure partially moved player");
        } },
        { "route failure preserves previous output", [] {
            auto s = Fixture(); std::vector<Id> route{ 7, 8 };
            Require(!FindRoute(s, 0, 2, route) && route == std::vector<Id>({7,8}), "Bad endpoint changed route");
            s.Areas.at(1).Neighbors.clear(); Require(!FindRoute(s, 1, 600, route), "Disconnected route accepted");
        } },
        { "empire seeds create exact connected regions and named owning houses", [] {
            std::vector<GeographicArea> graph; Accepted(MakeGridGeography(30, 20, 42, graph));
            Snapshot s; Accepted(GenerateEmpires(graph, {}, {}, s)); Accepted(Validate(s));
            Require(s.Domains.size() == 6 && s.Regions.size() == 36 && s.Entities.size() == 36 && s.Characters.size() == 42, "N/K or named ownership incorrect");
            const auto m = InspectEmpires(s);
            Require(m.ClaimedAreas + m.ContestedAreas + m.UnclaimedAreas == 600, "Sovereignty classifications overlap");
            Require(m.UnclaimedAreas > 0 && m.WildernessAreas > 0, "No outskirts/wilderness");
            Require(std::accumulate(m.Roles.begin(), m.Roles.end(), std::int64_t{0}) == m.Population, "Roles duplicated or lost inhabitants");
            for (const auto& domain : s.Domains) {
                std::size_t count = 0; for (const auto& region : s.Regions) if (region.second.DomainId == domain.first) ++count;
                Require(count == 6 && domain.second.HeadCharacterId && domain.second.CapitalAreaId, "Missing regional subdivision/ruler");
            }
        } },
        { "empire generation is seeded and independent of input/neighbor order", [] {
            std::vector<GeographicArea> graph; Accepted(MakeGridGeography(30, 20, 42, graph));
            Snapshot a, b; Accepted(GenerateEmpires(graph, {}, {}, a));
            std::reverse(graph.begin(), graph.end()); for (auto& area : graph) std::reverse(area.Neighbors.begin(), area.Neighbors.end());
            Accepted(GenerateEmpires(graph, {}, {}, b)); Require(Serialized(a) == Serialized(b), "Container ordering changed seeded world");
            EmpireGeneration different; different.Seed = 43; Accepted(GenerateEmpires(graph, different, {}, b));
            Require(a.Domains.at(1).CapitalAreaId != b.Domains.at(1).CapitalAreaId, "Seed did not change capital placement");
        } },
        { "contested borders have no owner while populations blend across them", [] {
            std::vector<GeographicArea> graph; Accepted(MakeGridGeography(10, 1, 42, graph));
            for (auto& area : graph) { area.SettlementSuitability = 1; area.PopulationCapacity = 10000; }
            EmpireGeneration o; o.EmpireCount = 2; o.RegionsPerEmpire = 2; o.ClaimRadius = 5; o.InfluenceRadius = 8;
            o.Seeds = {{1, Deity::Strife}, {10, Deity::Order}};
            Snapshot s; Accepted(GenerateEmpires(graph, o, {}, s));
            Require(s.Areas.at(5).RegionId == 0 && s.Areas.at(6).RegionId == 0 && s.Areas.at(5).Claimants == std::vector<Id>({1,2}), "Contest assigned an owner");
            Require(s.Areas.at(5).Cohorts.size() == 3 && s.Areas.at(5).Influences.size() == 2, "Frontier populations did not mix");
            Require(s.Areas.at(4).RegionId && s.Areas.at(4).Cohorts.size() == 3, "Foreign culture did not cross a recognized border");
            const auto m = InspectEmpires(s); Require(m.ContestedAreas == 2 && m.ClaimedAreas == 8, "Contested area counts wrong");
        } },
        { "claim borders remain fixed when cultural reach changes; wilderness stays sparse", [] {
            std::vector<GeographicArea> graph; Accepted(MakeGridGeography(30, 1, 42, graph));
            for (auto& area : graph) { area.SettlementSuitability = 1; area.PopulationCapacity = 10000; }
            EmpireGeneration o; o.EmpireCount = 2; o.RegionsPerEmpire = 2; o.ClaimRadius = 4;
            o.Seeds = {{5, Deity::Life}, {25, Deity::Order}};
            Snapshot a, b; Accepted(GenerateEmpires(graph, o, {}, a));
            Require(!a.Areas.at(10).RegionId && !a.Areas.at(10).Influences.empty(), "Cultural reach must extend into unclaimed land");
            Require(a.Areas.at(15).Wilderness, "Remote area was not wilderness");
            std::int64_t sparse = 0; for (const auto& c : a.Areas.at(15).Cohorts) sparse += c.Population;
            Require(sparse == 5, "Wilderness baseline was not sparse");
            o.Seeds[0].InfluenceReach = 2; Accepted(GenerateEmpires(graph, o, {}, b));
            for (const auto& area : a.Areas) Require(area.second.RegionId == b.Areas.at(area.first).RegionId &&
                area.second.Claimants == b.Areas.at(area.first).Claimants, "Culture regenerated political borders");
            Require(!b.Areas.at(15).Wilderness, "Increasing cultural reach did not extend settlement");
        } },
        { "terrain costs slow diffusion and unsuitable land limits settlement", [] {
            std::vector<GeographicArea> graph; Accepted(MakeGridGeography(15, 1, 42, graph));
            for (auto& area : graph) { area.SettlementSuitability = 1; area.PopulationCapacity = 10000; }
            EmpireGeneration o; o.EmpireCount = 1; o.RegionsPerEmpire = 2; o.InfluenceRadius = 20; o.ClaimRadius = 20;
            o.Seeds = {{1, Deity::Harmony}};
            Snapshot a, b; Accepted(GenerateEmpires(graph, o, {}, a));
            graph[7].DiffusionCost = 5; graph[10].SettlementSuitability = 0.01;
            Accepted(GenerateEmpires(graph, o, {}, b));
            Require(b.Areas.at(10).Influences[0].Strength < a.Areas.at(10).Influences[0].Strength, "Barrier did not reduce influence beyond it");
            Require(b.Areas.at(11).Wilderness, "Unsuitable land must remain sparse");
        } },
        { "deity profiles bias roles and blended patrons conserve population", [] {
            std::vector<GeographicArea> graph; Accepted(MakeGridGeography(10, 1, 42, graph));
            for (auto& area : graph) { area.SettlementSuitability = 1; area.PopulationCapacity = 10000; }
            EmpireGeneration o; o.EmpireCount = 1; o.RegionsPerEmpire = 2; o.Seeds = {{1, Deity::Strife}};
            Snapshot strife, order, blend; Accepted(GenerateEmpires(graph, o, {}, strife));
            o.Seeds[0].PrimaryPatron = Deity::Order; Accepted(GenerateEmpires(graph, o, {}, order));
            const auto fighters = EstimatePopulationRoles(strife, 1), industry = EstimatePopulationRoles(order, 1);
            Require(fighters[0] > industry[0] && fighters[2] < industry[2] && fighters[1] > industry[1], "Society biases were not applied");
            o.Seeds[0] = {1, Deity::Strife, Deity::Life, 0.25, 1};
            Accepted(GenerateEmpires(graph, o, {}, blend));
            Near(blend.Cultures.at(1).Occupations.Roles[0], 0.25 * 0.35 + 0.75 * 0.04);
            const auto mixedRoles = EstimatePopulationRoles(blend, 1);
            Require(mixedRoles[1] > fighters[1] && blend.Areas.at(1).FoodCapacity > strife.Areas.at(1).FoodCapacity, "Life blend did not improve farming and food supply");
            Require(blend.Cultures.at(1).SecondaryPatron == Deity::Life, "Second patron lost");
            for (const auto& area : blend.Areas) {
                std::int64_t population = 0; for (const auto& c : area.second.Cohorts) population += c.Population;
                const auto roles = EstimatePopulationRoles(blend, area.first);
                Require(std::accumulate(roles.begin(), roles.end(), std::int64_t{0}) == population, "Role rounding changed population");
            }
        } },
        { "invalid empire generation and insufficient region capacity are atomic", [] {
            std::vector<GeographicArea> graph; Accepted(MakeGridGeography(10, 1, 42, graph));
            for (auto& area : graph) area.SettlementSuitability = 1;
            Snapshot s = Fixture(); const auto before = Serialized(s);
            EmpireGeneration o; o.EmpireCount = 1; o.RegionsPerEmpire = 2; o.Seeds = {{1, Deity::Strife}}; o.ClaimRadius = 0.1;
            Require(!GenerateEmpires(graph, o, {}, s) && Serialized(s) == before, "Insufficient territory overwrote output");
            o.ClaimRadius = 5; o.Profiles[0].Roles[0] = std::numeric_limits<double>::quiet_NaN();
            Require(!GenerateEmpires(graph, o, {}, s) && Serialized(s) == before, "NaN profile accepted or changed state");
            o = {}; o.EmpireCount = 2; o.RegionsPerEmpire = 2; o.Seeds = {{1, Deity::Life}, {1, Deity::Order}};
            Require(!GenerateEmpires(graph, o, {}, s), "Duplicate capital accepted");
            o.Seeds.clear(); graph[0].Neighbors.clear();
            Require(!GenerateEmpires(graph, o, {}, s) && Serialized(s) == before, "Asymmetric geography accepted or changed state");
        } },
        { "empire saves resume society and frontier state without losing lineage", [] {
            std::vector<GeographicArea> graph; Accepted(MakeGridGeography(30, 20, 42, graph));
            Snapshot s; Accepted(GenerateEmpires(graph, {}, {}, s)); World a; Accepted(a.Restore(s));
            Accepted(a.Marry("empire-marriage", 1, 2)); Accepted(a.Birth("empire-heir", 9999, "Heir", 1, 2, Deity::Strife, Deity::Order));
            Accepted(a.AdvanceDays(30)); std::istringstream input(Serialized(a.State()));
            Snapshot saved; Accepted(Load(input, saved)); World b; Accepted(b.Restore(saved));
            Accepted(a.AdvanceDays(30)); Accepted(b.AdvanceDays(30));
            Require(Serialized(a.State()) == Serialized(b.State()) && b.State().Characters.at(9999).Parents.size() == 2, "Resume lost generation metadata or lineage");
            std::ostringstream json; Accepted(ExportJson(b.State(), json));
            Require(json.str().find("estimatedRoles") != std::string::npos && json.str().find("influences") != std::string::npos, "Inspection fields missing");
            std::ostringstream svg; Accepted(ExportEmpireSvg(b.State(), svg)); Require(svg.str().find("<svg") != std::string::npos, "Overview missing");
        } },
        { "version one saves migrate with neutral metadata and preserve all old state", [] {
            auto s = Fixture();
            // Version 1 permitted disconnected holdings; migration must not silently tighten that policy.
            s.Areas.at(600).RegionId = 1;
            auto old = Serialized(s); const auto metadata = old.find("EMPIRE_METADATA");
            Require(metadata != std::string::npos, "No version two metadata"); old.resize(metadata);
            old.replace(old.find("BIRTHRIGHT_WORLD 11"), std::string("BIRTHRIGHT_WORLD 11").size(), "BIRTHRIGHT_WORLD 1");
            std::istringstream input(old); Snapshot migrated; Accepted(Load(input, migrated));
            Require(migrated.Version == 11 && Serialized(migrated) == Serialized(s), "Migration changed legacy behavior or dropped records");
            auto corrupt = Serialized(s); corrupt.replace(corrupt.find("EMPIRE_METADATA 1"), std::string("EMPIRE_METADATA 1").size(), "EMPIRE_METADATA 99");
            const auto before = Serialized(migrated); std::istringstream bad(corrupt);
            Require(!Load(bad, migrated) && Serialized(migrated) == before, "Bad metadata partially loaded");
        } },
        { "ownership, influence and occupational metadata are validated", [] {
            std::vector<GeographicArea> graph; Accepted(MakeGridGeography(30, 20, 42, graph));
            Snapshot s; Accepted(GenerateEmpires(graph, {}, {}, s));
            auto bad = s; bad.Areas.begin()->second.Influences.push_back({9999, 0.5}); Require(!Validate(bad), "Missing empire influence accepted");
            bad = s; bad.Cultures.begin()->second.Occupations.Roles[0] = 1; Require(!Validate(bad), "Unnormalized society accepted");
            bad = s; auto& contestedOwner = bad.Areas.at(bad.Domains.at(1).CapitalAreaId); contestedOwner.Claimants = {1,2}; Require(!Validate(bad), "Contested territory retained a recognized owner");
            bad = s; auto& unowned = bad.Areas.at(bad.Domains.at(1).CapitalAreaId); unowned.RegionId = 0; Require(!Validate(bad), "Unclaimed capital accepted");
            bad = s;
            const auto isolated = std::find_if(bad.Areas.begin(), bad.Areas.end(), [&](const auto& item) {
                return !item.second.RegionId && std::none_of(item.second.Neighbors.begin(), item.second.Neighbors.end(),
                    [&](Id neighbor) { return bad.Areas.at(neighbor).RegionId == 1; });
            });
            Require(isolated != bad.Areas.end(), "No isolated unclaimed fixture area");
            isolated->second.RegionId = 1; isolated->second.Claimants.clear();
            Require(!Validate(bad), "Disconnected seeded region accepted");
        } },
        { "roads affect only their connections and terrain effort stays meaningful", [] {
            auto state = ConnectivityFixture();
            Near(TraversalCost(state, 1, 2), 1);
            state.Areas.at(2).DiffusionCost = 3; Near(TraversalCost(state, 1, 2), 2);
            state.Connections[{1,2}] = {1,2,1,1}; Near(TraversalCost(state, 1, 2), 0.5);
            Near(TraversalCost(state, 2, 3), 2); Near(TraversalCost(state, 3, 4), 15.5);
            Near(TraversalCost(state, 2, 1), 0.5);
            state.Areas.at(2).Development = 1; Near(TraversalCost(state, 2, 3), 0.875);
            Require(!std::isfinite(TraversalCost(state, 1, 12)), "Nonadjacent travel cost invented");
        } },
        { "diffusion chooses connected detours and road corridors at generation", [] {
            std::vector<GeographicArea> graph; Accepted(MakeGridGeography(3, 2, 42, graph));
            for (auto& area : graph) area.SettlementSuitability = 1;
            graph[1].DiffusionCost = 30;
            EmpireGeneration o; o.EmpireCount = 1; o.RegionsPerEmpire = 1; o.ClaimRadius = 100;
            o.Seeds = {{1, Deity::Harmony}};
            Snapshot before, after; Accepted(GenerateEmpires(graph, o, {}, before));
            const auto distances = TraversalDistances(before.Areas, before.Connections, 1);
            Near(distances.at(3), 4); Require(distances.at(2) > 10, "Mountain did not resist influence");
            o.Connections = {{1,4,1,1}, {4,5,1,1}, {5,6,1,1}, {3,6,1,1}};
            Accepted(GenerateEmpires(graph, o, {}, after));
            Near(TraversalDistances(after.Areas, after.Connections, 1).at(3), 1);
            Require(Influence(after.Areas.at(3).Influences) > Influence(before.Areas.at(3).Influences), "Road corridor did not extend cultural reach");
            std::reverse(o.Connections.begin(), o.Connections.end()); Snapshot ordered; Accepted(GenerateEmpires(graph, o, {}, ordered));
            Require(Serialized(after) == Serialized(ordered), "Road input order changed result");
        } },
        { "development schedules gradual influence without instant relocation or border changes", [] {
            World world; Accepted(world.Restore(ConnectivityFixture())); const auto original = world.State();
            const auto immediate = Influence(original.Areas.at(8).Influences);
            Accepted(world.DevelopConnection("pass-west", 1, {3,4,1,1}));
            Accepted(world.DevelopConnection("pass-east", 1, {4,5,1,1}));
            Near(Influence(world.State().Areas.at(8).Influences), immediate);
            Near(static_cast<double>(AreaPopulation(world.State(), 8)), static_cast<double>(AreaPopulation(original, 8)));
            const auto target = Influence(world.State().Areas.at(8).TargetInfluences);
            Require(target > immediate, "No new target past pass");
            Accepted(world.AdvanceDays(1));
            Near(Influence(world.State().Areas.at(8).Influences), immediate + (target-immediate)*0.2);
            Near(static_cast<double>(AreaPopulation(world.State(), 8)), static_cast<double>(AreaPopulation(original, 8)));
            for (const auto& area : original.Areas) Require(world.State().Areas.at(area.first).RegionId == area.second.RegionId &&
                world.State().Areas.at(area.first).Claimants == area.second.Claimants, "Development changed political borders");
            const auto firstDay = world.Inspect().Population; Accepted(world.AdvanceDays(180));
            Require(world.Inspect().Population == firstDay, "Migration created/lost people");
            Require(AreaPopulation(world.State(), 5) > AreaPopulation(original, 5), "Population did not follow improved connectivity gradually");
            Require(Influence(world.State().Areas.at(8).Influences) > target*0.99, "Influence did not converge");
            Require(world.State().Characters.at(1).AreaId == original.Characters.at(1).AreaId, "Named ruler moved with cohorts");
        } },
        { "migration uses synchronous neighboring flows and is bounded per source", [] {
            auto state = ConnectivityFixture();
            for (auto& area : state.Areas) { area.second.DiffusionCost = 1; area.second.Cohorts = {{2,0,0,0}}; }
            state.Areas.at(1).Cohorts = {{1,10000,0,0}};
            Accepted(UpdateInfluenceTargets(state));
            for (auto& area : state.Areas) area.second.Influences = area.second.TargetInfluences;
            World world; Accepted(world.Restore(state)); Accepted(world.AdvanceDays(1));
            Require(AreaPopulation(world.State(), 1) >= 9980 && AreaPopulation(world.State(), 2) > 0 &&
                AreaPopulation(world.State(), 3) == 0 && world.Inspect().Population == 10000, "Migrants exceeded rate, teleported or duplicated");
            const auto population = world.Inspect().Population; Accepted(world.AdvanceDays(365));
            Require(world.Inspect().Population == population && AreaPopulation(world.State(), 3) > 0, "Daily migration did not conserve people/progress");
        } },
        { "development errors replay and invalid authoring are atomic", [] {
            World world; Accepted(world.Restore(ConnectivityFixture())); const auto before = Serialized(world.State());
            Require(!world.DevelopConnection("bad-road", 1, {1,12,1,1}) && Serialized(world.State()) == before, "Nonadjacent road changed world");
            Require(!world.DevelopArea("bad-area", 1, 2, std::numeric_limits<double>::quiet_NaN()) && Serialized(world.State()) == before, "NaN development changed world");
            Require(!world.DevelopConnection("bad-road2", 1, {2,1,1,1}), "Reversed road accepted");
            Accepted(world.DevelopArea("clearing", 1, 4, 0.8)); const auto after = Serialized(world.State());
            Require(!world.DevelopArea("clearing", 1, 4, 1) && Serialized(world.State()) == after, "Repeated clearing changed world");
            auto bad = world.State(); bad.Connectivity.MigrationDailyFraction = 0.2; Require(!Validate(bad), "Migration faster than influence accepted");
            bad = world.State(); bad.Areas.at(1).TargetInfluences.push_back({999,1}); Require(!Validate(bad), "Invalid target reference accepted");
            std::vector<GeographicArea> graph; Accepted(MakeGridGeography(12, 1, 42, graph));
            EmpireGeneration o; o.EmpireCount = 1; o.RegionsPerEmpire = 1; o.Connections = {{1,2,1,1},{1,2,1,1}};
            auto output = world.State(); Require(!GenerateEmpires(graph, o, {}, output) && Serialized(output) == after, "Duplicate road replaced output");
        } },
        { "regional traversal reports actual internal roads and barriers", [] {
            auto state = ConnectivityFixture(); const auto before = InspectRegionTraversal(state, 1);
            Require(before.Areas == 12 && before.InternalLinks == 11 && before.RoadLinks == 0, "Region counts wrong");
            Near(before.MaximumLinkDifficulty, 15.5);
            state.Connections[{3,4}] = {3,4,1,1};
            const auto after = InspectRegionTraversal(state, 1);
            Require(after.RoadLinks == 1 && after.MeanLinkDifficulty < before.MeanLinkDifficulty, "Regional road effect missing");
            Near(after.MeanAreaDifficulty, before.MeanAreaDifficulty);
        } },
        { "connectivity saves retain pending response rounding lineage and receipts", [] {
            World a; Accepted(a.Restore(ConnectivityFixture()));
            Accepted(a.Marry("couple", 1, 2)); Accepted(a.Birth("child", 99, "Heir", 1, 2, Deity::Life, Deity::Harmony));
            Accepted(a.DevelopConnection("road", 1, {3,4,1,1})); Accepted(a.DevelopArea("pass", 1, 4, 1)); Accepted(a.AdvanceDays(7));
            std::istringstream input(Serialized(a.State())); Snapshot saved; Accepted(Load(input, saved));
            World b; Accepted(b.Restore(saved)); Accepted(a.AdvanceDays(90));
            for (int day = 0; day < 90; ++day) Accepted(b.AdvanceDays(1));
            Require(Serialized(a.State()) == Serialized(b.State()) && b.State().Characters.at(99).Parents.size() == 2, "Response resume lost state");
            const auto before = Serialized(b.State()); Require(!b.DevelopConnection("road", 1, {3,4,0,1}) && Serialized(b.State()) == before, "Receipt missing after reload");
            auto malformed = before; const auto pos = malformed.find("TRAVERSAL_METADATA 1"); malformed.replace(pos, std::string("TRAVERSAL_METADATA 1").size(), "TRAVERSAL_METADATA 99");
            const auto savedBefore = Serialized(saved);
            std::istringstream corrupt(malformed); Require(!Load(corrupt, saved) && Serialized(saved) == savedBefore, "Unsupported metadata partially loaded");
            auto truncated = before.substr(0, before.size() - 20); std::istringstream shortInput(truncated);
            Require(!Load(shortInput, saved) && Serialized(saved) == savedBefore, "Truncated connectivity metadata partially loaded");
            std::ostringstream json; Accepted(ExportJson(b.State(), json));
            Require(json.str().find("targetInfluences") != std::string::npos && json.str().find("meanLinkDifficulty") != std::string::npos, "Inspection missing");
        } },
        { "v2 migration retains cached fields until explicit propagation configuration", [] {
            const auto original = ConnectivityFixture(); auto old = Serialized(original); old.resize(old.find("TRAVERSAL_METADATA"));
            old.replace(old.find("BIRTHRIGHT_WORLD 11"), std::string("BIRTHRIGHT_WORLD 11").size(), "BIRTHRIGHT_WORLD 2");
            std::istringstream input(old); Snapshot migrated; Accepted(Load(input, migrated));
            Require(migrated.Version == 11 && migrated.Domains.at(1).InfluenceBudget == 0, "Missing recipe silently guessed");
            World world; Accepted(world.Restore(migrated)); Accepted(world.AdvanceDays(7));
            Near(Influence(world.State().Areas.at(3).Influences), Influence(original.Areas.at(3).Influences));
            const auto before = Serialized(world.State()); Require(!world.DevelopArea("unconfigured", 1, 4, 1) && Serialized(world.State()) == before, "Unknown reach used");
            Accepted(world.SetInfluenceBudget("reach", 1, 1, 20)); Accepted(world.DevelopArea("configured", 1, 4, 1));
            Require(Influence(world.State().Areas.at(8).TargetInfluences) > 0, "Configured legacy propagation failed");
        } },
        { "import preserves polygons islands roads and water exclusion", [] {
            const auto state = ImportedFixture();
            Require(state.Areas.size() == 4 && state.Geometry.Cells.size() == 4 && state.Connections.size() == 1, "Geometry lost cells/roads");
            Require(state.Areas.at(3).IsWater && !state.Areas.at(4).RegionId && state.Areas.at(4).Influences.empty(), "Water walkable or influence crossed ocean");
            Require(AreaAtMapPoint(state,{0.5,0.5}) == 1 && AreaAtMapPoint(state,{2.5,0.5}) == 0 &&
                AreaAtMapPoint(state,{3.5,0.5}) == 4 && AreaAtMapPoint(state,{-1,0.5}) == 0, "Polygon hit testing failed");
            Require(AreaAtMapPoint(state,{1,0.5}) == 1, "Shared edge tie unstable");
            std::vector<Id> route{99}; Require(!FindRoute(state,1,4,route) && route == std::vector<Id>({99}), "Sea route invented");
            Accepted(FindRoute(state,1,2,route)); Require(route == std::vector<Id>({2}), "Land route lost");
            World world; Accepted(world.Restore(state)); Accepted(world.DevelopConnection("island-road",1,{1,2,1,1})); Accepted(world.AdvanceDays(3));
            Require(world.State().Areas.at(4).Influences.empty(), "Development leaked across water");
        } },
        { "normalized geography loader validates complete symmetric land links", [] {
            const auto document = PolygonFixture();
            std::ostringstream text; text << "BIRTHRIGHT_GEOGRAPHY 1\n"; Accepted(WriteGeometry(document.Geometry,text));
            text << "3\n1 1 2\n2 1 1\n4 0\n1\n1 2 0.35\n";
            GeographyDocument loaded; std::istringstream input(text.str()); Accepted(LoadGeography(input,loaded));
            Require(loaded.Areas.size() == 3 && loaded.Connections.size() == 1, "Normalized geography lost records");
            Near(loaded.Areas[0].SettlementSuitability,0.9); Near(loaded.Connections[0].RoadQuality,0.35);
            const auto count = loaded.Areas.size();
            auto malformed = text.str(); malformed.replace(malformed.find("2 1 1\n4 0"),std::string("2 1 1\n4 0").size(),"2 0\n4 0");
            std::istringstream bad(malformed); Require(!LoadGeography(bad,loaded) && loaded.Areas.size() == count, "Asymmetric import partially loaded");
            std::istringstream trailing(text.str()+"unexpected"); Require(!LoadGeography(trailing,loaded), "Trailing import data accepted");
        } },
        { "polygon persistence resume retains geometry and campaign lineage", [] {
            auto state = ImportedFixture(); World first; Accepted(first.Restore(state)); Accepted(first.Marry("pair",1,2));
            Accepted(first.Birth("heir",99,"Heir",1,2,Deity::Life,Deity::Harmony));
            std::istringstream input(Serialized(first.State())); Snapshot restored; Accepted(Load(input,restored));
            Require(Serialized(restored) == Serialized(first.State()), "Polygon save changed values");
            World second; Accepted(second.Restore(restored)); Accepted(first.AdvanceDays(10)); Accepted(second.AdvanceDays(10));
            Require(Serialized(first.State()) == Serialized(second.State()) && second.State().Characters.at(99).Parents.size() == 2, "Imported lineage/resume diverged");
            std::ostringstream svg,json; Accepted(ExportEmpireSvg(restored,svg)); Accepted(ExportJson(restored,json));
            Require(svg.str().find("<polygon") != std::string::npos && json.str().find("\"polygon\"") != std::string::npos, "Polygon inspection missing");
        } },
        { "invalid polygons water areas and unsupported geometry reject atomically", [] {
            const auto state = ImportedFixture(); auto bad = state;
            bad.Geometry.Cells.at(1).Polygon[0].X = std::numeric_limits<double>::quiet_NaN(); Require(!Validate(bad), "NaN vertex accepted");
            bad = state; bad.Geometry.Cells.at(1).Polygon = {{0,0},{1,0},{0,0}}; Require(!Validate(bad), "Degenerate polygon accepted");
            bad = state; bad.Geometry.Cells.at(1).Height = 10; Require(!Validate(bad), "Walkable water accepted");
            auto encoded = Serialized(state); const auto pos = encoded.find("MAP_GEOMETRY 1"); encoded.replace(pos,14,"MAP_GEOMETRY 9");
            Snapshot output = state; const auto before = Serialized(output); std::istringstream input(encoded);
            Require(!Load(input,output) && Serialized(output) == before, "Unknown geometry partially loaded");
        } },
        { "version three migration retains connectivity with neutral geometry", [] {
            auto state = ConnectivityFixture(); auto encoded = Serialized(state); encoded.resize(encoded.find("MAP_GEOMETRY"));
            encoded.replace(encoded.find("BIRTHRIGHT_WORLD 11"),std::string("BIRTHRIGHT_WORLD 11").size(),"BIRTHRIGHT_WORLD 3");
            Snapshot migrated; std::istringstream input(encoded); Accepted(Load(input,migrated));
            Require(migrated.Version == 11 && migrated.Geometry.Cells.empty() && Serialized(migrated) == Serialized(state), "v3 migration changed connectivity");
        } },
        { "every polygon is an area and water stays inert", [] {
            const auto state = ImportedFixture(); World world; Accepted(world.Restore(state));
            Require(state.Areas.size() == state.Geometry.Cells.size(), "Missing cell areas");
            for (const auto& cell : state.Geometry.Cells) Require(state.Areas.at(cell.first).IsWater == (cell.second.Height < 20), "Cell type mismatch");
            std::vector<Id> route{99}; Require(!FindRoute(state,1,3,route) && !FindRoute(state,3,3,route), "Water walking accepted");
            Require(!world.Travel("water-step",1,3) && !world.DevelopArea("water-road",1,3,1), "Water development/travel accepted");
            Accepted(world.AdvanceDays(30)); const auto& water = world.State().Areas.at(3);
            Require(water.Cohorts.empty() && water.Influences.empty() && !water.RegionId && water.Prosperity == 0 && water.Unrest == 0, "Water gained land behavior");
            const auto metrics = world.Inspect(); Require(metrics.Areas == 4 && metrics.LandAreas == 3 && metrics.WaterAreas == 1, "Area metrics confused water with land");
            const auto empire = InspectEmpires(world.State()); Require(empire.ClaimedAreas + empire.UnclaimedAreas + empire.ContestedAreas == 3, "Ocean counted as outskirts");
            auto bad = state; bad.Areas.erase(3); Require(!Validate(bad), "Missing water polygon accepted");
            bad = state; bad.Areas.at(3).Cohorts = state.Areas.at(1).Cohorts; Require(!Validate(bad), "Water population accepted");
            bad = state; bad.Areas.at(3).RegionId = 1; Require(!Validate(bad), "Water ownership accepted");
            bad = state; bad.Characters.at(1).AreaId = 3; Require(!Validate(bad), "Named character at sea accepted");
            bad = state; bad.Areas.at(1).Neighbors.push_back(3); Require(!Validate(bad), "Water walking link accepted");
        } },
        { "polygon generation and simulation ignore placeholder grid coordinates", [] {
            auto document = PolygonFixture(); EmpireGeneration options; options.EmpireCount = 1; options.RegionsPerEmpire = 1;
            options.Seeds = {{1,Deity::Harmony}}; Snapshot first,second;
            Accepted(GenerateImportedWorld(document,options,{},first));
            for (auto& area : document.Areas) { area.X = 0; area.Y = 0; }
            Accepted(GenerateImportedWorld(document,options,{},second));
            World a,b; Accepted(a.Restore(first)); Accepted(b.Restore(second)); Accepted(a.AdvanceDays(20)); Accepted(b.AdvanceDays(20));
            for (auto& area : second.Areas) { area.second.X = first.Areas.at(area.first).X; area.second.Y = first.Areas.at(area.first).Y; }
            Require(Serialized(first) == Serialized(second), "Geometry generation depends on a grid");
            auto normalized = b.State(); for (auto& area : normalized.Areas) { area.second.X = a.State().Areas.at(area.first).X; area.second.Y = a.State().Areas.at(area.first).Y; }
            Require(Serialized(a.State()) == Serialized(normalized), "Geometry simulation depends on a grid");
        } },
        { "hierarchy aggregates cell membership population and occupations", [] {
            const auto state = Fixture(); const auto hierarchy = InspectHierarchy(state);
            std::set<Id> owned;
            for (const auto& domain : hierarchy.Domains)
            {
                std::vector<Id> memberAreas; std::int64_t population = 0; std::array<std::int64_t,PopulationRoleCount> roles{};
                for (const auto region : domain.second.RegionIds)
                {
                    const auto& summary = hierarchy.Regions.at(region);
                    Require(state.Regions.at(region).DomainId == domain.first, "Wrong domain membership");
                    memberAreas.insert(memberAreas.end(),summary.AreaIds.begin(),summary.AreaIds.end()); population += summary.Population;
                    for (std::size_t n = 0; n < PopulationRoleCount; ++n) roles[n] += summary.Roles[n];
                }
                std::sort(memberAreas.begin(),memberAreas.end());
                Require(memberAreas == domain.second.AreaIds && population == domain.second.Population && roles == domain.second.Roles, "Hierarchy totals diverged");
                for (const auto area : memberAreas) Require(owned.insert(area).second && state.Regions.at(state.Areas.at(area).RegionId).DomainId == domain.first, "Area double-owned");
            }
            Require(owned.size() == InspectEmpires(state).ClaimedAreas, "Hierarchy lost claimed cells");
            const auto imported = InspectHierarchy(ImportedFixture()); Require(imported.Domains.at(1).AreaIds == std::vector<Id>({1,2}), "Water/island added to empire");
        } },
        { "version four polygon saves gain water without changing existing records", [] {
            const std::string old = R"v4fixture(BIRTHRIGHT_WORLD 4 "azgaar-fixture-42" 42 0
1 100 50 1 0.80000000000000004 0.29999999999999999 0.20000000000000001 1 6570 0.0001 8.0000000000000007e-05 1
2
1 "Empire 1 tradition" 3
2 "Frontier communities" 1
1
1 "Empire 1" 1
1
1 "House 1" 2
1
1 1 1
3
1 1 1 0 7920 0.77086260416666663 0.050000000000000003
1 2
2
1 5455 0 0
2 115 0 0
2 1 2 0 7920 0.80000000000000004 0.050000000000000003
1 1
2
1 5740 0 0
2 115 0 0
4 0 4 0 7920 0.20000000000000001 0.050000000000000003
0
1
2 5 0 0
2
1 "Emperor 1" 2 1 -9159 1 0 0
0
0 3250 0 0 0 0 0
0 0 0 0 0 0 0
0 0 0 0 0 0 0
0 11500 0 0 0 0 0
0 0 0 0 0 0 0
0 0 0 0 0 0 0
0 0 0 0 0 0 0
2 "Steward 1" 2 1 -8884 1 0 0
0
0 3250 0 0 0 0 0
0 0 0 0 0 0 0
0 0 0 0 0 0 0
0 11500 0 0 0 0 0
0 0 0 0 0 0 0
0 0 0 0 0 0 0
0 0 0 0 0 0 0
0
0
EMPIRE_METADATA 1
2
1 0 0.69999999999999996 1 0.13300000000000001 0.59999999999999998 0.085999999999999993 0.18099999999999999
2 7 1 0 0.080000000000000002 0.59999999999999998 0.22 0.10000000000000001
1
1 2 3 0 0.69999999999999996
3
1 0 0.90000000000000002 1 0 1 1 0.9514376736111112
2 0 0.90000000000000002 1 0 1 1 1
4 1 0.90000000000000002 1 0 0
TRAVERSAL_METADATA 1
1 0.20000000000000001 0.002 0.65000000000000002 0.02
1
1 30
1
1 2 0.34999999999999998 1
3
1 0 10000 1 1 0.9514376736111112
2 1 0 2 0
2 0 10000 1 1 1
2 1 0 2 0
4 0 10000 0
1 2 0
MAP_GEOMETRY 1 "Archipelago" "1.154.0" "fixture" "mi" 4 1 1 4
1 0.5 0.5 30 4 4 0 0 1 0 1 1 0 1
2 1.5 0.5 30 4 4 1 0 2 0 2 1 1 1
3 2.5 0.5 10 4 4 2 0 3 0 3 1 2 1
4 3.5 0.5 30 4 4 3 0 4 0 4 1 3 1
)v4fixture";
            Snapshot state; std::istringstream input(old); Accepted(Load(input,state));
            Require(state.Version == 11 && state.Areas.size() == 4 && state.Areas.at(3).IsWater && state.Characters.size() == 2, "v4 map migration lost records");
            Near(state.Connections.at({1,2}).RoadQuality,0.35);
            World world; Accepted(world.Restore(state)); Accepted(world.Marry("v4-pair",1,2)); Accepted(world.Birth("v4-heir",99,"Heir",1,2,Deity::Life,Deity::Harmony));
            auto encoded = Serialized(world.State()); Snapshot restored; std::istringstream roundtrip(encoded); Accepted(Load(roundtrip,restored));
            Require(Serialized(restored) == encoded && restored.Characters.at(99).Parents.size() == 2 && restored.Events.size() == 2, "Migration damaged families/receipts");
            const auto header = encoded.find("AREA_TYPES 1"); encoded.replace(header,12,"AREA_TYPES 9");
            std::istringstream bad(encoded); Require(!Load(bad,restored) && Serialized(restored) == Serialized(world.State()), "Unknown area types loaded partially");
        } },
        { "travel uses influence effort for terrain development roads and cell distance", [] {
            auto state = Fixture(); Near(TravelDurationDays(state,1,2),1);
            state.Areas.at(1).Development = 1; state.Areas.at(2).Development = 1;
            Near(TravelDurationDays(state,1,2),0.25); Near(TravelDurationDays(state,1,31),0.625);
            state.Connections[{1,2}] = {1,2,1,1}; Near(TravelDurationDays(state,1,2),0.0625);
            state.Connections.at({1,2}).DistanceScale = 2; Near(TravelDurationDays(state,1,2),0.125);
            Near(TravelDurationDays(state,1,2),TraversalCost(state,1,2));
            state = Fixture(); state.Areas.at(1).DiffusionCost = 3; state.Areas.at(2).DiffusionCost = 3;
            Near(TravelDurationDays(state,1,2),3); state.Areas.at(2).DiffusionCost = 30;
            Near(TravelDurationDays(state,1,2),16.5);
            state.TravelPolicy.DaysPerEffort = 0.5; Near(TravelDurationDays(state,1,2),8.25);
            Require(std::isinf(TravelDurationDays(ImportedFixture(),1,3)), "Water has walking duration");
        } },
        { "fastest routes choose road detours and avoid mountain shortcuts deterministically", [] {
            auto state = Fixture(); std::vector<Id> route;
            Accepted(FindRoute(state,1,3,route)); Require(route == std::vector<Id>({2,3}), "Plain shortest route changed");
            for (const auto link : std::vector<std::pair<Id,Id>>{{1,31},{31,32},{32,33},{3,33}})
                state.Connections[link] = {link.first,link.second,1,1};
            Accepted(FindRoute(state,1,3,route)); Require(route == std::vector<Id>({31,32,33,3}), "Road detour ignored");
            Near(RouteDurationDays(state,1,route),1);
            for (auto& area : state.Areas) std::reverse(area.second.Neighbors.begin(),area.second.Neighbors.end());
            std::vector<Id> repeat; Accepted(FindRoute(state,1,3,repeat)); Require(repeat == route, "Neighbor order changed route");
            state = Fixture(); state.Areas.at(2).DiffusionCost = 30;
            Accepted(FindRoute(state,1,3,route)); Require(route == std::vector<Id>({31,32,33,3}), "Mountain shortcut chosen");
            Near(RouteDurationDays(state,1,route),4);
        } },
        { "fast crossings accumulate fractional time and daily society ticks once at the boundary", [] {
            auto state = Fixture(); state.Connections[{1,2}] = {1,2,1,1}; World world,daily;
            Accepted(world.Restore(state)); Accepted(daily.Restore(state)); const auto favor = state.Characters.at(1).Affinities;
            for (int n = 0; n < 3; ++n) Accepted(world.Travel("quarter-"+std::to_string(n),1,n%2 ? 1 : 2));
            Require(world.State().Day == 0 && world.State().Characters.at(1).Affinities[0].EarnedFavor == favor[0].EarnedFavor, "Daily activity ran early");
            Near(world.State().DayFraction,0.75);
            for (const auto& area : state.Areas) Require(AreaPopulation(world.State(),area.first) == AreaPopulation(state,area.first), "Population ticked per crossing");
            Accepted(world.Travel("quarter-3",1,1)); Accepted(daily.AdvanceDays(1));
            Require(world.State().Day == 1 && world.State().DayFraction == 0, "Four fast crossings lost/added time");
            for (const auto& area : state.Areas) Require(AreaPopulation(world.State(),area.first) == AreaPopulation(daily.State(),area.first), "Daily population diverged");
            for (std::size_t n = 0; n < DeityCount; ++n) Near(world.State().Characters.at(1).Affinities[n].EarnedFavor,daily.State().Characters.at(1).Affinities[n].EarnedFavor);
            state.TravelPolicy.DaysPerEffort = 0.4; Accepted(world.Restore(state));
            for (int n = 0; n < 10; ++n) Accepted(world.Travel("tenth-"+std::to_string(n),1,n%2 ? 1 : 2));
            Require(world.State().Day == 1 && world.State().DayFraction == 0, "Floating point lost daily boundary");
        } },
        { "travel remainder resume and changing development retain authoritative time and receipts", [] {
            auto state = ConnectivityFixture(); state.Connections[{1,2}] = {1,2,1,1};
            World uninterrupted,resumed; Accepted(uninterrupted.Restore(state)); Accepted(uninterrupted.Travel("first-quarter",1,2));
            Near(uninterrupted.State().DayFraction,0.25);
            Snapshot loaded; std::istringstream input(Serialized(uninterrupted.State())); Accepted(Load(input,loaded)); Accepted(resumed.Restore(loaded));
            Require(!resumed.Travel("first-quarter",1,1), "Fractional travel receipt lost");
            for (auto* world : {&uninterrupted,&resumed})
            {
                Accepted(world->DevelopArea("develop-one",1,1,1)); Accepted(world->DevelopArea("develop-two",1,2,1));
                Near(TravelDurationDays(world->State(),2,1),0.0625); Accepted(world->Travel("developed-crossing",1,1));
                Near(world->State().DayFraction,0.3125);
                Accepted(world->DevelopConnection("remove-road",1,{1,2,0,1}));
                Near(TravelDurationDays(world->State(),1,2),0.25); Accepted(world->Travel("without-road",1,2));
                Accepted(world->AdvanceDays(2));
            }
            Require(Serialized(uninterrupted.State()) == Serialized(resumed.State()), "Fractional resume/development diverged");
            Near(resumed.State().DayFraction,0.5625); Require(resumed.State().Day == 2, "Integer advancement discarded fractional time");
            Require(resumed.State().Events.back().Detail.find("days=0.25") != std::string::npos, "Receipt missing weighted cost");
        } },
        { "slow travel advances daily systems and excessive clock costs reject atomically", [] {
            auto state = Fixture(); state.Areas.at(1).DiffusionCost = 3; state.Areas.at(2).DiffusionCost = 3;
            World world,daily; Accepted(world.Restore(state)); Accepted(daily.Restore(state));
            Accepted(world.Travel("forest",1,2)); Accepted(daily.AdvanceDays(3)); Require(world.State().Day == 3, "Forest time ignored");
            for (const auto& area : state.Areas) Require(AreaPopulation(world.State(),area.first) == AreaPopulation(daily.State(),area.first), "Slow travel skipped simulation");
            state.Day = 100000000; state.DayFraction = 0; Accepted(world.Restore(state)); auto before = Serialized(world.State());
            Require(!world.Travel("out-of-time",1,2) && Serialized(world.State()) == before, "Clock overflow partially applied");
            state = Fixture(); state.Areas.at(1).DiffusionCost = 1000; state.Areas.at(2).DiffusionCost = 1000;
            state.Connections[{1,2}] = {1,2,0,1000}; Accepted(world.Restore(state)); before = Serialized(world.State());
            Require(!world.Travel("huge-link",1,2) && Serialized(world.State()) == before, "Excessive work accepted");
            state = Fixture(); state.DayFraction = 1; Require(!Validate(state), "Invalid fraction accepted");
            state = Fixture(); state.TravelPolicy.DaysPerEffort = std::numeric_limits<double>::quiet_NaN(); Require(!Validate(state), "NaN travel policy accepted");
        } },
        { "version five migration preserves old campaign and supplies neutral travel metadata", [] {
            const auto state = ImportedFixture(); auto encoded = Serialized(state); encoded.resize(encoded.find("TRAVEL_METADATA"));
            encoded.replace(encoded.find("BIRTHRIGHT_WORLD 11"),std::string("BIRTHRIGHT_WORLD 11").size(),"BIRTHRIGHT_WORLD 5");
            Snapshot loaded; std::istringstream old(encoded); Accepted(Load(old,loaded)); Require(Serialized(loaded) == Serialized(state), "v5 migration changed prior records");
            auto malformed = Serialized(state); const auto pos = malformed.find("TRAVEL_METADATA 1"); malformed.replace(pos,17,"TRAVEL_METADATA 9");
            std::istringstream bad(malformed); const auto before = Serialized(loaded); Require(!Load(bad,loaded) && Serialized(loaded) == before, "Unknown travel policy replaced state");
            malformed = Serialized(state); malformed.resize(malformed.find("TRAVEL_METADATA")); std::istringstream missing(malformed);
            Require(!Load(missing,loaded) && Serialized(loaded) == before, "Missing travel metadata accepted");
        } },
        { "weighted command levels strongest full others thirty percent with minimum ten", [] {
            auto s=Fixture(); auto& p=s.Characters.at(1); for (auto& a:p.Affinities) a={};
            Require(TroopCapacity(s,1)==10,"Minimum capacity lost");
            p.Affinities[0].EarnedFavor=FavorCost(4,s.Policy); p.Affinities[1].EarnedFavor=FavorCost(3,s.Policy);
            p.Affinities[2].EarnedFavor=FavorCost(2,s.Policy); Near(CommandLevels(s,1),5.5);
            Require(TroopCapacity(s,1)==27,"Weighted final floor wrong");
        } },
        { "recruitment is atomic bounded and replay safe and follows leader through travel", [] {
            auto s=Fixture(); auto& p=s.Characters.at(1); p.CultureId=s.Areas.at(p.AreaId).Cohorts.front().CultureId;
            for (auto& a:p.Affinities) a={}; World w; Accepted(w.Restore(s));
            const auto offers=LocalRecruitment(s,1); const auto culture=p.CultureId;
            const auto offer=std::find_if(offers.begin(),offers.end(),[&](const auto& o){return o.CultureId==culture;});
            Require(offer!=offers.end() && offer->Available>=10,"Fixture needs local volunteers");
            const auto initial=Serialized(w.State());
            Require(!w.RecruitPeasants("zero",1,p.AreaId,culture,0) && !w.RecruitPeasants("stale",1,p.AreaId+1,culture,1) && !w.RecruitPeasants("foreign",1,p.AreaId,999,1) && Serialized(w.State())==initial,"Rejected command mutated state");
            Accepted(w.RecruitPeasants("recruit",1,p.AreaId,culture,10)); Require(TroopCount(w.State(),1)==10,"Wrong count");
            const auto full=Serialized(w.State()); Require(!w.RecruitPeasants("recruit",1,p.AreaId,culture,1) && !w.RecruitPeasants("over",1,p.AreaId,culture,1) && Serialized(w.State())==full,"Replay or cap overrun");
            Accepted(w.Travel("band-travel",1,s.Areas.at(p.AreaId).Neighbors.front()));
            Require(TroopCount(w.State(),1)==10 && w.State().Troops.at(1).HomeAreaId==p.AreaId,"Band lost during travel");
            std::istringstream encoded(Serialized(w.State())); Snapshot loaded; Accepted(Load(encoded,loaded)); Require(Serialized(loaded)==Serialized(w.State()),"Roster save lost records");
            Require(!w.DismissTroop("not-yours",2,1),"Foreign dismissal allowed"); Accepted(w.DismissTroop("release",1,1)); Require(TroopCount(w.State(),1)==9,"Dismissal failed");
        } },
        { "local culture and lineage approval constrain finite regional recruitment", [] {
            auto s=Fixture(); auto& p=s.Characters.at(1); const auto local=s.Areas.at(p.AreaId).Cohorts.front().CultureId;
            p.CultureId=local; for (auto& a:p.Affinities) a={}; p.Affinities[0].EarnedFavor=FavorCost(100,s.Policy);
            const auto offers=LocalRecruitment(s,1); const auto willing=std::find_if(offers.begin(),offers.end(),[&](const auto& o){return o.CultureId==local;})->Available;
            World w; Accepted(w.Restore(s)); Accepted(w.RecruitPeasants("pool",1,p.AreaId,local,willing));
            Require(!w.RecruitPeasants("exhausted",1,p.AreaId,local,1),"Finite pool bypassed");
            const auto& t=w.State().Troops.begin()->second; Accepted(w.DismissTroop("dismiss",1,t.Key));
            Require(!w.RecruitPeasants("refill",1,p.AreaId,local,1),"Dismissal refilled pool");
            p.CultureId=local%DeityCount+1; for (auto& a:p.Affinities) a={};
            auto foreign=LocalRecruitment(s,1); Require(std::find_if(foreign.begin(),foreign.end(),[&](const auto& o){return o.CultureId==local;})->Available==0,"Untrusted foreign founder accepted");
            auto& ancestor=s.Characters.at(2); ancestor.CultureId=local; ancestor.BornDay=p.BornDay-1; p.Parents={2};
            foreign=LocalRecruitment(s,1); Require(std::find_if(foreign.begin(),foreign.end(),[&](const auto& o){return o.CultureId==local;})->Available>0,"Local ancestry not respected");
            p.Parents.clear(); p.Affinities[static_cast<std::size_t>(s.Cultures.at(local).Patron)].BirthrightPower=1;
            foreign=LocalRecruitment(s,1); Require(std::find_if(foreign.begin(),foreign.end(),[&](const auto& o){return o.CultureId==local;})->Available>0,"Inherited patron not respected");
            p.Alive=false; Require(LocalRecruitment(s,1).empty(),"Dead leader has volunteers");
        } },
        { "version six migration starts with no warband and corrupt troop records reject atomically", [] {
            const auto s=Fixture(); auto old=Serialized(s); old.resize(old.find("WARBAND_METADATA")); old.replace(0,std::string("BIRTHRIGHT_WORLD 11").size(),"BIRTHRIGHT_WORLD 6");
            Snapshot loaded; std::istringstream input(old); Accepted(Load(input,loaded)); Require(loaded.Version==11 && loaded.Troops.empty() && loaded.NextTroopId==1,"v6 roster migration wrong");
            auto bad=s; bad.Troops.emplace(1,Troop{1,1,1,1,TroopKind::Peasant}); Require(!Validate(bad),"Reused troop sequence accepted");
            auto truncated=Serialized(s); truncated.resize(truncated.find("WARBAND_METADATA")); std::istringstream missing(truncated);
            const auto before=Serialized(loaded); Require(!Load(missing,loaded) && Serialized(loaded)==before,"Truncated v7 replaced output");
        } },
        { "battle casualties preserve survivors and save permanent deaths without population refunds", [] {
            auto s=Fixture(); s.NextTroopId=5;
            for (Id id=1;id<=4;++id) { const Id leader=id==4 ? 2 : 1; const auto& p=s.Characters.at(leader); s.Troops.emplace(id,Troop{id,leader,p.CultureId,p.AreaId,TroopKind::Peasant}); }
            World w; Accepted(w.Restore(s));
            WarbandBattleInput input{1,s.WorldId,"encounter-a",1,s.Characters.at(1).AreaId,{1,2,3}};
            WarbandBattleResult result{1,"encounter-a",{1,3}};
            Accepted(w.RecordTroopBattleCasualties("battle-a",input,result));
            Require(w.State().Troops.count(2) && w.State().Troops.count(4) && !w.State().Troops.count(1) && !w.State().Troops.count(3),"Wrong survivors");
            Require(w.State().NextTroopId==5 && w.State().RecruitmentUses.size()==s.RecruitmentUses.size(),"Death reset IDs or recruitment depletion");
            for (const auto& a:s.Areas) Require(AreaPopulation(w.State(),a.first)==AreaPopulation(s,a.first),"Death refunded a civilian");
            const auto saved=Serialized(w.State()); Snapshot loaded; std::istringstream stream(saved); Accepted(Load(stream,loaded));
            Require(Serialized(loaded)==saved && loaded.Troops.count(2) && !loaded.Troops.count(1),"Save revived a casualty");
        } },
        { "battle casualty receipts reject replay and already dead IDs atomically", [] {
            auto s=Fixture(); s.NextTroopId=3; const auto& p=s.Characters.at(1);
            for (Id id=1;id<=2;++id) s.Troops.emplace(id,Troop{id,1,p.CultureId,p.AreaId,TroopKind::Peasant});
            World w; Accepted(w.Restore(s)); WarbandBattleInput input{1,s.WorldId,"battle",1,p.AreaId,{1,2}}; WarbandBattleResult result{1,"battle",{1}};
            Accepted(w.RecordTroopBattleCasualties("death-batch",input,result)); const auto before=Serialized(w.State());
            Require(!w.RecordTroopBattleCasualties("death-batch",input,result),"Replay accepted");
            Require(!w.RecordTroopBattleCasualties("new-operation",input,result),"Old roster accepted");
            input.Troops={2}; Require(!w.RecordTroopBattleCasualties("already-dead",input,result),"Dead ID killed again");
            Require(before==Serialized(w.State()),"Rejected casualty report mutated campaign");
            result.Casualties={2}; Accepted(w.RecordTroopBattleCasualties("second-death",input,result)); Require(TroopCount(w.State(),1)==0,"Incremental death failed");
        } },
        { "battle casualties validate versions world encounter area roster ownership and unique deployed IDs", [] {
            auto s=Fixture(); s.NextTroopId=5;
            for (Id id=1;id<=4;++id) { const Id leader=id==4 ? 2 : 1; const auto& p=s.Characters.at(leader); s.Troops.emplace(id,Troop{id,leader,p.CultureId,p.AreaId,TroopKind::Peasant}); }
            World w; Accepted(w.Restore(s)); const auto before=Serialized(w.State());
            const WarbandBattleInput good{1,s.WorldId,"battle",1,s.Characters.at(1).AreaId,{1,2,3}}; const WarbandBattleResult deaths{1,"battle",{1}};
            for (int i=0;i<12;++i) {
                auto input=good; auto result=deaths;
                switch(i) { case 0:input.Version=2;break;case 1:result.Version=2;break;case 2:input.WorldId="other";break;case 3:result.EncounterId="other";break;
                case 4:input.AreaId=999999;break;case 5:input.LeaderId=999999;break;case 6:input.Troops={1,1,3};break;case 7:input.Troops={1,2};break;
                case 8:input.Troops={1,2,4};break;case 9:result.Casualties={1,1};break;case 10:result.Casualties={4};break;case 11:result.Casualties={};break; }
                Require(!w.RecordTroopBattleCasualties("invalid-"+std::to_string(i),input,result),"Invalid casualty accepted case="+std::to_string(i));
                Require(before==Serialized(w.State()),"Invalid result partially applied");
            }
        } },
        { "a troop death can reconcile after its leader dies", [] {
            auto s=Fixture(); s.NextTroopId=2; const auto& p=s.Characters.at(1); s.Troops.emplace(1,Troop{1,1,p.CultureId,p.AreaId,TroopKind::Peasant});
            World w; Accepted(w.Restore(s)); Accepted(w.RecordDeath("leader-death",1,"test"));
            Accepted(w.RecordTroopBattleCasualties("final-casualty",{1,s.WorldId,"battle",1,p.AreaId,{1}},{1,"battle",{1}}));
            Require(!w.State().Troops.count(1),"Dead leader's casualty resurrected");
        } },
        { "JSON export includes pending grants and escapes text", [] {
            const auto w = Family(); std::ostringstream out; Accepted(ExportJson(w.State(), out));
            Require(out.str().find("Heir \\\"First\\\"") != std::string::npos && out.str().find("fullBirthrightPower") != std::string::npos, "JSON fields or escaping missing");
        } }
    };
    cases.push_back({"BR-068 alignment validates mixtures and rejects atomically", [] {
        using namespace CultureScaffold;
        DivineProfile profile{}, weights{};
        weights[static_cast<std::size_t>(Deity::Order)] = 9;
        weights[static_cast<std::size_t>(Deity::Life)] = 1;
        Accepted(NormalizeAlignment(weights, profile));
        Near(profile[static_cast<std::size_t>(Deity::Order)], 0.9);
        Require(IsProfile(profile), "BR-068 mixture not normalized");
        const auto before = profile;
        for (double invalid : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            weights[0] = invalid;
            Require(!NormalizeAlignment(weights, profile) && profile == before, "BR-068 invalid input mutated output");
        }
        weights = {};
        Require(!NormalizeAlignment(weights, profile) && profile == before, "BR-068 empty alignment accepted");
        weights.fill(std::numeric_limits<double>::max());
        Accepted(NormalizeAlignment(weights, profile));
        Require(IsProfile(profile), "BR-068 large weights overflowed");
    }});
    cases.push_back({"BR-068 distance diagnoses profiles with neutral unevaluated effects", [] {
        using namespace CultureScaffold;
        DivineProfile first{}, second{};
        first[static_cast<std::size_t>(Deity::Order)] = 1;
        second[static_cast<std::size_t>(Deity::Chaos)] = 1;
        Near(*DivineDistance(first, first), 0);
        Near(*DivineDistance(first, second), 1);
        Near(*DivineDistance(second, first), 1);
        Require(!DivineDistance(first, DivineProfile{}), "BR-068 unset profile treated as agreement");
        RegionalEffectQuote regional; DiplomacyEffectQuote diplomacy; TuningProposal tuning;
        Require(!regional.Evaluated && regional.YieldMultiplier == 1 && regional.RequestedSuppressionPerDay == 0 &&
            !diplomacy.Evaluated && diplomacy.AttitudeAdjustment == 0 && !tuning.SignificantMismatch,
            "BR-068 scaffold supplied active effects or balance");
    }});
    cases.push_back({"BR013 origins trees inventory migration", [] { std::string failure; const bool passed=Birthright::Campaign::Testing::CheckPlayerProgression(failure); Require(passed, failure); }});
    Birthright::Campaign::Testing::AddSimulationTests(cases);
    std::size_t failures = 0, selected = 0;
    for (const auto& test : cases)
    {
        if (!filter.empty() && test.first.find(filter) == std::string::npos) continue;
        ++selected;
        try { test.second(); std::cout << "PASS " << test.first << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << test.first << ": " << error.what() << '\n'; }
    }
    std::cout << selected - failures << '/' << selected << " campaign tests passed\n";
    return failures || !selected ? 1 : 0;
}
bool Number(const char* text, std::uint32_t& number)
{
    const std::string value(text); const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}
bool Real(const char* text, double& number)
{
    const std::string value(text); const auto parsed = std::from_chars(value.data(),value.data()+value.size(),number);
    return parsed.ec == std::errc{} && parsed.ptr == value.data()+value.size() && std::isfinite(number);
}
std::string JsonText(const std::string& text)
{
    std::ostringstream out; out << '"';
    for (const unsigned char c : text)
    {
        if (c == '"' || c == '\\') out << '\\' << static_cast<char>(c);
        else if (c < 32) out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
        else out << static_cast<char>(c);
    }
    out << '"'; return out.str();
}
int Fail(const Result& result)
{
    std::cerr << "{\"status\":\"rejected\",\"code\":" << JsonText(result.Code) << ",\"detail\":" << JsonText(result.Detail) << "}\n"; return 1;
}
int WriteWorld(const World& world, const std::string& prefix)
{
    const std::filesystem::path path(prefix);
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    // CLI replaces only the explicitly selected outputs. Runtime atomic-save adapter remains future work.
    std::ofstream save(prefix + ".world"); auto result = Save(world.State(), save); if (!result) return Fail(result);
    save.close(); if (!save) return Fail({ false, "save_close_failed", "Could not finish save output" });
    std::ofstream json(prefix + ".json"); result = ExportJson(world.State(), json); if (!result) return Fail(result);
    json.close(); if (!json) return Fail({ false, "json_close_failed", "Could not finish JSON output" });
    if (world.State().Domains.begin()->second.CapitalAreaId)
    {
        std::ofstream svg(prefix + ".svg"); result = ExportEmpireSvg(world.State(), svg); if (!result) return Fail(result);
        svg.close(); if (!svg) return Fail({ false, "svg_close_failed", "Could not finish SVG overview" });
    }
    const auto m = world.Inspect();
    const auto empire = InspectEmpires(world.State());
    std::cout << "{\"status\":\"accepted\",\"day\":" << m.Day << ",\"dayFraction\":" << m.DayFraction << ",\"areas\":" << m.Areas
        << ",\"landAreas\":" << m.LandAreas << ",\"waterAreas\":" << m.WaterAreas << ",\"population\":" << m.Population
        << ",\"namedCharacters\":" << m.NamedCharacters << ",\"operations\":" << m.Operations << ",\"meanUnrest\":" << m.MeanUnrest
        << ",\"claimedAreas\":" << empire.ClaimedAreas << ",\"contestedAreas\":" << empire.ContestedAreas
        << ",\"unclaimedAreas\":" << empire.UnclaimedAreas << ",\"wildernessAreas\":" << empire.WildernessAreas << "}\n";
    return 0;
}
}

int main(int argc, char** argv)
{
    try
    {
        if ((argc == 2 || argc == 3) && std::string(argv[1]) == "--test") return Tests(argc == 3 ? argv[2] : "");
        if (argc > 1 && std::string(argv[1]) == "--simulate") return Birthright::Campaign::Testing::RunSimulationCli(argc, argv);
        Snapshot state; Result result; std::uint32_t days = 0;
        const bool familyDemo = argc == 5 && std::string(argv[1]) == "--family-demo";
        const bool connectivityDemo = argc == 5 && std::string(argv[1]) == "--connectivity-demo";
        if ((argc == 8 || argc == 10) && std::string(argv[1]) == "--import")
        {
            EmpireGeneration options;
            options.ClaimRadius = 20; options.InfluenceRadius = 30;
            if (!Number(argv[4],options.Seed) || !Number(argv[5],days) || !Number(argv[6],options.EmpireCount) || !Number(argv[7],options.RegionsPerEmpire))
                return Fail({false, "arguments", "Seed, days, N and K must be unsigned integers"});
            if (argc == 10 && (!Real(argv[8],options.ClaimRadius) || !Real(argv[9],options.InfluenceRadius)))
                return Fail({false, "arguments", "Claim/influence radii must be finite numbers"});
            GeographyDocument document; std::ifstream source(argv[2]);
            result = LoadGeography(source, document); if (!result) return Fail(result);
            options.WorldId = "azgaar-" + document.Geometry.SourceSeed + "-" + std::to_string(options.Seed);
            result = GenerateImportedWorld(document,options,{},state); if (!result) return Fail(result);
        }
        else if (connectivityDemo)
        {
            std::uint32_t seed = 0;
            if (!Number(argv[3], seed) || !Number(argv[4], days)) return Fail({false, "arguments", "Seed and days must be unsigned integers"});
            std::vector<GeographicArea> graph; result = MakeGridGeography(30, 20, seed, graph); if (!result) return Fail(result);
            for (auto& area : graph)
            {
                area.SettlementSuitability = 1; area.PopulationCapacity = 10000;
                if (area.X >= 4 && area.X <= 8) area.DiffusionCost = 3;
                if (area.X == 12) area.DiffusionCost = 30;
            }
            EmpireGeneration options; options.Seed = seed; options.EmpireCount = 1; options.RegionsPerEmpire = 4;
            options.ClaimRadius = 10000; options.InfluenceRadius = 30; options.Seeds = {{301, Deity::Harmony}};
            Rules policy; policy.DailyBirthRate = 0; policy.DailyDeathRate = 0; policy.DailyStewardFavor = 0;
            result = GenerateEmpires(graph, options, policy, state); if (!result) return Fail(result);
            for (auto& area : state.Areas) area.second.FoodCapacity = 1000000;
        }
        else if (argc == 7 && std::string(argv[1]) == "--grid-empires")
        {
            EmpireGeneration options;
            if (!Number(argv[3], options.Seed) || !Number(argv[4], days) || !Number(argv[5], options.EmpireCount) ||
                !Number(argv[6], options.RegionsPerEmpire)) return Fail({ false, "arguments", "Seed, days, N and K must be unsigned integers" });
            std::vector<GeographicArea> geography;
            result = MakeGridGeography(30, 20, options.Seed, geography); if (!result) return Fail(result);
            result = GenerateEmpires(geography, options, {}, state); if (!result) return Fail(result);
        }
        else if (argc == 5 && (std::string(argv[1]) == "--grid-generate" || familyDemo))
        {
            Generation options;
            if (!Number(argv[3], options.Seed) || !Number(argv[4], days)) return Fail({ false, "arguments", "Seed and days must be unsigned integers" });
            result = Generate(options, {}, state); if (!result) return Fail(result);
            if (familyDemo) state.Characters.at(2).AreaId = state.Characters.at(1).AreaId;
        }
        else if (argc == 5 && std::string(argv[1]) == "--resume")
        {
            if (!Number(argv[4], days)) return Fail({ false, "arguments", "Days must be an unsigned integer" });
            std::ifstream input(argv[2]); result = Load(input, state); if (!result) return Fail(result);
        }
        else
        {
            std::cerr << "WorldTool --test\nWorldTool --grid-generate OUTPUT_PREFIX SEED DAYS\nWorldTool --grid-empires OUTPUT_PREFIX SEED DAYS N K\nWorldTool --connectivity-demo OUTPUT_PREFIX SEED DAYS\nWorldTool --family-demo OUTPUT_PREFIX SEED DAYS\nWorldTool --resume INPUT.world OUTPUT_PREFIX DAYS\n"; return 2;
        }
        World world; result = world.Restore(state); if (!result) return Fail(result);
        if (connectivityDemo)
        {
            if (WriteWorld(world, std::string(argv[2]) + "-before")) return 1;
            for (Id first = 301; first < 325; ++first)
            {
                result = world.DevelopConnection("demo-road-" + std::to_string(first), 1, {first,first+1,1,1});
                if (!result) return Fail(result);
            }
            result = world.DevelopArea("demo-pass", 1, 313, 0.5); if (!result) return Fail(result);
            if (WriteWorld(world, std::string(argv[2]) + "-scheduled")) return 1;
        }
        if (familyDemo)
        {
            const auto check = [](const Result& command) { if (!command) throw std::runtime_error(command.Code + ": " + command.Detail); };
            check(world.Marry("demo-marriage", 1, 2));
            check(world.Birth("demo-first", 1000, "First heir", 1, 2, Deity::Harmony, Deity::Death));
            check(world.Birth("demo-second", 1001, "Second heir", 1, 2, Deity::Harmony, Deity::Death));
            check(world.Birth("demo-third", 1002, "Third heir", 1, 2, Deity::Harmony, Deity::Death));
            check(world.SplitBirthright("demo-split", 1, Deity::Harmony, {{1000, 0.5}, {1002, 0.5}}));
            check(world.ReleaseBirthright("demo-ceremony-first", 1, 1000));
            check(world.ReleaseBirthright("demo-ceremony-third", 1, 1002));
            check(world.AwardFavor("demo-strife-award-one", 1, Deity::Strife, FavorCost(50, state.Policy) - FavorCost(Power(world.State().Characters.at(1).Affinities[0], state.Policy), state.Policy), "Scripted duel preparation"));
            check(world.AwardFavor("demo-strife-award-two", 2, Deity::Strife, FavorCost(50, state.Policy) - FavorCost(Power(world.State().Characters.at(2).Affinities[0], state.Policy), state.Policy), "Scripted duel preparation"));
            check(world.ResolveStrifeDuel("demo-duel", 1, 2));
        }
        if (days) { result = world.AdvanceDays(days); if (!result) return Fail(result); }
        return WriteWorld(world, (std::string(argv[1]) == "--resume" || std::string(argv[1]) == "--import") ? argv[3] : argv[2]);
    }
    catch (const std::exception& error) {
        if (argc >= 7 && std::string(argv[1]) == "--simulate")
            return Fail({false,"simulation_io",std::string("scenario=") + argv[6] + "; " + error.what()});
        std::cerr << "world_tool_io: " << error.what() << '\n'; return 1;
    }
}
