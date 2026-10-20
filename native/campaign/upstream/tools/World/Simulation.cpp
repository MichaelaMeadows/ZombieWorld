#include "Simulation.h"
#include "EconomyObservation.h"
#include "../../Source/Birthright/Campaign/WorldEvolution.h"
#include "WorldPolitics.h"
#include "WorldMarket.h"
#include "WorldBandits.h"
#include "EmpireGeneration.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ostream>
#include <set>
#include <sstream>

namespace Birthright::Campaign::Testing
{
namespace
{
Result Ok() { return {true, "accepted", ""}; }
std::string Quote(const std::string& value)
{
    std::ostringstream out; out << '"';
    for (unsigned char c : value)
        if (c == '"' || c == '\\') out << '\\' << static_cast<char>(c);
        else if (c < 32) out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
        else out << static_cast<char>(c);
    out << '"'; return out.str();
}
Result Failure(const Scenario& scenario, std::int64_t day, const std::string& code, const std::string& detail)
{
    return {false, code, "scenario=" + scenario.Id + "; day=" + std::to_string(day) + "; " + detail};
}
bool HasBirthright(const Character& c)
{
    for (std::size_t d = 0; d < DeityCount; ++d)
        if (c.Affinities[d].BirthrightPower > 0 || c.Birthright[d].FullPower > 0) return true;
    return false;
}
double TotalPower(const Character& c, const Rules& rules)
{
    double total = 0;
    for (const auto& affinity : c.Affinities) total += Power(affinity, rules);
    return total;
}
bool Changed(const Character& a, const Character& b)
{
    if (a.Alive != b.Alive || a.AreaId != b.AreaId || a.CultureId != b.CultureId ||
        a.SpouseId != b.SpouseId || a.Parents != b.Parents) return true;
    for (std::size_t d = 0; d < DeityCount; ++d)
    {
        const auto& x = a.Affinities[d]; const auto& y = b.Affinities[d];
        const auto& p = a.Birthright[d]; const auto& q = b.Birthright[d];
        if (x.BirthrightPower != y.BirthrightPower || x.EarnedFavor != y.EarnedFavor ||
            x.TransferredFavor != y.TransferredFavor || p.SourceParentId != q.SourceParentId ||
            p.FullPower != q.FullPower || p.Released != q.Released || p.GrantedFavor != q.GrantedFavor) return true;
    }
    return false;
}
void Ids(std::ostream& out, const std::vector<Id>& ids)
{
    out << '[';
    for (std::size_t i = 0; i < ids.size(); ++i) { if (i) out << ','; out << ids[i]; }
    out << ']';
}
Observation Inspect(const Snapshot& now, const Snapshot& before, std::uint32_t elapsed, bool final, bool baseline)
{
    Observation observation; observation.Day = now.Day; observation.ElapsedDays = elapsed; observation.Final = final;
    const auto empire = InspectEmpires(now);
    auto& m = observation.Metrics;
    m = {{"population", static_cast<double>(empire.Population)}, {"empires", static_cast<double>(now.Domains.size()-now.Politics.RetiredDomains.size())},
        {"claimedAreas", static_cast<double>(empire.ClaimedAreas)}, {"contestedAreas", static_cast<double>(empire.ContestedAreas)},
        {"unclaimedAreas", static_cast<double>(empire.UnclaimedAreas)}, {"wildernessAreas", static_cast<double>(empire.WildernessAreas)},
        {"namedNpcs", static_cast<double>(now.Characters.size())}, {"livingNpcs", 0}, {"deadNpcs", 0},
        {"birthrightNpcs", 0}, {"livingBirthrightNpcs", 0}, {"deadBirthrightNpcs", 0}, {"birthrightPower", 0},
        {"birthrightNpcTotalPower", 0}, {"vacantEmpireHeads", 0}, {"meanUnrest", 0}, {"meanProsperity", 0},
        {"newNpcs", 0}, {"npcDeaths", 0}, {"events", 0}};
    std::size_t land = 0;
    for (const auto& area : now.Areas) if (!area.second.IsWater)
    {
        ++land; m["meanUnrest"] += area.second.Unrest; m["meanProsperity"] += area.second.Prosperity;
    }
    if (land) { m["meanUnrest"] /= static_cast<double>(land); m["meanProsperity"] /= static_cast<double>(land); }
    for (const auto& domain : now.Domains)
    {
        if (!baseline && !before.Domains.count(domain.first)) observation.CreatedEmpires.push_back(domain.first);
        if (!baseline && !before.Domains.count(domain.first) && now.Politics.RetiredDomains.count(domain.first)) observation.DestroyedEmpires.push_back(domain.first);
        if (now.Politics.RetiredDomains.count(domain.first)) continue;
        if (!domain.second.HeadCharacterId) ++m["vacantEmpireHeads"];
    }
    if (!baseline) for (const auto& domain : before.Domains)
        if (!before.Politics.RetiredDomains.count(domain.first) && (!now.Domains.count(domain.first) || now.Politics.RetiredDomains.count(domain.first))) observation.DestroyedEmpires.push_back(domain.first);
    for (const auto& item : now.Characters)
    {
        const auto& c = item.second; const auto previous = before.Characters.find(item.first);
        ++m[c.Alive ? "livingNpcs" : "deadNpcs"];
        if (!baseline && previous == before.Characters.end()) ++m["newNpcs"];
        if (!baseline && !c.Alive && (previous == before.Characters.end() || previous->second.Alive)) ++m["npcDeaths"];
        const bool bearer = HasBirthright(c);
        if (bearer)
        {
            ++m["birthrightNpcs"]; ++m[c.Alive ? "livingBirthrightNpcs" : "deadBirthrightNpcs"];
            if (c.Alive)
            {
                for (const auto& affinity : c.Affinities) m["birthrightPower"] += affinity.BirthrightPower;
                m["birthrightNpcTotalPower"] += TotalPower(c, now.Policy);
            }
            if (!baseline && (previous == before.Characters.end() || !HasBirthright(previous->second))) observation.NewBirthrightNpcs.push_back(item.first);
        }
        if (!baseline && previous != before.Characters.end() && (bearer || HasBirthright(previous->second)) && Changed(c, previous->second))
            observation.ChangedBirthrightNpcs.push_back(item.first);
        if (!baseline && previous != before.Characters.end() && previous->second.Alive && HasBirthright(previous->second) && (!c.Alive || !bearer))
            observation.LostBirthrightNpcs.push_back(item.first);
    }
    if (!baseline) for (const auto& item : before.Characters)
        if (item.second.Alive && HasBirthright(item.second) && !now.Characters.count(item.first)) observation.LostBirthrightNpcs.push_back(item.first);
    m["events"] = baseline ? 0 : static_cast<double>(now.Events.size() - before.Events.size());
    m["createdEmpires"] = static_cast<double>(observation.CreatedEmpires.size());
    m["destroyedEmpires"] = static_cast<double>(observation.DestroyedEmpires.size());
    m["newBirthrightNpcs"] = static_cast<double>(observation.NewBirthrightNpcs.size());
    m["lostBirthrightNpcs"] = static_cast<double>(observation.LostBirthrightNpcs.size());
    m["changedBirthrightNpcs"] = static_cast<double>(observation.ChangedBirthrightNpcs.size());
    m["populationDelta"] = baseline ? 0 : static_cast<double>(empire.Population - InspectEmpires(before).Population);
    m["foodStored"] = 0; m["foodProduced"] = 0; m["foodConsumed"] = 0; m["foodWasted"] = 0;
    m["families"]=static_cast<double>(now.Politics.Families.size()); m["familyWealth"]=0; m["familySpending"]=0;
    m["activeFactions"]=0; m["activeWars"]=0; m["armySoldiers"]=0; m["armyCasualties"]=static_cast<double>(now.Politics.Casualties); m["retiredEmpires"]=static_cast<double>(now.Politics.RetiredDomains.size());
    m["meanLoyalty"]=0; m["meanDesiredHoldings"]=0; m["rebellions"]=0; m["warsDeclared"]=0;
    for(const auto& f:now.Politics.Families) { m["familyWealth"]+=f.second.Wealth; m["familySpending"]+=f.second.Spent; }
    for(const auto& f:now.Politics.Factions) if(f.second.Status==FactionStatus::Organizing) ++m["activeFactions"];
    for(const auto& w:now.Politics.Wars) if(w.second.Active) ++m["activeWars"];
    for(const auto& a:now.Politics.Armies) m["armySoldiers"]+=a.second.Soldiers;
    for(const auto& l:now.Politics.Lords) { m["meanLoyalty"]+=l.second.Loyalty; m["meanDesiredHoldings"]+=DesiredHoldings(now,l.first); }
    if(!now.Politics.Lords.empty()) { m["meanLoyalty"]/=static_cast<double>(now.Politics.Lords.size()); m["meanDesiredHoldings"]/=static_cast<double>(now.Politics.Lords.size()); }
    m["marketMoney"]=MarketMoney(now); m["foodTraded"]=now.Market.FoodTraded; m["estateInvestment"]=now.Market.Investment; m["inheritedWealth"]=now.Market.Inherited; m["armyFood"]=now.Market.ArmyFood; m["wages"]=now.Market.Wages; m["taxes"]=now.Market.Taxes;
    double regionalPopulation=0, totalPopulation=0;
    m["weightedFoodCoverage"]=0; m["populationBelowFood95"]=0; m["weightedNutrition"]=0; m["weightedWageCoverage"]=0; m["householdCash"]=0; m["producerCash"]=0; m["goodsProduced"]=0; m["goodsWasted"]=0;
    for(const auto& row:now.Areas) { double n=0; for(const auto& c:row.second.Cohorts)n+=static_cast<double>(c.Population);
        if(now.Market.Areas.count(row.first)) { totalPopulation+=n; m["weightedNutrition"]+=n*now.Market.Areas.at(row.first).Nutrition; }
        if(row.second.RegionId && now.Market.Regions.count(row.second.RegionId)) { const auto& r=now.Market.Regions.at(row.second.RegionId); regionalPopulation+=n; m["weightedFoodCoverage"]+=n*r.FoodCoverage; m["weightedWageCoverage"]+=n*r.WageCoverage; if(r.FoodCoverage<0.95)m["populationBelowFood95"]+=n; }
    }
    if(regionalPopulation>0) { m["weightedFoodCoverage"]/=regionalPopulation; m["weightedWageCoverage"]/=regionalPopulation; }
    if(totalPopulation>0)m["weightedNutrition"]/=totalPopulation;
    for(const auto& row:now.Market.Regions) { m["householdCash"]+=row.second.HouseholdCash; m["producerCash"]+=row.second.ProducerCash; for(std::size_t g=0;g<MarketGoodCount;++g) {m["goodsProduced"]+=row.second.Produced[g];m["goodsWasted"]+=row.second.Wasted[g];} }
    m["meanFoodCoverage"]=0; m["meanFoodPrice"]=0; for(const auto& r:now.Market.Regions) { m["meanFoodCoverage"]+=r.second.FoodCoverage; m["meanFoodPrice"]+=r.second.FoodPrice; }
    if(!now.Market.Regions.empty()) { m["meanFoodCoverage"]/=now.Market.Regions.size(); m["meanFoodPrice"]/=now.Market.Regions.size(); }
    m["treasury"] = 0; m["meanLegitimacy"] = 0; m["successions"] = 0; m["activities"] = 0;
    for (const auto& a : now.Evolution.Areas) { m["foodStored"] += a.second.FoodStored; m["foodProduced"] += a.second.Produced; m["foodConsumed"] += a.second.Consumed; m["foodWasted"] += a.second.Wasted; }
    for (const auto& row : now.Bandits.Camps) { const auto& c=row.second; m["foodStored"]+=c.FoodStored; m["foodConsumed"]+=c.FoodConsumed; m["foodWasted"]+=c.FoodWasted; }
    for (const auto& d : now.Evolution.Domains) { m["treasury"] += d.second.Treasury; m["meanLegitimacy"] += d.second.Legitimacy; }
    if (!now.Evolution.Domains.empty()) m["meanLegitimacy"] /= static_cast<double>(now.Evolution.Domains.size());
    for (std::size_t i = baseline ? now.Events.size() : before.Events.size(); i < now.Events.size(); ++i) {
        const auto& kind = now.Events[i].Kind; if (kind == "succession") ++m["successions"];
        if(kind=="rebellion") ++m["rebellions"]; if(kind=="war_declared") ++m["warsDeclared"];
        if (kind == "practice" || kind == "govern" || kind == "cultivate") ++m["activities"];
    }
    ObserveEconomy(now,before,baseline,m);
    return observation;
}
bool Compare(double actual, const Condition& c)
{
    if (c.Operator == "eq") return std::abs(actual - c.Value) <= 1e-9;
    if (c.Operator == "ne") return std::abs(actual - c.Value) > 1e-9;
    if (c.Operator == "ge") return actual >= c.Value;
    if (c.Operator == "gt") return actual > c.Value;
    if (c.Operator == "le") return actual <= c.Value;
    return actual < c.Value; // Validated lt.
}
void WriteCheckpoint(std::ostream& out, const Scenario& scenario, const Observation& o,
    const Snapshot& now, const Snapshot& before, bool baseline, bool passed)
{
    out << std::setprecision(17) << "{\"schemaVersion\":1,\"kind\":\"checkpoint\",\"scenarioId\":" << Quote(scenario.Id)
        << ",\"worldId\":" << Quote(now.WorldId) << ",\"seed\":" << now.Seed << ",\"day\":" << now.Day
        << ",\"dayFraction\":" << now.DayFraction << ",\"elapsedDays\":" << o.ElapsedDays
        << ",\"baseline\":" << (baseline ? "true" : "false") << ",\"final\":" << (o.Final ? "true" : "false")
        << ",\"passed\":" << (passed ? "true" : "false") << ",\"metrics\":{";
    bool comma = false;
    for (const auto& metric : o.Metrics) { if (comma) out << ','; comma = true; out << Quote(metric.first) << ':' << metric.second; }
    out << "},\"createdEmpires\":"; Ids(out, o.CreatedEmpires);
    out << ",\"destroyedEmpires\":"; Ids(out, o.DestroyedEmpires);
    out << ",\"newBirthrightNpcs\":"; Ids(out, o.NewBirthrightNpcs);
    out << ",\"lostBirthrightNpcs\":"; Ids(out, o.LostBirthrightNpcs);
    out << ",\"changedBirthrightNpcs\":"; Ids(out, o.ChangedBirthrightNpcs);
    WriteEconomyRegions(out,now,before,baseline);
    ExportBandits(now,out);
    const auto hierarchy = InspectHierarchy(now), previous = InspectHierarchy(before);
    out << ",\"empires\":["; comma = false;
    for (const auto& item : now.Domains)
    {
        if(now.Politics.RetiredDomains.count(item.first)) continue;
        if (comma) out << ','; comma = true;
        const auto& territory = hierarchy.Domains.at(item.first);
        const auto prior = previous.Domains.find(item.first);
        const auto populationBefore = prior == previous.Domains.end() ? 0 : prior->second.Population;
        const auto areasBefore = prior == previous.Domains.end() ? 0 : static_cast<std::int64_t>(prior->second.AreaIds.size());
        out << "{\"id\":" << item.first << ",\"name\":" << Quote(item.second.Name) << ",\"headId\":" << item.second.HeadCharacterId
            << ",\"population\":" << territory.Population << ",\"populationDelta\":" << (baseline ? 0 : territory.Population - populationBefore)
            << ",\"areas\":" << territory.AreaIds.size() << ",\"areasDelta\":" << (baseline ? 0 : static_cast<std::int64_t>(territory.AreaIds.size()) - areasBefore)
            << ",\"regions\":" << territory.RegionIds.size() << '}';
    }
    out << "],\"birthrightNpcs\":["; comma = false;
    for (const auto& item : now.Characters) if (HasBirthright(item.second))
    {
        if (comma) out << ','; comma = true; const auto& c = item.second;
        const auto prior = before.Characters.find(item.first);
        const auto ageDay = c.Alive ? now.Day : c.DiedDay;
        out << "{\"id\":" << item.first << ",\"name\":" << Quote(c.Name) << ",\"alive\":" << (c.Alive ? "true" : "false")
            << ",\"ageDays\":" << ageDay - c.BornDay << ",\"areaId\":" << c.AreaId << ",\"cultureId\":" << c.CultureId << ",\"parents\":";
        Ids(out, c.Parents); out << ",\"deities\":[";
        for (std::size_t d = 0; d < DeityCount; ++d)
        {
            if (d) out << ','; const auto& a = c.Affinities[d]; const auto& grant = c.Birthright[d];
            const auto oldPower = prior == before.Characters.end() ? 0 : Power(prior->second.Affinities[d], before.Policy);
            out << "{\"name\":" << Quote(DeityName(static_cast<Deity>(d))) << ",\"power\":" << Power(a, now.Policy)
                << ",\"powerDelta\":" << (baseline ? 0 : Power(a, now.Policy) - oldPower) << ",\"birthrightPower\":" << a.BirthrightPower
                << ",\"earnedFavor\":" << a.EarnedFavor << ",\"transferredFavor\":" << a.TransferredFavor
                << ",\"sourceParentId\":" << grant.SourceParentId << ",\"fullBirthrightPower\":" << grant.FullPower
                << ",\"released\":" << (grant.Released ? "true" : "false") << ",\"grantedFavor\":" << grant.GrantedFavor << '}';
        }
        out << "]}";
    }
    out << "],\"events\":[";
    for (std::size_t i = baseline ? now.Events.size() : before.Events.size(); i < now.Events.size(); ++i)
    {
        if (i != before.Events.size()) out << ','; const auto& e = now.Events[i];
        out << "{\"operationId\":" << Quote(e.OperationId) << ",\"kind\":" << Quote(e.Kind) << ",\"day\":" << e.Day
            << ",\"actorId\":" << e.ActorId << ",\"otherId\":" << e.OtherId << ",\"detail\":" << Quote(e.Detail) << '}';
    }
    out << "],\"conditions\":["; comma = false;
    for (const auto& c : scenario.Conditions) if (c.Scope == "every" || o.Final)
    {
        if (comma) out << ','; comma = true;
        out << "{\"id\":" << Quote(c.Id) << ",\"metric\":" << Quote(c.Metric) << ",\"operator\":" << Quote(c.Operator)
            << ",\"expected\":" << c.Value << ",\"actual\":" << o.Metrics.at(c.Metric)
            << ",\"passed\":" << (Compare(o.Metrics.at(c.Metric), c) ? "true" : "false") << '}';
    }
    out << "]}\n";
}
}

Result SeedBirthrightFamilies(World& world, const std::string& scenarioId)
{
    World candidate; auto result = candidate.Restore(world.State()); if (!result) return result;
    if (candidate.State().Characters.rbegin()->first == std::numeric_limits<Id>::max()) return {false, "fixture_id_limit", scenarioId};
    Id next = candidate.State().Characters.rbegin()->first + 1;
    // A synthetic starting fixture, using existing adults and authoritative family commands.
    for (const auto& item : world.State().Domains)
    {
        const auto parent = item.second.HeadCharacterId; Id spouse = 0;
        if (!parent || !candidate.State().Characters.at(parent).Alive || candidate.State().Characters.at(parent).SpouseId)
            return {false, "fixture_parent_ineligible", scenarioId + "; empire=" + std::to_string(item.first)};
        for (const auto& c : candidate.State().Characters)
            if (c.first != parent && c.second.CultureId == candidate.State().Characters.at(parent).CultureId && c.second.Alive &&
                !c.second.SpouseId && candidate.State().Day - c.second.BornDay >= candidate.State().Policy.AdultAgeDays)
            { spouse = c.first; break; }
        if (!spouse || !next) return {false, "fixture_spouse_or_id_missing", scenarioId + "; empire=" + std::to_string(item.first)};
        const auto operation = scenarioId + "-fixture-" + std::to_string(item.first);
        result = candidate.Marry(operation + "-marriage", parent, spouse); if (!result) return result;
        const auto primary = item.second.PrimaryPatron;
        const auto secondary = item.second.SecondaryPatron == Deity::Count ? static_cast<Deity>((static_cast<std::size_t>(primary) + 1) % DeityCount) : item.second.SecondaryPatron;
        result = candidate.Birth(operation + "-birth", next++, "Observation heir " + std::to_string(item.first), parent, spouse, primary, secondary);
        if (!result) return result;
    }
    return world.Restore(candidate.State());
}

Result Simulate(World& world, const Scenario& scenario, std::ostream& out, const Observer& observer)
{
    const auto start = world.State().Day;
    auto valid = Validate(world.State()); if (!valid) return Failure(scenario, start, valid.Code, valid.Detail);
    if (scenario.Version != 1 || scenario.Id.empty() || scenario.Id.size() > 128 || !scenario.ReportEveryDays ||
        scenario.Days > static_cast<std::uint32_t>(100000000 - start) ||
        (scenario.Days == static_cast<std::uint32_t>(100000000 - start) && world.State().DayFraction > 0))
        return Failure(scenario, start, "scenario_invalid", "Version 1, scenario ID and a positive reporting interval inside the campaign clock range are required");
    auto before = world.State();
    const auto baseline = Inspect(before, before, 0, scenario.Days == 0, true);
    std::set<std::string> ids;
    const std::set<std::string> operators = {"eq", "ne", "ge", "gt", "le", "lt"};
    for (const auto& c : scenario.Conditions)
        if (c.Id.empty() || !ids.insert(c.Id).second || !baseline.Metrics.count(c.Metric) || !operators.count(c.Operator) ||
            (c.Scope != "every" && c.Scope != "final") || !std::isfinite(c.Value))
            return Failure(scenario, start, "condition_invalid", "condition=" + c.Id + "; check unique ID, metric, operator, scope and finite value");
    auto actions = scenario.Actions;
    ids.clear();
    for (const auto& action : actions)
        if (action.Id.empty() || !ids.insert(action.Id).second || action.Day > scenario.Days || !action.Apply)
            return Failure(scenario, start, "action_invalid", "operation=" + action.Id);
    std::stable_sort(actions.begin(), actions.end(), [](const auto& a, const auto& b) { return a.Day < b.Day; });
    std::size_t actionIndex = 0;
    const auto apply = [&](std::uint32_t elapsed) -> Result {
        while (actionIndex < actions.size() && actions[actionIndex].Day == elapsed)
        {
            const auto& action = actions[actionIndex++]; const auto day = world.State().Day; const auto fraction = world.State().DayFraction;
            auto result = action.Apply(world);
            if (!result) return Failure(scenario, world.State().Day, result.Code, "operation=" + action.Id + "; " + result.Detail);
            if (world.State().Day != day || world.State().DayFraction != fraction)
                return Failure(scenario, day, "action_changed_clock", "operation=" + action.Id + "; actions must not advance time");
        }
        return Ok();
    };
    const auto emit = [&](std::uint32_t elapsed, bool initial) -> Result {
        auto result = Validate(world.State()); if (!result) return Failure(scenario, world.State().Day, result.Code, result.Detail);
        const auto o = Inspect(world.State(), before, elapsed, elapsed == scenario.Days, initial);
        Result checked = Ok();
        for (const auto& c : scenario.Conditions) if ((c.Scope == "every" || o.Final) && !Compare(o.Metrics.at(c.Metric), c))
        {
            std::ostringstream detail; detail << "condition=" << c.Id << "; metric=" << c.Metric << "; actual=" << o.Metrics.at(c.Metric)
                << "; expected " << c.Operator << ' ' << c.Value;
            checked = Failure(scenario, o.Day, "condition_failed", detail.str()); break;
        }
        if (checked && observer) { const auto observed = observer(o, world.State()); if (!observed) checked = Failure(scenario, o.Day, observed.Code, observed.Detail); }
        WriteCheckpoint(out, scenario, o, world.State(), before, initial, checked.Accepted); out.flush();
        if (!out) return Failure(scenario, o.Day, "report_write_failed", "Could not write checkpoint");
        before = world.State(); return checked;
    };
    auto result = apply(0); if (!result) return result;
    before = world.State(); result = emit(0, true); if (!result) return result;
    std::uint32_t elapsed = 0;
    while (elapsed < scenario.Days)
    {
        const auto untilReport = scenario.ReportEveryDays - elapsed % scenario.ReportEveryDays;
        auto step = std::min({scenario.Days - elapsed, untilReport, 36500u});
        if (actionIndex < actions.size()) step = std::min(step, actions[actionIndex].Day - elapsed);
        result = world.AdvanceDays(step); if (!result) return Failure(scenario, world.State().Day, result.Code, result.Detail);
        elapsed += step; result = apply(elapsed); if (!result) return result;
        if (elapsed == scenario.Days || elapsed % scenario.ReportEveryDays == 0) { result = emit(elapsed, false); if (!result) return result; }
    }
    return Ok();
}

int RunSimulationCli(int argc, char** argv)
{
    const auto fail = [](const Result& r) {
        std::cerr << "{\"status\":\"rejected\",\"code\":" << Quote(r.Code) << ",\"detail\":" << Quote(r.Detail) << "}\n"; return 1;
    };
    if (argc < 7) return fail({false, "arguments", "--simulate INPUT.world OUTPUT_PREFIX DAYS REPORT_EVERY_DAYS SCENARIO_ID [--seed-families] [--rule NAME VALUE] [--condition ID SCOPE METRIC OP VALUE]"});
    Scenario scenario; scenario.Id = argv[6];
    const auto parse = [](const char* text, auto& value) {
        const std::string s(text); const auto r = std::from_chars(s.data(), s.data() + s.size(), value);
        return r.ec == std::errc{} && r.ptr == s.data() + s.size();
    };
    if (!parse(argv[4], scenario.Days) || !parse(argv[5], scenario.ReportEveryDays)) return fail({false, "arguments", "Days and report interval must be unsigned integers"});
    Snapshot state; std::ifstream input(argv[2]); auto result = Load(input, state); if (!result) return fail(result);
    bool bandits = false, families = false, evolution = false, evolutionOverride = false, politics = false, economy = false;
    EvolutionRules evolutionPolicy; evolutionPolicy.Enabled = true;
    for (int i = 7; i < argc;)
    {
        const std::string option = argv[i++];
        if (option == "--seed-families") { families = true; continue; }
        if (option == "--evolution") { evolution = true; continue; }
        if (option == "--bandits") { bandits=true; evolution=true; continue; }
        if (option == "--economy") { economy=true; politics=true; evolution=true; continue; }
        if (option == "--politics") { politics = true; evolution = true; continue; }
        if (option == "--evolution-rule" && i + 2 <= argc)
        {
            const std::string name = argv[i++]; double value = 0;
            if (!parse(argv[i++], value) || !std::isfinite(value)) return fail({false, "arguments", "Invalid evolution parameter: " + name});
            const std::map<std::string, double*> fields = {
                {"annualFoodSpoilage", &evolutionPolicy.AnnualFoodSpoilage}, {"annualBirthChance", &evolutionPolicy.AnnualBirthChance}, {"foodReserveDays", &evolutionPolicy.FoodReserveDays},
                {"seasonalAmplitude", &evolutionPolicy.SeasonalAmplitude}, {"taxPerPerson", &evolutionPolicy.TaxPerPerson},
                {"upkeepPerPerson", &evolutionPolicy.UpkeepPerPerson}, {"treasuryPerPerson", &evolutionPolicy.TreasuryPerPerson},
                {"activityFavor", &evolutionPolicy.ActivityFavor}, {"investmentPerPerson", &evolutionPolicy.InvestmentPerPerson},
                {"cultivationGain", &evolutionPolicy.CultivationGain}};
            if (fields.count(name)) *fields.at(name) = value;
            else if (value >= 1 && value <= 100000 && value == std::floor(value)) {
                if (name == "decisionDays") evolutionPolicy.DecisionDays = static_cast<std::uint32_t>(value);
                else if (name == "demographyDays") evolutionPolicy.DemographyDays = static_cast<std::uint32_t>(value);
                else if (name == "namedPerRegion") evolutionPolicy.NamedPerRegion = static_cast<std::uint32_t>(value);
                else if (name == "maxChildren") evolutionPolicy.MaxChildren = static_cast<std::uint32_t>(value);
                else if (name == "fertilityEndDays") evolutionPolicy.FertilityEndDays = static_cast<std::int64_t>(value);
                else if (name == "mortalityStartDays") evolutionPolicy.MortalityStartDays = static_cast<std::int64_t>(value);
                else if (name == "maximumAgeDays") evolutionPolicy.MaximumAgeDays = static_cast<std::int64_t>(value);
                else return fail({false, "arguments", "Unknown evolution parameter: " + name});
            }
            else return fail({false, "arguments", "Unknown evolution parameter or invalid integer: " + name});
            evolutionOverride = true; continue;
        }
        if (option == "--condition" && i + 5 <= argc)
        {
            Condition c; c.Id = argv[i++]; c.Scope = argv[i++]; c.Metric = argv[i++]; c.Operator = argv[i++];
            if (!parse(argv[i++], c.Value)) return fail({false, "arguments", "Invalid condition value: " + c.Id});
            scenario.Conditions.push_back(c); continue;
        }
        if (option == "--rule" && i + 2 <= argc)
        {
            const std::string name = argv[i++]; double value = 0;
            if (!parse(argv[i++], value)) return fail({false, "arguments", "Invalid rule value: " + name});
            if (name == "dailyBirthRate") state.Policy.DailyBirthRate = value;
            else if (name == "dailyDeathRate") state.Policy.DailyDeathRate = value;
            else if (name == "dailyStewardFavor") state.Policy.DailyStewardFavor = value;
            else if (name == "adultAgeDays" && std::isfinite(value) && value >= 1 && value <= 100000 && std::floor(value) == value)
                state.Policy.AdultAgeDays = static_cast<std::int64_t>(value);
            else return fail({false, "arguments", "Unknown rule or invalid integer age: " + name});
            continue;
        }
        return fail({false, "arguments", "Unknown or incomplete option: " + option});
    }
    World world; result = world.Restore(state); if (!result) return fail(result);
    if (families) { result = SeedBirthrightFamilies(world, scenario.Id); if (!result) return fail(result); }
    if (evolutionOverride && (!evolution || world.State().Evolution.Policy.Enabled)) return fail({false, "arguments", "Evolution overrides require first-time --evolution; resumed saves retain their recorded policy."});
    if (evolution && !world.State().Evolution.Policy.Enabled) { result = world.ConfigureEvolution(scenario.Id + "/enable-evolution", evolutionPolicy); if (!result) return fail(result); }
    if(politics && !world.State().Politics.Policy.Enabled) { PoliticsRules policy; policy.Enabled=true; result=world.ConfigurePolitics(scenario.Id+"/enable-politics",policy); if(!result) return fail(result); }
    if(economy && !world.State().Market.Policy.Enabled) { MarketRules policy; policy.Enabled=true; result=world.ConfigureMarket(scenario.Id+"/enable-market",policy); if(!result) return fail(result); }
    if(bandits && !world.State().Bandits.Policy.Enabled) { BanditRules policy; policy.Enabled=true; result=world.ConfigureBandits(scenario.Id+"/enable-bandits",policy); if(!result) return fail(result); }
    const std::string prefix = argv[3];
    // Never replace the checkpoint supplied as input, including case-insensitive Windows paths.
    const auto normalized = [](const std::filesystem::path& path) {
        auto value = std::filesystem::weakly_canonical(path).generic_string();
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    };
    for (const auto& extension : {".world", ".initial.world", ".timeline.jsonl", ".json"})
        if (normalized(prefix + extension) == normalized(argv[2])) return fail({false, "output_overwrites_input", scenario.Id + "; choose a different output prefix"});
    const std::filesystem::path path(prefix);
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::ofstream initial(prefix + ".initial.world"); result = Save(world.State(), initial); initial.close();
    if (!result) return fail(result);
    if (!initial) return fail({false, "save_write_failed", scenario.Id + "; initial checkpoint"});
    std::ofstream timeline(prefix + ".timeline.jsonl");
    if (!timeline) return fail({false, "report_open_failed", scenario.Id});
    result = Simulate(world, scenario, timeline, [&](const Observation& o, const Snapshot&) {
        std::cout << "scenario=" << scenario.Id << " day=" << o.Day << " elapsed=" << o.ElapsedDays
            << " population=" << o.Metrics.at("population") << " empires=" << o.Metrics.at("empires")
            << " created=" << o.Metrics.at("createdEmpires") << " destroyed=" << o.Metrics.at("destroyedEmpires")
            << " birthrightLiving=" << o.Metrics.at("livingBirthrightNpcs") << " birthrightChanged=" << o.Metrics.at("changedBirthrightNpcs") << '\n' << std::flush;
        return Ok();
    });
    timeline.close(); if (!result) return fail(result);
    if (!timeline) return fail({false, "report_write_failed", scenario.Id});
    std::ofstream save(prefix + ".world"); result = Save(world.State(), save); save.close();
    if (!result) return fail(result);
    if (!save) return fail({false, "save_write_failed", scenario.Id + "; final checkpoint"});
    std::ofstream json(prefix + ".json"); result = ExportJson(world.State(), json); json.close();
    if (!result) return fail(result);
    if (!json) return fail({false, "json_write_failed", scenario.Id});
    std::cout << "PASS scenario=" << scenario.Id << " timeline=" << prefix + ".timeline.jsonl" << '\n'; return 0;
}
}
