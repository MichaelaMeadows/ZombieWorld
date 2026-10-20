#include "Simulation.h"
#include "EmpireGeneration.h"
#include "WorldEvolution.h"

#include <limits>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace Birthright::Campaign::Testing
{
namespace
{
void Check(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
void Accept(const Result& r) { Check(r.Accepted, r.Code + ": " + r.Detail); }
World Fixture(bool families = false)
{
    std::vector<GeographicArea> graph; Accept(MakeGridGeography(8, 4, 42, graph));
    EmpireGeneration options; options.EmpireCount = 1; options.RegionsPerEmpire = 1;
    options.ClaimRadius = 100; options.InfluenceRadius = 100; options.Seeds = {{1, Deity::Harmony}};
    Rules rules; rules.DailyBirthRate = 0; rules.DailyDeathRate = 0;
    Snapshot state; Accept(GenerateEmpires(graph, options, rules, state));
    for (auto& a : state.Areas) a.second.FoodCapacity = 1000000;
    World world; Accept(world.Restore(state));
    if (families) Accept(SeedBirthrightFamilies(world, "BR-056-test"));
    return world;
}
std::string Serialized(const World& world) { std::ostringstream out; Accept(Save(world.State(), out)); return out.str(); }
}

void AddSimulationTests(std::vector<std::pair<std::string, std::function<void()>>>& cases)
{
    AddPoliticsTests(cases);
    AddMarketTests(cases);
    AddBanditTests(cases);
    cases.push_back({"BR-057 policy and activity rejection are atomic and cooldowns survive restore", [] {
        auto w = Fixture(); const auto before = Serialized(w);
        Check(PlanActivities(w.State()).empty(), "Disabled policy proposed autonomous actions");
        EvolutionRules p; p.Enabled = true; p.AnnualBirthChance = std::numeric_limits<double>::quiet_NaN();
        Check(!w.ConfigureEvolution("bad-policy", p), "NaN policy accepted"); Check(Serialized(w) == before, "Bad policy mutated state");
        p.AnnualBirthChance = 0.22; Accept(w.ConfigureEvolution("enable", p));
        const auto actor = w.State().Characters.begin()->second;
        ActivityIntent action{1, actor.Key, actor.AreaId, CampaignActivity::Practice};
        Accept(w.PerformActivity("practice", action)); const auto after = Serialized(w);
        Check(!w.PerformActivity("duplicate", action), "Cooldown ignored");
        Check(!w.PerformActivity("practice", action), "Replay accepted");
        action.Area = 999999; Check(!w.PerformActivity("remote", action), "Remote action accepted");
        Check(Serialized(w) == after, "Rejected activity mutated state");
        Snapshot restored; std::istringstream in(after); Accept(Load(in, restored)); Accept(w.Restore(restored));
        action.Area = actor.AreaId; Check(!w.PerformActivity("resumed", action), "Restore lost cooldown");
        Accept(w.AdvanceDays(p.DecisionDays - 1)); Check(!w.PerformActivity("early", action), "Cooldown ended early");
    }});
    cases.push_back({"BR-057 economic activity requires an office and budget and balances its ledger", [] {
        auto w = Fixture(); EvolutionRules p; p.Enabled = true; Accept(w.ConfigureEvolution("enable", p));
        auto state = w.State(); const Id head = state.Domains.begin()->second.HeadCharacterId;
        ActivityIntent action{1, head, state.Characters.at(head).AreaId, CampaignActivity::Cultivate};
        Check(!w.PerformActivity("unfunded", action), "Unfunded action accepted");
        auto& money = state.Evolution.Domains.begin()->second; money.Treasury = money.Revenue = 100000;
        Accept(w.Restore(state)); const auto quoted = QuoteActivity(w.State(), action); Accept(quoted.Eligibility);
        Accept(w.PerformActivity("funded", action));
        const auto& spent = w.State().Evolution.Domains.begin()->second;
        Check(std::abs(spent.Spent - quoted.Cost) < 1e-8 && std::abs(spent.Treasury + spent.Spent - spent.Revenue) < 1e-8, "Cost not reconciled");
        auto invalid = w.State(); invalid.Evolution.Areas.begin()->second.FoodStored = 1;
        Check(!w.Restore(invalid), "Fabricated food accepted");
    }});
    cases.push_back({"BR-057 autonomous life cycle respects player control and releases birthright", [] {
        auto w = Fixture(true); auto s = w.State();
        const Id parent = s.Domains.begin()->second.HeadCharacterId;
        const Id child = s.Characters.rbegin()->first;
        const Id protectedAdult = s.Characters.at(parent).SpouseId;
        s.Characters.at(parent).BornDay = -40000;
        s.Characters.at(protectedAdult).BornDay = -40000;
        s.Evolution.ProtectedCharacters.push_back(child);
        s.Evolution.ProtectedCharacters.push_back(protectedAdult);
        Accept(w.Restore(s)); EvolutionRules p; p.Enabled = true; Accept(w.ConfigureEvolution("enable", p));
        const auto favors = w.State().Characters.at(child).Affinities;
        const auto adultFavors = w.State().Characters.at(protectedAdult).Affinities;
        Accept(w.AdvanceDays(365));
        Check(!w.State().Characters.at(parent).Alive, "Maximum age ignored");
        Check(w.State().Characters.at(protectedAdult).Alive, "Protected adult died through autonomous mortality");
        Check(w.State().Domains.begin()->second.HeadCharacterId != 0, "Succession left an avoidable vacancy");
        for (std::size_t i = 0; i < DeityCount; ++i) {
            const auto& c = w.State().Characters.at(child);
            if (c.Birthright[i].SourceParentId == parent) Check(c.Birthright[i].Released, "Death failed to release inheritance");
            Check(c.Affinities[i].EarnedFavor == favors[i].EarnedFavor, "Protected character received autonomous favor");
            Check(w.State().Characters.at(protectedAdult).Affinities[i].EarnedFavor == adultFavors[i].EarnedFavor, "Protected adult performed autonomous activities");
        }
    }});
    cases.push_back({"BR-057 batching and save resume preserve exact autonomous outcomes", [] {
        auto a = Fixture(true); EvolutionRules p; p.Enabled = true; Accept(a.ConfigureEvolution("enable", p));
        auto b = a; Accept(a.AdvanceDays(1095));
        Accept(b.AdvanceDays(401)); Snapshot resumed; std::istringstream in(Serialized(b)); Accept(Load(in, resumed)); Accept(b.Restore(resumed));
        for (unsigned i = 0; i < 694; ++i) Accept(b.AdvanceDays(1));
        Check(Serialized(a) == Serialized(b), "Batch/cadence/resume altered outcomes");
    }});
    cases.push_back({"BR-057 schema seven migration preserves old behavior without enabling policies", [] {
        auto w = Fixture(true); auto text = Serialized(w);
        text.replace(0, std::string("BIRTHRIGHT_WORLD 11").size(), "BIRTHRIGHT_WORLD 7");
        text.erase(text.find("EVOLUTION_METADATA"));
        Snapshot s; std::istringstream in(text); Accept(Load(in, s));
        Check(s.Version == 11 && !s.Evolution.Policy.Enabled && s.Evolution.Areas.empty(), "Legacy policy changed");
        Check(s.Characters.size() == w.State().Characters.size(), "Migration lost genealogy");
    }});
    cases.push_back({"BR-057 appointments and promotion validate office life state and population", [] {
        auto w = Fixture(); EvolutionRules p; p.Enabled = true; Accept(w.ConfigureEvolution("enable", p));
        const auto domain = w.State().Domains.begin()->second;
        const auto head = w.State().Characters.at(domain.HeadCharacterId);
        const auto before = Serialized(w);
        Check(!w.AppointOffice("occupied", OfficeKind::Domain, domain.Key, head.Key), "Occupied office replaced");
        Check(!w.PromoteCandidate("existing", head.Key, head.AreaId, head.CultureId), "Existing ID replaced");
        Check(!w.PromoteCandidate("missing-culture", 99, head.AreaId, 9999), "Missing cohort promoted");
        Check(Serialized(w) == before, "Rejected office operations mutated state");
        const auto population = w.Inspect().Population;
        Accept(w.PromoteCandidate("promote", 99, head.AreaId, head.CultureId));
        Check(w.Inspect().Population == population - 1, "Promotion duplicated a cohort member");
        Accept(w.RecordDeath("vacate", head.Key, "test"));
        Check(!w.AppointOffice("dead", OfficeKind::Domain, domain.Key, head.Key), "Dead office candidate accepted");
        Accept(w.AppointOffice("appoint", OfficeKind::Domain, domain.Key, 99));
    }});
    cases.push_back({"BR-057 failed autonomous operation rolls back the whole time advance", [] {
        auto w = Fixture(); EvolutionRules p; p.Enabled = true; Accept(w.ConfigureEvolution("enable", p));
        auto s = w.State(); const Id actor = s.Characters.begin()->first;
        s.Events.push_back({"evolution/30/activity/" + std::to_string(actor), "test", 0, actor, 0, "Deliberate operation collision"});
        Accept(w.Restore(s)); const auto before = Serialized(w);
        Check(!w.AdvanceDays(365), "Conflicting operation accepted");
        Check(Serialized(w) == before, "Failed batch committed partial economics/time/actions");
    }});
    cases.push_back({"BR-057 scarcity has finite reserves and recovery is causal", [] {
        auto w = Fixture(); EvolutionRules p; p.Enabled = true; Accept(w.ConfigureEvolution("enable", p));
        auto s = w.State(); for (auto& a : s.Evolution.Areas) a.second.ProductiveCapacity = 0;
        Accept(w.Restore(s)); Accept(w.AdvanceDays(29));
        const auto unrest = w.Inspect().MeanUnrest; Check(unrest > 0.1, "Shortage produced no pressure");
        s = w.State(); for (auto& a : s.Evolution.Areas) a.second.ProductiveCapacity = 100000;
        Accept(w.Restore(s)); Accept(w.AdvanceDays(365));
        Check(w.Inspect().MeanUnrest < unrest, "Restored supply did not recover stability"); Accept(Validate(w.State()));
    }});
    cases.push_back({"BR-057 fractional food does not turn a small shortage into total starvation", [] {
        auto w = Fixture(); EvolutionRules p; p.Enabled = true; p.SeasonalAmplitude = 0; Accept(w.ConfigureEvolution("enable", p));
        auto s = w.State();
        for (auto& a : s.Areas) { for (auto& c : a.second.Cohorts) c.Population = 0; a.second.Cohorts.front().Population = 1; s.Evolution.Areas.at(a.first).ProductiveCapacity = 0.9; }
        const auto shortages = AdvanceEconomy(s);
        for (double shortage : shortages) Check(std::abs(shortage - 0.1) < 1e-9, "Fractional food rounded into total starvation");
        Accept(Validate(s));
    }});
    cases.push_back({"BR-057 three seeds retain bounded population and functioning offices for a century", [] {
        for (unsigned seed : {7U, 42U, 91U}) {
            auto w = Fixture(true); auto s = w.State(); s.Seed = seed; Accept(w.Restore(s));
            EvolutionRules p; p.Enabled = true; Accept(w.ConfigureEvolution("enable", p));
            Accept(w.AdvanceDays(36500)); std::size_t alive = 0, births = 0, deaths = 0, successions = 0;
            for (const auto& c : w.State().Characters) if (c.second.Alive) ++alive;
            for (const auto& e : w.State().Events) { if (e.Kind == "birth") ++births; if (e.Kind == "death") ++deaths; if (e.Kind == "succession") ++successions; }
            Check(alive > 0 && alive <= 8 && births > 1 && deaths > 0 && successions > 0, "Century had no turnover or exceeded capacity");
            Check(w.Inspect().Population > 0 && w.State().Domains.begin()->second.HeadCharacterId != 0, "World lost society or leadership");
            Accept(Validate(w.State()));
        }
    }});
    cases.push_back({"BR-056 long durations split campaign batches and preserve fractional time", [] {
        std::vector<GeographicArea> graph; Accept(MakeGridGeography(1,1,42,graph));
        EmpireGeneration generation; generation.EmpireCount = 1; generation.RegionsPerEmpire = 1;
        generation.Seeds = {{1,Deity::Harmony}};
        Rules rules; rules.DailyBirthRate = 0; rules.DailyDeathRate = 0; rules.DailyStewardFavor = 0;
        Snapshot state; Accept(GenerateEmpires(graph,generation,rules,state)); state.DayFraction = 0.5;
        World world; Accept(world.Restore(state)); Scenario s; s.Days = 36501; s.ReportEveryDays = 36501;
        std::vector<std::uint32_t> reports; std::ostringstream out;
        Accept(Simulate(world,s,out,[&](const Observation& o,const Snapshot&) { reports.push_back(o.ElapsedDays); return Result{true,"",""}; }));
        Check(reports == std::vector<std::uint32_t>{0,36501} && world.State().Day == 36501 && world.State().DayFraction == 0.5,
            "Batch split lost time or emitted extra reports");
        state = world.State(); state.Day = 99999999; Accept(world.Restore(state)); const auto before = Serialized(world);
        s.Days = 1; std::ostringstream invalid; Check(!Simulate(world,s,invalid) && Serialized(world) == before && invalid.str().empty(), "Clock overflow was not rejected upfront");
    }});
    cases.push_back({"BR-056 day-zero and same-day actions precede checkpoint assertions", [] {
        auto world = Fixture(); Scenario s; s.Days = 0;
        s.Actions = {{0,"start-family",[](World& w) { return SeedBirthrightFamilies(w,"BR-056-zero-family"); }},
            {0,"award",[](World& w) { return w.AwardFavor("BR-056-zero-favor",w.State().Characters.rbegin()->first,Deity::Harmony,100,"scripted test"); }}};
        s.Conditions = {{"bearer","final","livingBirthrightNpcs","eq",1}};
        std::ostringstream out; Accept(Simulate(world,s,out));
        Check(world.State().Characters.rbegin()->second.Affinities[1].EarnedFavor == 100 && world.State().Day == 0, "Actions ran out of order");
    }});
    cases.push_back({"BR-056 exact checkpoints, partial final interval and zero duration", [] {
        auto world = Fixture(); Scenario s; s.Days = 25; s.ReportEveryDays = 10;
        std::vector<std::uint32_t> days; std::ostringstream report;
        Accept(Simulate(world, s, report, [&](const Observation& o, const Snapshot&) {
            days.push_back(o.ElapsedDays); Check(o.Final == (o.ElapsedDays == 25), "Wrong final marker"); return Result{true, "", ""};
        }));
        Check(days == std::vector<std::uint32_t>{0,10,20,25}, "Wrong checkpoint cadence");
        s.Days = 0; days.clear(); std::ostringstream zero;
        Accept(Simulate(world, s, zero, [&](const Observation& o, const Snapshot&) {
            Check(o.Final && o.ElapsedDays == 0, "Zero-day baseline is final"); days.push_back(0); return Result{true,"",""};
        }));
        Check(days.size() == 1 && world.State().Day == 25, "Zero duration advanced the clock");
    }});
    cases.push_back({"BR-056 cadence cannot alter seeded simulation and save/resume", [] {
        auto a = Fixture(true), b = Fixture(true); Scenario s; s.Days = 40; s.ReportEveryDays = 7;
        std::ostringstream x, y; Accept(Simulate(a,s,x)); s.ReportEveryDays = 19; Accept(Simulate(b,s,y));
        Check(Serialized(a) == Serialized(b), "Reporting cadence changed authority");
        auto c = Fixture(true); Accept(c.AdvanceDays(40)); Check(Serialized(a) == Serialized(c), "Observer changed simulation");
        World resumed; Snapshot checkpoint; std::istringstream input(Serialized(a)); Accept(Load(input, checkpoint)); Accept(resumed.Restore(checkpoint));
        s.Days = 3; std::ostringstream tail; Accept(Simulate(resumed,s,tail)); Accept(c.AdvanceDays(3));
        Check(Serialized(resumed) == Serialized(c), "Observed resume diverged");
        Check(tail.str().find("\"day\":40") != std::string::npos, "Resume lost absolute day");
        auto repeat = Fixture(true); s.Days = 40; s.ReportEveryDays = 7; std::ostringstream z; Accept(Simulate(repeat,s,z));
        Check(x.str() == z.str(), "Same seed/config did not reproduce timeline");
    }});
    cases.push_back({"BR-056 assertions fail with scenario/day/condition evidence", [] {
        auto world = Fixture(); Scenario s; s.Id = "BR-056-assert"; s.Days = 2; s.ReportEveryDays = 1;
        s.Conditions = {{"stable-empires","every","empires","eq",1}, {"impossible","final","population","lt",0}};
        std::ostringstream out; const auto r = Simulate(world,s,out);
        Check(!r && r.Code == "condition_failed" && r.Detail.find("condition=impossible") != std::string::npos &&
            r.Detail.find("day=2") != std::string::npos && r.Detail.find(s.Id) != std::string::npos, "Failure lacks actionable context");
        Check(out.str().find("\"passed\":false") != std::string::npos, "Failed checkpoint missing");
        s.Conditions.back().Value = 1e12; std::ostringstream pass; Accept(Simulate(world,s,pass));
    }});
    cases.push_back({"BR-056 invalid configuration and unknown metrics reject before mutation", [] {
        auto world = Fixture(); const auto before = Serialized(world); Scenario s;
        std::ostringstream out; s.ReportEveryDays = 0; Check(!Simulate(world,s,out), "Zero interval accepted");
        s.ReportEveryDays = 1; s.Version = 2; Check(!Simulate(world,s,out), "Unknown version accepted"); s.Version = 1;
        s.Conditions = {{"typo","every","missing","eq",0}}; Check(!Simulate(world,s,out), "Unknown metric accepted");
        s.Conditions = {{"nan","every","empires","eq",std::numeric_limits<double>::quiet_NaN()}}; Check(!Simulate(world,s,out), "NaN accepted");
        s.Conditions.clear(); s.Actions = {{s.Days+1,"too-late",[](World&) { return Result{true,"",""}; }}};
        Check(!Simulate(world,s,out), "Out-of-range action accepted");
        Check(out.str().empty() && Serialized(world) == before, "Invalid scenario emitted data or changed state");
    }});
    cases.push_back({"BR-056 birthright aging, adulthood, death and reserve release", [] {
        auto world = Fixture(true); auto state = world.State(); state.Policy.AdultAgeDays = 2; Accept(world.Restore(state));
        const auto child = world.State().Characters.rbegin()->first;
        const auto parent = world.State().Characters.at(child).Parents[0];
        const auto initialPower = Power(world.State().Characters.at(child).Affinities[1], state.Policy);
        Scenario s; s.Days = 3; s.ReportEveryDays = 1;
        s.Actions = {{1,"source-death",[parent](World& w) { return w.RecordDeath("BR-056-source-death",parent,"scripted test"); }}};
        std::ostringstream out;
        Accept(Simulate(world,s,out,[&](const Observation& o, const Snapshot& now) {
            if (o.ElapsedDays == 1) {
                Check(now.Characters.at(child).Birthright[1].Released, "Death failed to release reserve");
                Check(o.Metrics.at("vacantEmpireHeads") == 1 && o.Metrics.at("destroyedEmpires") == 0, "Vacancy mistaken for destruction");
                Check(o.ChangedBirthrightNpcs == std::vector<Id>{child}, "Grant release not observed");
            }
            return Result{true,"",""};
        }));
        Check(Power(world.State().Characters.at(child).Affinities[1], state.Policy) > initialPower &&
            world.State().Characters.at(child).Affinities[1].EarnedFavor == 2, "Adult daily favor missing");
        Check(out.str().find("\"ageDays\":3") != std::string::npos && out.str().find("BR-056-source-death") != std::string::npos, "NPC details/event missing");
        s.Days = 1; s.Actions = {{1,"heir-death",[child](World& w) { return w.RecordDeath("BR-056-heir-death",child,"scripted test"); }}};
        std::ostringstream dead; Accept(Simulate(world,s,dead,[&](const Observation& o, const Snapshot&) {
            if (o.Final) Check(o.LostBirthrightNpcs == std::vector<Id>{child} && o.Metrics.at("livingBirthrightNpcs") == 0, "NPC death missing");
            return Result{true,"",""};
        }));
    }});
    cases.push_back({"BR-056 scheduled birth runs at its day regardless of report cadence", [] {
        auto world = Fixture(); Scenario s; s.Days = 9; s.ReportEveryDays = 9;
        s.Actions = {{3,"family",[](World& w) { return SeedBirthrightFamilies(w,"BR-056-later-family"); }}};
        std::ostringstream out; Accept(Simulate(world,s,out,[&](const Observation& o, const Snapshot&) {
            if (o.Final) Check(o.NewBirthrightNpcs.size() == 1 && o.Metrics.at("newNpcs") == 1 && o.Metrics.at("events") == 2, "Interval birth missing");
            return Result{true,"",""};
        }));
        Check(world.State().Characters.rbegin()->second.BornDay == 3, "Action rounded to report boundary");
    }});
    cases.push_back({"BR-056 failed callbacks/actions and report writes propagate", [] {
        auto world = Fixture(); Scenario s; s.Days = 3;
        s.Actions = {{1,"invalid-death",[](World& w) { return w.RecordDeath("BR-056-missing",999,"test"); }}};
        std::ostringstream out; const auto r = Simulate(world,s,out);
        Check(!r && r.Detail.find("operation=invalid-death") != std::string::npos, "Action rejection swallowed");
        s.Actions.clear(); std::ostringstream callback; Check(!Simulate(world,s,callback,[](const Observation&,const Snapshot&) { return Result{false,"custom_assertion","test invariant"}; }), "Custom assertion ignored");
        std::ostringstream broken; broken.setstate(std::ios::badbit); Check(!Simulate(world,s,broken), "Write failure ignored");
    }});
    cases.push_back({"BR-056 empire creation/removal uses IDs rather than head life state", [] {
        auto world = Fixture(); Scenario s; s.Days = 1; s.ReportEveryDays = 1;
        // Test-only registry replacement: no political-evolution policy is added to World.
        s.Actions = {{1,"rename-domain-id",[](World& w) {
            auto next = w.State(); auto domain = next.Domains.at(1); next.Domains.clear(); domain.Key = 99; next.Domains.emplace(99,domain);
            for (auto& r : next.Regions) r.second.DomainId = 99;
            for (auto& c : next.Cultures) if (c.second.OriginDomainId == 1) c.second.OriginDomainId = 99;
            for (auto& a : next.Areas) {
                for (auto& id : a.second.Claimants) if (id == 1) id = 99;
                for (auto& i : a.second.Influences) if (i.DomainId == 1) i.DomainId = 99;
                for (auto& i : a.second.TargetInfluences) if (i.DomainId == 1) i.DomainId = 99;
            }
            return w.Restore(next);
        }}};
        std::ostringstream out; Accept(Simulate(world,s,out,[](const Observation& o,const Snapshot&) {
            if (o.Final) Check(o.CreatedEmpires == std::vector<Id>{99} && o.DestroyedEmpires == std::vector<Id>{1}, "Registry changes hidden by unchanged count");
            return Result{true,"",""};
        }));
    }});
}
}
