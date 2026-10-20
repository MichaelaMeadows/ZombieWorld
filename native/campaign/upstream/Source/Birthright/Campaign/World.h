#pragma once

#include <array>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>
#include <unordered_set>
#include "WorldPoliticsTypes.h"
#include "WorldMarketTypes.h"
#include "WorldBanditsTypes.h"
#include "PlayerProgressionTypes.h"

namespace Birthright::Campaign
{
using Id = std::uint64_t;
enum class Deity : std::uint32_t { Strife, Harmony, Death, Life, Nature, Order, Chaos, Count };
constexpr std::size_t DeityCount = static_cast<std::size_t>(Deity::Count);
const char* DeityName(Deity deity);

struct Rules
{
    std::uint32_t Version = 1;
    double BasePointCost = 100;
    double PointCostGrowth = 50;
    double PrimaryInheritance = 1;
    double SecondaryInheritance = 0.8;
    double OtherInheritance = 0.3;
    double ImmediateBirthrightFraction = 0.2;
    double StrifeTransferFraction = 1;
    std::int64_t AdultAgeDays = 6570;
    double DailyBirthRate = 0.0001;
    double DailyDeathRate = 0.00008;
    double DailyStewardFavor = 1;
};

// Favor is effort, power is strength. Inherited power does not masquerade as earned favor.
struct Affinity
{
    double BirthrightPower = 0;
    double EarnedFavor = 0;
    double TransferredFavor = 0;
};
double FavorCost(double power, const Rules& rules);
double PowerFromFavor(double favor, const Rules& rules);
double Power(const Affinity& affinity, const Rules& rules);

enum class PopulationRole : std::size_t { Fighters, Farmers, Industry, Other, Count };
constexpr std::size_t PopulationRoleCount = static_cast<std::size_t>(PopulationRole::Count);
struct Society
{
    // Potential occupations, not deployed armies or an inventory of manufactured goods.
    std::array<double, PopulationRoleCount> Roles{{ 0.08, 0.60, 0.22, 0.10 }};
};
struct Culture
{
    Id Key = 0;
    std::string Name;
    Deity Patron = Deity::Harmony;
    Deity SecondaryPatron = Deity::Count; // Count means none; primary then has weight 1.
    double PrimaryPatronWeight = 1;
    Id OriginDomainId = 0;
    Society Occupations;
};
struct Cohort
{
    Id CultureId = 0;
    std::int64_t Population = 0;
    double BirthRemainder = 0;
    double DeathRemainder = 0;
    double MigrationRemainder = 0;
};
// Overrides on canonical undirected links (smaller ID first); missing entries are ordinary links.
struct TraversalConnection
{
    Id FirstAreaId = 0;
    Id SecondAreaId = 0;
    double RoadQuality = 0; // 0..1; a complete road reduces link effort by 75%.
    double DistanceScale = 1; // Authored distance/crossing multiplier, not kilometers.
};
struct ConnectivityRules
{
    std::uint32_t Version = 1;
    double InfluenceDailyResponse = 0.2;
    double MigrationDailyFraction = 0.002;
    double SettlementFraction = 0.65;
    double LocalCultureWeight = 0.02;
};
struct Area
{
    Id Key = 0;
    Id RegionId = 0;
    std::int32_t X = 0;
    std::int32_t Y = 0;
    std::vector<Id> Neighbors;
    std::vector<Cohort> Cohorts;
    std::int64_t FoodCapacity = 0; // Daily population-equivalent food supply, not a stockpile.
    double Prosperity = 0.5;
    double Unrest = 0.1;
    bool IsWater = false; // A source cell area with no land population, ownership or walking links.
    bool Wilderness = false;
    double SettlementSuitability = 1;
    double DiffusionCost = 1;
    double Development = 0; // 0..1; full development reduces local terrain effort by 75%.
    std::int64_t PopulationCapacity = 10000;
    struct Influence { Id DomainId = 0; double Strength = 0; };
    std::vector<Influence> Influences; // Cultural presence, independent of recognized ownership.
    std::vector<Influence> TargetInfluences;
    std::vector<Id> Claimants; // Two or more means contested; RegionId=0 means no recognized owner.
};
struct Region { Id Key = 0; Id DomainId = 0; Id OwnerEntityId = 0; };
struct MapPoint { double X = 0; double Y = 0; };
struct MapCell
{
    Id Key = 0; // Azgaar source cell ID + 1, scoped by WorldId.
    MapPoint Center;
    std::vector<MapPoint> Polygon;
    std::uint32_t Height = 20;
    std::uint32_t Biome = 4;
};
struct MapGeometry
{
    std::uint32_t Version = 1;
    std::string Name, SourceVersion, SourceSeed, DistanceUnit;
    double Width = 0, Height = 0, DistanceScale = 1;
    std::map<Id, MapCell> Cells; // One campaign Area per polygon, including non-inhabitable water.
};
struct PoliticalDomain
{
    Id Key = 0; std::string Name; Id HeadCharacterId = 0;
    Id CapitalAreaId = 0;
    Deity PrimaryPatron = Deity::Count;
    Deity SecondaryPatron = Deity::Count;
    double PrimaryPatronWeight = 1;
    double InfluenceBudget = 0; // Persisted weighted reach. 0 means an older/unconfigured world.
};
struct PoliticalEntity { Id Key = 0; std::string Name; Id HeadCharacterId = 0; };
struct BirthrightGrant
{
    Id SourceParentId = 0;
    double FullPower = 0;
    bool Released = false;
    double GrantedFavor = 0; // Lifetime vesting receipt; spending active power never resets it.
};
struct Character
{
    Id Key = 0;
    std::string Name;
    Id AreaId = 0;
    Id CultureId = 0;
    std::int64_t BornDay = 0;
    bool Alive = true;
    std::int64_t DiedDay = 0;
    std::vector<Id> Parents;
    Id SpouseId = 0;
    std::array<Affinity, DeityCount> Affinities{};
    std::array<BirthrightGrant, DeityCount> Birthright{};
};
struct Event
{
    std::string OperationId;
    std::string Kind;
    std::int64_t Day = 0;
    Id ActorId = 0;
    Id OtherId = 0;
    std::string Detail;
};
struct BirthrightAllocation
{
    Id ParentId = 0;
    Deity Path = Deity::Strife;
    double FavorBudget = 0;
    bool SplitAuthorized = false;
};
struct BirthrightShare { Id ChildId = 0; double Fraction = 0; };
struct TravelRules
{
    std::uint32_t Version = 1;
    double DaysPerEffort = 1; // An ordinary undeveloped plains link costs one day.
};
enum class TroopKind : std::uint32_t { Peasant };
struct Troop { Id Key = 0, LeaderId = 0, CultureId = 0, HomeAreaId = 0; TroopKind Kind = TroopKind::Peasant; };
struct RecruitmentUse { Id AreaId = 0, CultureId = 0; std::uint32_t Count = 0; };
struct RecruitmentOffer { Id CultureId = 0; std::uint32_t Available = 0; std::string Approval; };
// Versioned host contracts. Troops lists all currently deployed living campaign members;
// casualty IDs come from the combat authority, never from player-supplied health values.
struct WarbandBattleInput {
    std::uint32_t Version = 1;
    std::string WorldId, EncounterId;
    Id LeaderId = 0, AreaId = 0;
    std::vector<Id> Troops;
};
struct WarbandBattleResult {
    std::uint32_t Version = 1;
    std::string EncounterId;
    std::vector<Id> Casualties;
};
// Versioned, opt-in policy. Legacy worlds retain their original behavior until configured.
struct EvolutionRules
{
    std::uint32_t Version = 3;
    bool Enabled = false;
    std::uint32_t DecisionDays = 30;
    std::uint32_t DemographyDays = 365;
    std::uint32_t NamedPerRegion = 4;
    std::uint32_t MaxChildren = 3;
    double AnnualBirthChance = 0.22;
    std::int64_t FertilityEndDays = 18250;
    std::int64_t MortalityStartDays = 20075;
    std::int64_t MaximumAgeDays = 34675;
    double FoodReserveDays = 45;
    double AnnualFoodSpoilage = 0.125; // Provisional fraction lost per 365 campaign days; zero disables.
    double SeasonalAmplitude = 0.12;
    double TaxPerPerson = 0.001;
    double UpkeepPerPerson = 0.0007;
    double TreasuryPerPerson = 1;
    double ActivityFavor = 6;
    double InvestmentPerPerson = 0.02;
    double CultivationGain = 0.015;
};
struct AreaEconomy
{
    double ProductiveCapacity = 0;
    double FoodStored = 0;
    double Produced = 0;
    double Consumed = 0;
    double Wasted = 0;
    double Spoiled = 0; // Subset of Wasted; remainder is storage overflow.
};
struct DomainEconomy
{
    double Treasury = 0;
    double Revenue = 0;
    double Spent = 0;
    double Overflow = 0;
    double Legitimacy = 0.65;
};
struct EvolutionState
{
    EvolutionRules Policy;
    std::int64_t SpoilageStartedDay = 0;
    std::map<Id, AreaEconomy> Areas;
    std::map<Id, DomainEconomy> Domains;
    std::map<Id, std::int64_t> LastActivity;
    std::vector<Id> ProtectedCharacters; // Explicit player/host control, excluded from autonomous policies.
};
struct ActivityIntent;
enum class OfficeKind : std::uint32_t { Domain, Entity };
struct Snapshot
{
    std::uint32_t Version = 11;
    std::string WorldId;
    std::uint32_t Seed = 0;
    std::int64_t Day = 0;
    double DayFraction = 0; // Committed fractional campaign time; daily simulation runs at integer boundaries.
    TravelRules TravelPolicy;
    Rules Policy;
    EvolutionState Evolution;
    PoliticsState Politics;
    MarketState Market;
    BanditState Bandits;
    ConnectivityRules Connectivity;
    MapGeometry Geometry;
    std::map<std::pair<Id, Id>, TraversalConnection> Connections;
    std::map<Id, Culture> Cultures;
    std::map<Id, Area> Areas;
    std::map<Id, Region> Regions;
    std::map<Id, PoliticalDomain> Domains;
    std::map<Id, PoliticalEntity> Entities;
    std::map<Id, Character> Characters;
    std::map<Id, PlayerProgression> Players;
    Id NextTroopId = 1;
    std::map<Id, Troop> Troops;
    std::vector<RecruitmentUse> RecruitmentUses;
    std::vector<Event> Events;
    std::vector<BirthrightAllocation> Allocations;
};
struct Result
{
    bool Accepted = false;
    std::string Code;
    std::string Detail;
    explicit operator bool() const { return Accepted; }
};
struct Generation
{
    std::string WorldId = "birthright-prototype";
    std::uint32_t Seed = 42;
    std::uint32_t Width = 30;
    std::uint32_t Height = 20;
    std::uint32_t RegionWidth = 5;
    std::uint32_t RegionHeight = 2;
    std::uint32_t DomainCount = 6; // Independent of the seven divine affinities.
};
struct Metrics
{
    std::int64_t Day = 0;
    double DayFraction = 0;
    std::int64_t Population = 0;
    std::size_t Areas = 0;
    std::size_t LandAreas = 0;
    std::size_t WaterAreas = 0;
    std::size_t NamedCharacters = 0;
    std::size_t Operations = 0;
    double MeanUnrest = 0;
};

double CommandLevels(const Snapshot& state, Id leader);
std::uint32_t TroopCapacity(const Snapshot& state, Id leader);
std::uint32_t TroopCount(const Snapshot& state, Id leader);
std::vector<RecruitmentOffer> LocalRecruitment(const Snapshot& state, Id leader);
Result ValidateWarbands(const Snapshot& state);

Result ValidateRules(const Rules& rules);
Result Validate(const Snapshot& state);
Result Generate(const Generation& options, const Rules& rules, Snapshot& output);
Result Save(const Snapshot& state, std::ostream& output);
Result Load(std::istream& input, Snapshot& output);
Result ExportJson(const Snapshot& state, std::ostream& output);

// Single authority. Caller authenticates player/AI intents; these are trusted host operations.
// No Actors, UObject access, wall clock, physics, persistence side effects or global random state.
class World
{
public:
    Result Restore(const Snapshot& snapshot);
    const Snapshot& State() const { return StateData; }
    Metrics Inspect() const;
    Result ChooseOrigin(const std::string& operation, Id player, const std::string& origin);
    Result LearnDivineNode(const std::string& operation, Id player, const std::string& node);
    Result RespecDivineNodes(const std::string& operation, Id player);
    Result AssignDivineNode(const std::string& operation, Id player, std::uint32_t slot, const std::string& node);
    Result EquipPlayerItem(const std::string& operation, Id player, EquipmentSlot slot, Id item);
    std::vector<Id> ChildrenOf(Id parent) const;
    Result TendWarband(const std::string& operation, Id leader, Id expectedArea, Id trainTroop = 0);
    Result RecruitPeasants(const std::string& operation, Id leader, Id expectedArea, Id culture, std::uint32_t count);
    Result RecordTroopBattleCasualties(const std::string& operation, const WarbandBattleInput& input, const WarbandBattleResult& result);
    Result DismissTroop(const std::string& operation, Id leader, Id troop);
    Result Marry(const std::string& operation, Id first, Id second);
    Result Birth(const std::string& operation, Id child, const std::string& name,
        Id firstParent, Id secondParent, Deity primary, Deity secondary);
    Result AwardFavor(const std::string& operation, Id character, Deity deity,
        double favor, const std::string& reason);
    Result ResolveStrifeDuel(const std::string& operation, Id winner, Id loser);
    Result ReleaseBirthright(const std::string& operation, Id parent, Id child);
    Result SplitBirthright(const std::string& operation, Id parent, Deity deity,
        const std::vector<BirthrightShare>& shares);
    Result RecordDeath(const std::string& operation, Id character, const std::string& cause);
    Result AdvanceDays(std::uint32_t days);
    Result ConfigureBandits(const std::string& operation, const BanditRules& policy);
    Result BeginCampAssault(const std::string& operation, Id leader, Id camp);
    Result AbandonCampAssault(const std::string& operation, Id leader);
    Result ApplyCampCombat(const CampCombatEvidence& evidence);
    Result ConfigureEvolution(const std::string& operation, const EvolutionRules& policy);
    Result PerformActivity(const std::string& operation, const ActivityIntent& intent);
    Result AppointOffice(const std::string& operation, OfficeKind kind, Id office, Id candidate);
    Result PromoteCandidate(const std::string& operation, Id character, Id area, Id culture);
    Result ConfigurePolitics(const std::string& operation, const PoliticsRules& policy);
    Result ConfigureMarket(const std::string& operation, const MarketRules& policy);
    Result InvestEstate(const std::string& operation, Id character, Id region);
    Result BefriendLord(const std::string& operation, Id liege, Id lord);
    Result GrantRegion(const std::string& operation, Id liege, Id lord, Id region);
    Result SpendFamilyWealth(const std::string& operation, Id character);
    Result JoinFaction(const std::string& operation, Id character);
    Result Rebel(const std::string& operation, Id faction);
    Result DeclareWar(const std::string& operation, Id ruler, Id targetRegion);
    Result RaiseArmy(const std::string& operation, Id ruler, Id war);
    Result DispatchCampExpedition(const std::string& operation, Id owner, Id camp);
    Result ResolveWarRound(const std::string& operation, Id war);
    Result Travel(const std::string& operation, Id character, Id destinationArea);
    Result DevelopArea(const std::string& operation, Id actor, Id area, double development);
    Result DevelopConnection(const std::string& operation, Id actor, const TraversalConnection& connection);
    Result SetInfluenceBudget(const std::string& operation, Id actor, Id domain, double budget);

private:
    friend Result AdvanceBandits(World& world);
    friend Result AdvanceDefense(World& world);
    friend Result AdvanceEvolution(World& world);
    friend Result AdvancePolitics(World& world);
    friend Result AdvanceEstates(World& world);
    // Only the private AdvanceDays candidate batches mutations; external commands remain atomic.
    bool Batched = false;
    mutable std::unordered_set<std::string> OperationIndex;
    mutable std::size_t IndexedEvents = 0;
    Snapshot BeginMutation();
    Snapshot StateData;
    Result CheckOperation(const std::string& operation) const;
    Result Commit(Snapshot&& next);
};
}
