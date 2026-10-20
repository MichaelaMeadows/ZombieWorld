#include "Simulation.h"
#include "EmpireGeneration.h"
#include "WorldMarket.h"
#include "WorldEvolution.h"
#include <limits>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace Birthright::Campaign::Testing
{
namespace
{
void Check(bool ok,const std::string& why) { if(!ok) throw std::runtime_error(why); }
void Accept(const Result& r) { Check(r.Accepted,r.Code+": "+r.Detail); }
std::string Saved(const World& w) { std::ostringstream o; Accept(Save(w.State(),o)); return o.str(); }
World Fixture(unsigned seed=42,unsigned regions=2) {
    const unsigned width=seed==7?8:seed==91?16:12;
    std::vector<GeographicArea> graph; Accept(MakeGridGeography(width,96/width,seed,graph));
    EmpireGeneration g; g.EmpireCount=2; g.RegionsPerEmpire=regions; g.ClaimRadius=100; g.InfluenceRadius=100; g.Seeds={{1,Deity::Harmony},{96,Deity::Strife}};
    Rules rules; rules.DailyBirthRate=rules.DailyDeathRate=0; Snapshot s; Accept(GenerateEmpires(graph,g,rules,s));
    s.Connectivity.MigrationDailyFraction=0; for(auto& c:s.Characters) c.second.BornDay=-s.Policy.AdultAgeDays-1;
    World w; Accept(w.Restore(s)); EvolutionRules e; e.Enabled=true; e.AnnualBirthChance=0; Accept(w.ConfigureEvolution("evolution",e));
    PoliticsRules p; p.Enabled=true; p.AnnualWarChance=0; Accept(w.ConfigurePolitics("politics",p));
    s=w.State(); for(auto& d:s.Evolution.Domains) d.second.Treasury=d.second.Revenue=10000; Accept(w.Restore(s));
    MarketRules m; m.Enabled=true; Accept(w.ConfigureMarket("market",m)); return w;
}
double Produced(const World& w) { double n=0; for(const auto& a:w.State().Evolution.Areas) n+=a.second.Produced; return n; }
double Coverage(const World& w) { double n=0; for(const auto& r:w.State().Market.Regions) n+=r.second.FoodCoverage; return n/w.State().Market.Regions.size(); }
std::uint64_t Soldiers(const World& w) { std::uint64_t n=0; for(const auto& a:w.State().Politics.Armies) n+=a.second.Soldiers; return n; }
void War(World& w) { const auto& s=w.State(); Id target=0; for(const auto& r:s.Regions) if(r.second.DomainId==2) { target=r.first; break; } Accept(w.DeclareWar("war",s.Domains.at(1).HeadCharacterId,target)); Accept(w.RaiseArmy("army1",w.State().Domains.at(1).HeadCharacterId,1)); Accept(w.RaiseArmy("army2",w.State().Domains.at(2).HeadCharacterId,1)); }
void Sample(const std::string& scenario,const World& w) {
    std::ofstream o("Artifacts/World/br-061-impact.jsonl",std::ios::app);
    const auto& m=w.State().Market; o<<"{\"scenario\":\""<<scenario<<"\",\"day\":"<<w.State().Day<<",\"population\":"<<w.Inspect().Population<<",\"foodCoverage\":"<<Coverage(w)<<",\"produced\":"<<Produced(w)<<",\"trade\":"<<m.FoodTraded<<",\"investment\":"<<m.Investment<<",\"inherited\":"<<m.Inherited<<",\"armyFood\":"<<m.ArmyFood<<",\"money\":"<<MarketMoney(w.State())<<"}\n";
}
}
void AddMarketTests(std::vector<std::pair<std::string,std::function<void()>>>& cases) {
    cases.push_back({"BR-067 actual income builds affordable reserves without stopping services",[] {
        auto w=Fixture();auto s=w.State();
        for(auto& d:s.Evolution.Domains){CreditMarketServices(s,d.first,d.second.Treasury);d.second.Spent+=d.second.Treasury;d.second.Treasury=0;}
        for(const auto& c:s.Characters)s.Evolution.ProtectedCharacters.push_back(c.first);
        Accept(w.Restore(s));Accept(w.AdvanceDays(180));double treasury=0,services=0;
        for(const auto& d:w.State().Evolution.Domains)treasury+=d.second.Treasury;
        for(const auto& r:w.State().Market.Regions)services+=r.second.ServiceReceipts-s.Market.Regions.at(r.first).ServiceReceipts;
        Check(treasury>w.State().Market.Taxes*0.07 && services>treasury,"Actual receipts did not build modest reserves while keeping services flowing");
        Accept(Validate(w.State()));
    }});
    cases.push_back({"BR-067 budget quotes respect income shocks zero cash commitments and surplus",[] {
        MarketRules p;
        auto normal=QuotePublicBudget(p,500,10,10,0,0);
        Check(normal.Services>5 && normal.Services<10,"Services consume all recurring income");
        auto shock=QuotePublicBudget(p,500,0,10,0,0);
        Check(shock.Services>0 && shock.Services<=normal.Services && shock.Emergency==shock.Services,"Income shock bypasses reserve buffer");
        Check(QuotePublicBudget(p,0,0,0,0,0).Services==0,"Zero income/cash invents funding");
        auto war=QuotePublicBudget(p,105,10,10,4,100);
        Check(war.Services<=5 && war.ReserveTarget>=100 && war.Desired>war.Services,"Services consume committed military reserve");
        Check(QuotePublicBudget(p,10000,0,0,0,0).Services>0,"No-income surplus is trapped forever");
        for(double cash:{0.0,1.0,50.0,1000.0})for(double income:{0.0,5.0,50.0}){
            const auto q=QuotePublicBudget(p,cash,income,10,2,20);
            Check(q.Services>=0 && q.Services<=std::max(0.0,cash-20) && q.Emergency+q.Surplus<=q.Services+1e-9,"Unaffordable quote/release attribution");
        }
    }});
    cases.push_back({"BR-067 actual receipt history survives shocks military obligations and resume",[] {
        auto w=Fixture();War(w);Accept(w.AdvanceDays(10));
        double commitments=0;for(const auto& b:w.State().Market.Budgets)commitments+=b.second.MilitaryReserve;
        Check(commitments>0,"Active armies omitted from budget commitments");
        auto s=w.State();s.Market.Policy.TaxRate=0;Accept(w.Restore(s));auto split=w;
        Accept(w.AdvanceDays(70));Accept(split.AdvanceDays(11));Snapshot loaded;std::istringstream input(Saved(split));Accept(Load(input,loaded));Accept(split.Restore(loaded));
        for(int i=11;i<70;++i)Accept(split.AdvanceDays(1));Check(Saved(w)==Saved(split),"Budget depends on partition/save resume");
        Check(w.State().Market.Taxes==s.Market.Taxes,"Zero tax rate generated budget revenue");
        for(const auto& b:w.State().Market.Budgets)Check(b.second.ReceiptsToday==0 && b.second.SmoothedReceipts<s.Market.Budgets.at(b.first).SmoothedReceipts,"Income shock did not reach budget");
        Accept(Validate(w.State()));
    }});
    cases.push_back({"BR-067 v3 migration preserves money and starts honest receipt history",[] {
        auto w=Fixture();Accept(w.AdvanceDays(12));auto text=Saved(w);text.replace(text.find("MARKET_METADATA 4"),17,"MARKET_METADATA 3");text.erase(text.find("MARKET_BUDGETS"));text.replace(0,19,"BIRTHRIGHT_WORLD 10");
        Snapshot loaded;std::istringstream input(text);Accept(Load(input,loaded));
        Check(loaded.Market.Policy.Version==4 && loaded.Market.BudgetStartedDay==12 && loaded.Market.BudgetOpeningTaxes==loaded.Market.Taxes && loaded.Market.Budgets.empty(),"Invented historical receipts");
        Check(MarketMoney(loaded)==MarketMoney(w.State()),"Migration changed cash");Accept(w.Restore(loaded));Accept(w.AdvanceDays(1));
        const auto original=Saved(w);for(double share:{-1.0,1.1,std::numeric_limits<double>::quiet_NaN()}){auto bad=w.State();bad.Market.Policy.ServiceIncomeShare=share;Check(!w.Restore(bad) && Saved(w)==original,"Invalid budget policy was not atomic");}
        auto bad=w.State();bad.Market.Budgets.begin()->second.TaxReceipts+=1;Check(!w.Restore(bad),"Invented tax receipt accepted");
    }});
    cases.push_back({"BR-067 varied receipt budgets retain circulation and build reserves",[] {
        std::ofstream out("Artifacts/World/br-067-controlled.jsonl");
        for(unsigned seed:{7U,42U,91U}){
            auto on=Fixture(seed,3);auto s=on.State();
            for(auto& d:s.Evolution.Domains){CreditMarketServices(s,d.first,d.second.Treasury);d.second.Spent+=d.second.Treasury;d.second.Treasury=0;}
            for(const auto& c:s.Characters)s.Evolution.ProtectedCharacters.push_back(c.first);
            Accept(on.Restore(s));auto off=on;s.Market.Policy.ReceiptBudgets=false;Accept(off.Restore(s));
            Accept(on.AdvanceDays(730));Accept(off.AdvanceDays(730));double a=0,b=0;for(const auto& d:on.State().Evolution.Domains)a+=d.second.Treasury;for(const auto& d:off.State().Evolution.Domains)b+=d.second.Treasury;
            Check(a>b && on.State().Market.Wages>off.State().Market.Wages*0.8 && Coverage(on)>0.7,"Reserves failed to build or fixture circulation collapsed");
            out<<"{\"seed\":"<<seed<<",\"days\":730,\"receiptTreasury\":"<<a<<",\"previousTreasury\":"<<b<<",\"receiptWages\":"<<on.State().Market.Wages<<",\"previousWages\":"<<off.State().Market.Wages<<",\"receiptFoodCoverage\":"<<Coverage(on)<<",\"previousFoodCoverage\":"<<Coverage(off)<<"}\n";
        }
    }});
    cases.push_back({"BR-063 isolated annual retention and tiny stocks",[] {
        Snapshot s;s.Evolution.Policy.Enabled=true;s.Evolution.Areas[1].FoodStored=s.Evolution.Areas[1].Produced=1000;
        s.Evolution.Areas[2].FoodStored=s.Evolution.Areas[2].Produced=1e-200;
        for(int day=0;day<365;++day)DecayFoodReserves(s);
        Check(std::abs(s.Evolution.Areas.at(1).FoodStored-875)<1e-9,"Annual compounded retention differs");
        Check(std::abs(s.Evolution.Areas.at(2).FoodStored/1e-200-0.875)<1e-12,"Tiny stocks rounded away");
        for(int day=0;day<365;++day)DecayFoodReserves(s);
        const auto& a=s.Evolution.Areas.at(1);Check(std::abs(a.FoodStored-765.625)<1e-9 && std::abs(a.Spoiled+a.FoodStored-1000)<1e-9 && a.Wasted==a.Spoiled,"Two-year decay ledger differs");
        s.Evolution.Policy.AnnualFoodSpoilage=0;const auto stored=a.FoodStored;DecayFoodReserves(s);Check(a.FoodStored==stored,"Zero rate decays");
    }});
    cases.push_back({"BR-063 daily partition resume and both economic paths",[] {
        for(bool market:{false,true}) {
            auto w=Fixture();auto s=w.State();if(!market){s.Market={};s.Politics={};}
            for(auto& a:s.Evolution.Areas){a.second.FoodStored+=1000;a.second.Produced+=1000;}
            Accept(w.Restore(s));auto split=w;Accept(w.AdvanceDays(730));Accept(split.AdvanceDays(117));
            Snapshot loaded;std::istringstream input(Saved(split));Accept(Load(input,loaded));Accept(split.Restore(loaded));
            for(unsigned i=117;i<730;++i)Accept(split.AdvanceDays(1));
            Check(Saved(w)==Saved(split),"Decay depends on partition or resume");double loss=0;for(const auto& a:w.State().Evolution.Areas)loss+=a.second.Spoiled;Check(loss>0,"Economic path skipped decay");Accept(Validate(w.State()));
        }
    }});
    cases.push_back({"BR-063 migration and invalid decay policy are atomic",[] {
        auto w=Fixture();Accept(w.AdvanceDays(12));const auto original=Saved(w);auto text=original;
        text.replace(text.find("EVOLUTION_METADATA 3"),20,"EVOLUTION_METADATA 1");
        const auto start=text.find("FOOD_DECAY"),end=text.find("POLITICS_METADATA",start);Check(end!=std::string::npos,"Missing fixture boundary");text.erase(start,end-start);
        Snapshot migrated;std::istringstream input(text);Accept(Load(input,migrated));Check(migrated.Evolution.Policy.Version==3 && migrated.Evolution.SpoilageStartedDay==12,"Legacy decay epoch missing");
        for(const auto& a:migrated.Evolution.Areas)Check(a.second.Spoiled==0 && a.second.FoodStored==w.State().Evolution.Areas.at(a.first).FoodStored,"Historical decay invented");
        for(double rate:{-0.1,1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){auto bad=w.State();bad.Evolution.Policy.AnnualFoodSpoilage=rate;Check(!w.Restore(bad) && Saved(w)==original,"Invalid policy mutated authority");}
        auto bad=w.State();bad.Evolution.Areas.begin()->second.Spoiled=bad.Evolution.Areas.begin()->second.Wasted+1;Check(!w.Restore(bad),"Decay exceeds total waste");
    }});
    cases.push_back({"BR-063 food decay has bounded full-demography economic impact",[] {
        auto on=Fixture();auto s=on.State();s.Policy.DailyBirthRate=0.0001;s.Policy.DailyDeathRate=0.00008;s.Connectivity.MigrationDailyFraction=0.001;Accept(on.Restore(s));auto off=on;s.Evolution.Policy.AnnualFoodSpoilage=0;Accept(off.Restore(s));
        const auto initial=on.Inspect().Population;Accept(on.AdvanceDays(3650));Accept(off.AdvanceDays(3650));double decay=0,noDecay=0;
        for(const auto& a:on.State().Evolution.Areas)decay+=a.second.Spoiled;for(const auto& a:off.State().Evolution.Areas)noDecay+=a.second.Spoiled;
        Check(decay>0 && noDecay==0 && on.Inspect().Population>initial/2 && on.Inspect().Population<initial*2,"Decay lacks impact or destabilizes fixture");
        std::ofstream out("Artifacts/World/br-063-impact.json");out<<"{\"days\":3650,\"onPopulation\":"<<on.Inspect().Population<<",\"offPopulation\":"<<off.Inspect().Population<<",\"spoiled\":"<<decay<<",\"offSpoiled\":"<<noDecay<<"}\n";
    }});

    cases.push_back({"BR-061 regional storage preserves concentrated food within aggregate capacity",[] {
        auto w=Fixture(42,1);auto s=w.State();const Id region=s.Regions.begin()->first;Id cell=0;double n=0;
        for(const auto& a:s.Areas)if(a.second.RegionId==region){if(!cell)cell=a.first;for(const auto& c:a.second.Cohorts)n+=static_cast<double>(c.Population);}
        const double supply=n*10;s.Evolution.Areas.at(cell).FoodStored+=supply;s.Evolution.Areas.at(cell).Produced+=supply;Accept(w.Restore(s));
        double beforeWaste=0;for(const auto& a:s.Evolution.Areas)beforeWaste+=a.second.Wasted;Accept(w.AdvanceDays(1));double waste=0,stock=0;for(const auto& a:w.State().Evolution.Areas){waste+=a.second.Wasted;if(w.State().Areas.at(a.first).RegionId==region)stock+=a.second.FoodStored;}
        Check(waste-beforeWaste<supply*0.05 && stock>supply*0.8,"Regional food wasted despite available local storage");Accept(Validate(w.State()));
    }});
    cases.push_back({"BR-061 distinguish unavailable food from unaffordable food",[] {
        auto cashWorld=Fixture();auto s=cashWorld.State();for(auto& a:s.Evolution.Areas)a.second.ProductiveCapacity*=3;
        for(auto& row:s.Market.Regions){auto& d=s.Evolution.Domains.at(s.Regions.at(row.first).DomainId);double cash=row.second.HouseholdCash+row.second.ProducerCash;d.Treasury+=cash;d.Revenue+=cash;row.second.HouseholdCash=row.second.ProducerCash=0;}
        Accept(cashWorld.Restore(s));Accept(cashWorld.AdvanceDays(1));double missing=0,cash=0;for(const auto& a:cashWorld.State().Market.Areas){missing+=a.second.SupplyShortagePersonDays;cash+=a.second.CashShortagePersonDays;}Check(cash>0 && missing<cash*0.01,"Cash shortage classified as missing goods");
        auto supplyWorld=Fixture();s=supplyWorld.State();for(auto& a:s.Evolution.Areas){a.second.ProductiveCapacity=0;a.second.Wasted+=a.second.FoodStored;a.second.FoodStored=0;}Accept(supplyWorld.Restore(s));Accept(supplyWorld.AdvanceDays(1));missing=cash=0;for(const auto& a:supplyWorld.State().Market.Areas){missing+=a.second.SupplyShortagePersonDays;cash+=a.second.CashShortagePersonDays;}Check(missing>0 && cash<missing*0.01,"Physical shortage classified as insufficient money");
    }});
    cases.push_back({"BR-061 recovered checkpoints retain intervening hardship",[] {
        auto w=Fixture();auto s=w.State();for(auto& a:s.Evolution.Areas){a.second.ProductiveCapacity=0;a.second.Wasted+=a.second.FoodStored;a.second.FoodStored=0;}Accept(w.Restore(s));
        Scenario scenario;scenario.Id="BR-061-hidden-hardship";scenario.Days=60;scenario.ReportEveryDays=60;
        scenario.Actions.push_back({10,"restore-harvest",[](World& world){auto state=world.State();for(auto& a:state.Evolution.Areas)a.second.ProductiveCapacity=state.Market.Areas.at(a.first).LaborCapacity*3;return world.Restore(state);}});
        scenario.Conditions={{"hardship-recorded","final","intervalSupplyShortagePersonDays","gt",0},{"recovery","final","weightedFoodCoverage","gt",0.95}};
        std::ostringstream output;Accept(Simulate(w,scenario,output));Check(output.str().find("economyRegions")!=std::string::npos,"Regional diagnostics missing");
    }});
    cases.push_back({"BR-061 diagnostic intervals reconcile across report cadence",[] {
        auto w=Fixture();auto s=w.State();s.Policy.DailyBirthRate=0.0001;s.Policy.DailyDeathRate=0.00008;for(auto& r:s.Market.Regions){r.second.HarvestFactor=0.5;r.second.ShockUntil=45;}Accept(w.Restore(s));auto other=w;
        Scenario scenario;scenario.Id="BR-061-cadence";scenario.Days=90;scenario.ReportEveryDays=10;std::ostringstream output;double summed=0;
        Accept(Simulate(w,scenario,output,[&](const Observation& o,const Snapshot&){summed+=o.Metrics.at("intervalSupplyShortagePersonDays");return Result{true,"accepted",""};}));
        scenario.ReportEveryDays=90;double single=0;Accept(Simulate(other,scenario,output,[&](const Observation& o,const Snapshot&){single+=o.Metrics.at("intervalSupplyShortagePersonDays");return Result{true,"accepted",""};}));
        Check(std::abs(single-summed)<1e-7*std::max(1.0,single) && Saved(w)==Saved(other),"Reports change hardship totals or world state");
        std::uint64_t births=0,deaths=0;for(const auto& a:w.State().Market.Areas){births+=a.second.DemographicBirths;deaths+=a.second.DemographicDeaths;}
        Check(births>0 && deaths>0,"Demographic diagnostics empty");
    }});
    cases.push_back({"BR-061 three full-demography worlds remain viable through fifty years",[] {
        for(unsigned seed:{7U,42U,91U}) {auto w=Fixture(seed,3);auto s=w.State();s.Policy.DailyBirthRate=0.0001;s.Policy.DailyDeathRate=0.00008;s.Connectivity.MigrationDailyFraction=0.001;s.Evolution.Policy.AnnualBirthChance=0.22;s.Politics.Policy.AnnualWarChance=0.2;Accept(w.Restore(s));const auto initial=w.Inspect().Population;Accept(w.AdvanceDays(18250));Accept(Validate(w.State()));double nutrition=0;for(const auto& a:w.State().Market.Areas)nutrition+=a.second.Nutrition;
            Check(w.Inspect().Population>initial/3 && w.Inspect().Population<initial*3 && nutrition/w.State().Market.Areas.size()>0.5,"Late-generation viability band exceeded");Sample("BR-061-fifty-year-seed-"+std::to_string(seed),w);
        }
    }});
    cases.push_back({"BR-061 v2 migration preserves health and begins a fresh diagnostic window",[] {
        auto w=Fixture();Accept(w.AdvanceDays(20));auto text=Saved(w);text.replace(text.find("MARKET_METADATA 4"),17,"MARKET_METADATA 2");text.erase(text.find("MARKET_DIAGNOSTICS"));text.replace(0,19,"BIRTHRIGHT_WORLD 10");Snapshot state;std::istringstream input(text);Accept(Load(input,state));Check(state.Market.DiagnosticsStartedDay==state.Day && state.Market.Policy.Version==4,"Missing diagnostic migration epoch");
        for(const auto& a:state.Market.Areas)Check(a.second.Nutrition==w.State().Market.Areas.at(a.first).Nutrition && a.second.SupplyShortagePersonDays==0,"Migration fabricated history or lost health");Accept(w.Restore(state));const auto before=Saved(w);auto invalid=state;invalid.Market.Areas.begin()->second.CashShortagePersonDays=-1;Check(!w.Restore(invalid),"Negative exposure accepted");Check(Saved(w)==before,"Diagnostic rejection changed state");
    }});

    cases.push_back({"BR-060 public services follow residents rather than region count",[] {
        auto w=Fixture(42,3); auto s=w.State(); Id large=0,small=0; double maximum=0,minimum=1e20;
        for(const auto& r:s.Regions) if(r.second.DomainId==1) { double n=0; for(const auto& a:s.Areas) if(a.second.RegionId==r.first) for(const auto& c:a.second.Cohorts)n+=static_cast<double>(c.Population); if(n>maximum){maximum=n;large=r.first;} if(n<minimum){minimum=n;small=r.first;} }
        Check(maximum>minimum,"Fixture needs unequal regions"); const double a=s.Market.Regions.at(large).ProducerCash,b=s.Market.Regions.at(small).ProducerCash;
        CreditMarketServices(s,1,100); Check(s.Market.Regions.at(large).ProducerCash-a>1.1*(s.Market.Regions.at(small).ProducerCash-b),"Services ignore regional population");
    }});
    cases.push_back({"BR-060 a single missed harvest day is buffered before starvation mortality",[] {
        auto w=Fixture(); auto s=w.State(); for(auto& a:s.Evolution.Areas){a.second.Wasted+=a.second.FoodStored;a.second.FoodStored=0;a.second.ProductiveCapacity=0;}
        Accept(w.Restore(s));const auto population=w.Inspect().Population;Accept(w.AdvanceDays(1));
        Check(w.Inspect().Population>population*0.9995,"Immediate linear starvation after one day");
    }});
    cases.push_back({"BR-060 durable production avoids persistent unwanted surplus",[] {
        auto w=Fixture();Accept(w.AdvanceDays(1825));double produced=0,wasted=0;for(const auto& r:w.State().Market.Regions){produced+=r.second.Produced[1];wasted+=r.second.Wasted[1];}
        Check(produced>0 && wasted<produced*0.4,"Most tools are produced only to be discarded");
    }});


    cases.push_back({"BR-060 public reserves recirculate money into stressed regional wages",[] {
        auto improved=Fixture(); auto s=improved.State();
        for(auto& row:s.Market.Regions) { auto& d=s.Evolution.Domains.at(s.Regions.at(row.first).DomainId); const double cash=row.second.HouseholdCash+row.second.ProducerCash; row.second.HouseholdCash=row.second.ProducerCash=0; d.Treasury+=cash;d.Revenue+=cash; }
        Accept(improved.Restore(s)); auto baseline=improved;s.Market.Policy.ResponsiveBudgets=false;Accept(baseline.Restore(s));
        Accept(improved.AdvanceDays(365));Accept(baseline.AdvanceDays(365));
        double wages=0,oldWages=0;for(const auto& r:improved.State().Market.Regions)wages+=r.second.WageCoverage;for(const auto& r:baseline.State().Market.Regions)oldWages+=r.second.WageCoverage;
        double unpaid=0,oldUnpaid=0,cashGap=0,oldCashGap=0;
        for(const auto& r:improved.State().Market.Regions)unpaid+=r.second.UnpaidWages;for(const auto& r:baseline.State().Market.Regions)oldUnpaid+=r.second.UnpaidWages;
        for(const auto& a:improved.State().Market.Areas)cashGap+=a.second.CashShortagePersonDays;for(const auto& a:baseline.State().Market.Areas)oldCashGap+=a.second.CashShortagePersonDays;
        Check(wages>=oldWages && unpaid<oldUnpaid && cashGap<oldCashGap,"Budget response did not reduce accumulated unpaid wages/food hardship"); Sample("BR-060-responsive-budget",improved);Sample("BR-060-fixed-budget",baseline);
    }});
    cases.push_back({"BR-060 prolonged deprivation damages nutrition and adequate supply recovers it",[] {
        auto w=Fixture();auto s=w.State();for(auto& r:s.Market.Regions){r.second.HarvestFactor=0.1;r.second.ShockUntil=150;} for(auto& a:s.Evolution.Areas){a.second.Wasted+=a.second.FoodStored;a.second.FoodStored=0;}
        Accept(w.Restore(s));Accept(w.AdvanceDays(5));double early=0;for(const auto& a:w.State().Market.Areas)early+=a.second.Nutrition;
        Accept(w.AdvanceDays(100));double stressed=0;for(const auto& a:w.State().Market.Areas)stressed+=a.second.Nutrition;
        Check(stressed<early,"Persistent shortage had no accumulated effect");Accept(w.AdvanceDays(500));double recovered=0;for(const auto& a:w.State().Market.Areas)recovered+=a.second.Nutrition;
        Check(recovered>stressed && w.Inspect().Population>0,"Nutrition did not recover with restored supply");
    }});
    cases.push_back({"BR-060 demand-aware investment avoids surplus regions",[] {
        auto responsive=Fixture();auto s=responsive.State();for(auto& a:s.Evolution.Areas)a.second.ProductiveCapacity*=3;Accept(responsive.Restore(s));auto fixed=responsive;s.Market.Policy.DemandProduction=false;Accept(fixed.Restore(s));
        Accept(responsive.AdvanceDays(1825));Accept(fixed.AdvanceDays(1825));
        Check(responsive.State().Market.Investment<fixed.State().Market.Investment,"Autonomous investment ignored surplus production");
    }});
    cases.push_back({"BR-060 market v1 migration starts neutral nutrition without inventing demand",[] {
        auto w=Fixture();Accept(w.AdvanceDays(30));auto saved=Saved(w);const auto header=saved.find("MARKET_METADATA 4");Check(header!=std::string::npos,"Missing market header");saved.replace(header,17,"MARKET_METADATA 1");saved.erase(saved.find("MARKET_STABILITY"));saved.replace(0,19,"BIRTHRIGHT_WORLD 10");Snapshot state;std::istringstream input(saved);Accept(Load(input,state));Check(state.Market.Policy.Version==4,"Policy not migrated");
        for(const auto& a:state.Market.Areas)Check(a.second.Nutrition==1,"Invented historical malnutrition");for(const auto& r:state.Market.Regions)Check(r.second.LastObservedUse==r.second.Used,"Historical consumption became one-day demand");Accept(w.Restore(state));Accept(w.AdvanceDays(1));
    }});
    cases.push_back({"BR-060 full demography remains viable over varied twenty-year worlds",[] {
        for(unsigned seed:{7U,42U,91U}) { auto w=Fixture(seed,3);auto s=w.State();s.Policy.DailyBirthRate=0.0001;s.Policy.DailyDeathRate=0.00008;s.Connectivity.MigrationDailyFraction=0.001;s.Evolution.Policy.AnnualBirthChance=0.22;s.Politics.Policy.AnnualWarChance=0.2;Accept(w.Restore(s));const auto initial=w.Inspect().Population;Accept(w.AdvanceDays(7300));
            Check(w.Inspect().Population>initial/2 && w.Inspect().Population<initial*2 && Coverage(w)>0.7,"Broad demographic stability band exceeded");Sample("BR-060-full-demography-"+std::to_string(seed),w);
        }
    }});
    cases.push_back({"BR-059 money goods and food conserve through years of exchange",[] {
        auto w=Fixture(); const double money=MarketMoney(w.State()); Accept(w.AdvanceDays(1825)); Accept(Validate(w.State()));
        Check(std::abs(MarketMoney(w.State())-money)<money*1e-7,"Currency created or destroyed");
        Check(w.State().Market.Wages>0 && w.State().Market.Taxes>0 && w.State().Market.Rents>0,"Economy did not circulate income");
        Check(w.Inspect().Population>0 && Coverage(w)>0.5,"Basic society unable to feed itself"); Sample("five-year-circulation",w);
        const auto before=Saved(w); auto s=w.State(); s.Market.Regions.begin()->second.HouseholdCash+=100; Check(!w.Restore(s),"Unfunded cash accepted"); Check(Saved(w)==before,"Rejected economy mutated state");
        s=w.State(); s.Market.Policy.Enabled=false; Check(!w.Restore(s),"Disabled economy retained active accounts");
    }});
    cases.push_back({"BR-059 drought reduces supply and recovery improves access",[] {
        auto normal=Fixture(); auto drought=normal; auto s=drought.State(); for(auto& r:s.Market.Regions) { r.second.HarvestFactor=0.6; r.second.ShockUntil=120; } Accept(drought.Restore(s));
        Accept(normal.AdvanceDays(100)); Accept(drought.AdvanceDays(100));
        Check(Produced(drought)<Produced(normal) && Coverage(drought)<Coverage(normal),"Drought had no broad supply impact"); const double stressed=Coverage(drought); Sample("drought",drought);
        Accept(drought.AdvanceDays(500)); Check(Coverage(drought)>stressed && drought.Inspect().Population>0,"Economy did not recover after harvest shock"); Sample("recovery",drought);
    }});
    cases.push_back({"BR-059 connected food markets soften local scarcity",[] {
        auto open=Fixture(); auto s=open.State(); for(auto& a:s.Evolution.Areas) { const auto& area=s.Areas.at(a.first); double n=0; for(const auto& c:area.Cohorts)n+=static_cast<double>(c.Population); a.second.ProductiveCapacity=n*(area.RegionId==1?0.65:1.8); }
        Accept(open.Restore(s)); auto closed=open; s=closed.State(); s.Market.Regions.at(1).TradeBlockedUntil=1000; Accept(closed.Restore(s));
        Accept(open.AdvanceDays(240)); Accept(closed.AdvanceDays(240));
        Check(open.State().Market.FoodTraded>closed.State().Market.FoodTraded,"Trade closure did not reduce exchange");
        Check(open.State().Market.Regions.at(1).FoodCoverage>closed.State().Market.Regions.at(1).FoodCoverage,"Trade failed to soften local shortage"); Sample("trade-open",open); Sample("trade-blocked",closed);
    }});
    cases.push_back({"BR-059 mobilization removes labor and consumes army food",[] {
        auto peace=Fixture(42,1); auto war=peace; War(war); Accept(peace.AdvanceDays(20)); Accept(war.AdvanceDays(20));
        Check(Produced(war)<Produced(peace),"Mobilization left production unchanged"); Check(war.State().Market.ArmyFood>0,"Army did not consume food"); Sample("peace-labor",peace); Sample("mobilized-labor",war);
    }});
    cases.push_back({"BR-059 aggregate battle losses do not depend on contingent partition",[] {
        auto whole=Fixture(42,1); War(whole); auto split=whole; auto s=split.State();
        auto first=s.Politics.Armies.begin(); auto piece=first->second; const auto moved=piece.Soldiers/2; Check(moved>0,"Fixture too small"); first->second.Soldiers-=moved; piece.Soldiers=moved; piece.Key=s.Politics.NextArmy++; s.Politics.Armies.emplace(piece.Key,piece); s.Day=30; Accept(split.Restore(s));
        s=whole.State(); s.Day=30; Accept(whole.Restore(s)); Accept(whole.ResolveWarRound("battle",1)); Accept(split.ResolveWarRound("battle",1));
        Check(whole.State().Politics.Casualties==split.State().Politics.Casualties && Soldiers(whole)==Soldiers(split),"Partition altered total losses");
    }});
    cases.push_back({"BR-059 unfunded armies lose readiness and demobilize gradually",[] {
        auto w=Fixture(42,1); War(w); auto s=w.State(); for(auto& d:s.Evolution.Domains) { CreditMarketServices(s,d.first,d.second.Treasury); d.second.Spent+=d.second.Treasury; d.second.Treasury=0; } s.Day=30; Accept(w.Restore(s));
        const auto initial=Soldiers(w); Accept(w.ResolveWarRound("unfunded-round",1));
        Check(Soldiers(w)>0 && Soldiers(w)<initial && w.State().Politics.Returned>0,"Unfunded army vanished instantly or had no consequence");
        Check(w.State().Market.Readiness.at({1,1})<1,"Unpaid army kept full readiness"); Accept(Validate(w.State()));
    }});
    cases.push_back({"BR-059 estate investments require real inputs savings time and retain value",[] {
        auto w=Fixture(); Accept(w.AdvanceDays(120)); auto s=w.State(); const Id region=s.Regions.begin()->first, lord=s.Entities.at(s.Regions.at(region).OwnerEntityId).HeadCharacterId, family=s.Politics.CharacterFamilies.at(lord);
        auto& f=s.Politics.Families.at(family); auto& market=s.Market.Regions.at(region); const double grant=std::min(20000.0,market.ProducerCash); market.ProducerCash-=grant; f.Wealth+=grant; f.Received+=grant; Accept(w.Restore(s));
        Accept(w.InvestEstate("investment",lord,region)); const auto before=Saved(w); Check(!w.InvestEstate("investment",lord,region),"Repeated investment accepted"); Check(Saved(w)==before,"Replay changed stocks");
        const double capital=w.State().Market.Estates.at({family,region}).Capital; Accept(w.AdvanceDays(100)); Check(w.State().Market.Estates.at({family,region}).Capital>capital,"Completed investment had no productive asset"); Sample("estate-investment",w);
    }});
    cases.push_back({"BR-059 extinct estates return cash to active society and preserve conservation",[] {
        auto w=Fixture(); Accept(w.AdvanceDays(180)); auto s=w.State(); Id family=0; double value=0; for(const auto& f:s.Politics.Families) if(f.second.Wealth>value) { family=f.first; value=f.second.Wealth; } Check(value>0,"No estate wealth");
        std::vector<Id> deaths; for(const auto& c:s.Politics.CharacterFamilies) if(c.second==family && s.Characters.at(c.first).Alive) deaths.push_back(c.first);
        for(Id id:deaths) Accept(w.RecordDeath("death/"+std::to_string(id),id,"inheritance scenario")); Accept(w.AdvanceDays(30));
        Check(w.State().Politics.Families.at(family).Wealth==0 && w.State().Market.Inherited>=value,"Extinct family trapped money"); Accept(Validate(w.State())); Sample("extinct-estate",w);
    }});
    cases.push_back({"BR-059 economic stress and succession retain meaningful political preferences",[] {
        auto w=Fixture(); auto s=w.State(); const Id lord=s.Politics.Lords.begin()->first, region=s.Areas.at(s.Characters.at(lord).AreaId).RegionId;
        const double healthy=EconomicSatisfaction(s,lord); s.Market.Regions.at(region).FoodCoverage=0.3; Check(EconomicSatisfaction(s,lord)<healthy,"Food insecurity did not affect political wants");
        s.Market.Regions.at(region).HarvestFactor=0.7; s.Market.Regions.at(region).ShockUntil=365;
        const auto wants=s.Market.Wants.at(lord); const Id liege=s.Politics.Lords.at(lord).Liege; s.Politics.Lords.at(lord).Friendship=0.9; Accept(w.Restore(s)); Accept(w.RecordDeath("succession",liege,"succession stress")); Accept(w.AdvanceDays(30));
        Check(w.State().Politics.Lords.at(lord).Friendship>0.4,"Succession forgot established relationship history");
        Accept(w.AdvanceDays(335));
        Check(w.State().Market.Wants.at(lord).Income==wants.Income,"Succession reset persistent personality"); Accept(Validate(w.State())); Sample("succession-stress",w);
    }});
    cases.push_back({"BR-059 market save resume and legacy migration preserve contracts",[] {
        auto a=Fixture(); Accept(a.AdvanceDays(730)); auto b=Fixture(); Accept(b.AdvanceDays(117)); Snapshot s; std::istringstream input(Saved(b)); Accept(Load(input,s)); Accept(b.Restore(s)); for(unsigned i=117;i<730;++i) Accept(b.AdvanceDays(1)); Check(Saved(a)==Saved(b),"Markets depend on report/resume cadence");
        auto legacy=Saved(Fixture()); legacy.replace(0,std::string("BIRTHRIGHT_WORLD 11").size(),"BIRTHRIGHT_WORLD 9"); legacy.erase(legacy.find("MARKET_METADATA")); std::istringstream old(legacy); Accept(Load(old,s)); Check(!s.Market.Policy.Enabled && s.Version==11,"Legacy save enabled markets implicitly");
    }});
    cases.push_back({"BR-059 varied seeds and region layouts sustain a functioning economy",[] {
        for(unsigned seed:{7U,42U,91U}) { auto w=Fixture(seed,seed==42?1:3); auto s=w.State(); s.Politics.Policy.AnnualWarChance=0.2; s.Evolution.Policy.AnnualBirthChance=0.22; Accept(w.Restore(s)); const auto initial=w.Inspect().Population; Accept(w.AdvanceDays(7300)); Accept(Validate(w.State()));
            Check(w.Inspect().Population>initial/4 && w.Inspect().Population<initial*3 && Coverage(w)>0.4,"Long-run economic collapse or runaway growth");
            Check(w.State().Domains.size()>w.State().Politics.RetiredDomains.size() && w.State().Market.Wages>0,"Long-run society stopped functioning"); Sample("twenty-year-seed-"+std::to_string(seed),w);
        }
    }});
}
}
