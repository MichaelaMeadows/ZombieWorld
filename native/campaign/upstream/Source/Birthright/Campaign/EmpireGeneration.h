#pragma once

#include "World.h"

namespace Birthright::Campaign
{
// Authoring input: pass only navigable areas/links; the PNG is never the authority.
// Cost controls diffusion/claims, migration and player travel through the shared link effort.
struct GeographicArea
{
    Id Key = 0;
    std::int32_t X = 0;
    std::int32_t Y = 0;
    std::vector<Id> Neighbors;
    double SettlementSuitability = 1;
    double DiffusionCost = 1;
    std::int64_t PopulationCapacity = 10000;
    double Development = 0;
};
struct EmpireSeed
{
    Id CapitalAreaId = 0; // Zero requests deterministic automatic placement.
    Deity PrimaryPatron = Deity::Count; // Count requests a seeded profile.
    Deity SecondaryPatron = Deity::Count;
    double PrimaryWeight = 0.7;
    double InfluenceReach = 1;
};
struct EmpireGeneration
{
    std::uint32_t Version = 1;
    std::string WorldId = "birthright-empires";
    std::uint32_t Seed = 42;
    std::uint32_t EmpireCount = 6;
    std::uint32_t RegionsPerEmpire = 6;
    double ClaimRadius = 7;
    double ContestMargin = 1;
    double InfluenceRadius = 10;
    double WildernessThreshold = 0.035;
    std::int64_t WildernessPopulation = 5;
    double SettlementFraction = 0.65;
    double LocalCultureWeight = 0.02;
    std::vector<EmpireSeed> Seeds; // Empty, or one entry per empire.
    std::vector<TraversalConnection> Connections;
    double InfluenceDailyResponse = 0.2;
    double MigrationDailyFraction = 0.002;
    bool AllowDisconnected = false; // Imported islands: no implicit sea links.
    MapGeometry Geometry;
    // Illustrative adjustable role shares: fighters, farmers, industry, other.
    std::array<Society, DeityCount> Profiles = {{
        {{{0.35, 0.32, 0.10, 0.23}}}, {{{0.04, 0.50, 0.28, 0.18}}},
        {{{0.12, 0.40, 0.12, 0.36}}}, {{{0.04, 0.72, 0.08, 0.16}}},
        {{{0.06, 0.70, 0.04, 0.20}}}, {{{0.10, 0.30, 0.45, 0.15}}},
        {{{0.20, 0.32, 0.18, 0.30}}}
    }};
};
struct TerritorySummary
{
    std::vector<Id> AreaIds;
    std::vector<Id> RegionIds; // Domain summaries only.
    std::int64_t Population = 0;
    std::array<std::int64_t, PopulationRoleCount> Roles{};
};
struct WorldHierarchy
{
    std::map<Id,TerritorySummary> Regions;
    std::map<Id,TerritorySummary> Domains;
};
// Derived from Area.RegionId and Region.DomainId; membership is never independently persisted.
WorldHierarchy InspectHierarchy(const Snapshot& state);
struct EmpireMetrics
{
    std::size_t ClaimedAreas = 0;
    std::size_t ContestedAreas = 0;
    std::size_t UnclaimedAreas = 0; // Excludes contested.
    std::size_t WildernessAreas = 0; // Independent of sovereignty.
    std::int64_t Population = 0;
    std::array<std::int64_t, PopulationRoleCount> Roles{};
};
Result MakeGridGeography(std::uint32_t width, std::uint32_t height, std::uint32_t seed,
    std::vector<GeographicArea>& output);
Result GenerateEmpires(const std::vector<GeographicArea>& geography, const EmpireGeneration& options,
    const Rules& rules, Snapshot& output);
std::array<std::int64_t, PopulationRoleCount> EstimatePopulationRoles(const Snapshot& state, Id area);
EmpireMetrics InspectEmpires(const Snapshot& state);
Result ExportEmpireSvg(const Snapshot& state, std::ostream& output);
}
