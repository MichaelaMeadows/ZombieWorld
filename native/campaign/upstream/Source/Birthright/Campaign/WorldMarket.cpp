#include "WorldMarket.h"
#include "WorldPolitics.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <set>
#include <sstream>
#include <iomanip>

namespace Birthright::Campaign
{
namespace
{
Result Good() { return {true,"accepted",""}; }
Result Bad(const std::string& reason) { return {false,"market_invalid",reason}; }
Result Denied(const std::string& reason) { return {false,"market_ineligible",reason}; }
double Population(const Area& a) { double n=0; for(const auto& c:a.Cohorts) n+=static_cast<double>(c.Population); return n; }
double Population(const Snapshot& s,Id r) { double n=0; for(const auto& a:s.Areas) if(a.second.RegionId==r) n+=Population(a.second); return n; }
bool Range(double n,double lo,double hi) { return std::isfinite(n) && n>=lo && n<=hi; }
Id HomeRegion(const Snapshot& s,Id character) { return s.Areas.at(s.Characters.at(character).AreaId).RegionId; }
Id OwnerFamily(const Snapshot& s,Id region) {
    const Id lord=s.Entities.at(s.Regions.at(region).OwnerEntityId).HeadCharacterId;
    const auto f=s.Politics.CharacterFamilies.find(lord); return f==s.Politics.CharacterFamilies.end()?0:f->second;
}
void ReceiveFamily(Snapshot& s,Id family,double amount) { auto& f=s.Politics.Families.at(family); f.Wealth+=amount; f.Received+=amount; }
void ReceiveTax(Snapshot& s,Id region,double amount) { const Id domain=s.Regions.at(region).DomainId; auto& b=s.Market.Budgets[domain]; b.TaxReceipts+=amount; b.ReceiptsToday+=amount; auto& d=s.Evolution.Domains.at(domain); d.Treasury+=amount; d.Revenue+=amount; s.Market.Taxes+=amount; }
double Food(const Snapshot& s,Id region) { double n=0; for(const auto& a:s.Areas) if(a.second.RegionId==region) n+=s.Evolution.Areas.at(a.first).FoodStored; return n; }
// Food has exactly one inventory, in AreaEconomy. Transfers record both sides of its ledger.
void MoveFood(Snapshot& s,Id from,Id to,double quantity) {
    double left=quantity; Id destination=0;
    for(const auto& a:s.Areas) if(a.second.RegionId==to) { destination=a.first; break; }
    for(const auto& a:s.Areas) if(a.second.RegionId==from && left>0) {
        auto& stock=s.Evolution.Areas.at(a.first); const double take=std::min(left,stock.FoodStored);
        stock.FoodStored-=take; s.Market.Areas.at(a.first).FoodExported+=take; left-=take;
        s.Evolution.Areas.at(destination).FoodStored+=take; s.Market.Areas.at(destination).FoodImported+=take;
    }
}
double ConsumeFood(Snapshot& s,Id region,double quantity) {
    double left=quantity;
    for(const auto& a:s.Areas) if(a.second.RegionId==region && left>0) {
        auto& stock=s.Evolution.Areas.at(a.first); const double take=std::min(left,stock.FoodStored);
        stock.FoodStored-=take; stock.Consumed+=take; left-=take;
    }
    return quantity-left;
}
// Regional consumption already pools inventory. Pool storage capacity too, while retaining
// one authoritative area inventory and balanced transfer entries for every local movement.
void BalanceRegionalStorage(Snapshot& s) {
    std::map<Id,std::vector<Id>> regions;
    for(const auto& row:s.Evolution.Areas) if(s.Areas.at(row.first).RegionId) regions[s.Areas.at(row.first).RegionId].push_back(row.first);
    for(const auto& region:regions) {
        double total=0,capacity=0;
        for(Id id:region.second) {total+=s.Evolution.Areas.at(id).FoodStored;capacity+=std::max(1.0,Population(s.Areas.at(id)))*s.Evolution.Policy.FoodReserveDays;}
        const double retained=std::min(total,capacity);
        if(total>capacity) for(Id id:region.second) {auto& a=s.Evolution.Areas.at(id);const double keep=total>0?a.FoodStored*(retained/total):0;a.Wasted+=a.FoodStored-keep;a.FoodStored=keep;}
        if(capacity<=0) continue;
        std::vector<std::pair<Id,double>> donors,receivers;
        for(Id id:region.second) {const double target=retained*(std::max(1.0,Population(s.Areas.at(id)))*s.Evolution.Policy.FoodReserveDays/capacity);const double excess=s.Evolution.Areas.at(id).FoodStored-target;if(excess>0)donors.push_back({id,excess});else if(excess<0)receivers.push_back({id,-excess});}
        std::size_t source=0;
        for(auto& receiver:receivers) while(receiver.second>0 && source<donors.size()) {
            auto& donor=donors[source];const double moved=std::min({receiver.second,donor.second,s.Evolution.Areas.at(donor.first).FoodStored});
            if(moved<=0){++source;continue;}
            s.Evolution.Areas.at(donor.first).FoodStored-=moved;s.Evolution.Areas.at(receiver.first).FoodStored+=moved;
            s.Market.Areas.at(donor.first).FoodExported+=moved;s.Market.Areas.at(receiver.first).FoodImported+=moved;
            donor.second-=moved;receiver.second-=moved;if(donor.second<=0)++source;
        }
    }
}
void UseGood(RegionalMarket& m,std::size_t good,double quantity) { quantity=std::min(quantity,m.Stock[good]); m.Stock[good]-=quantity; m.Used[good]+=quantity; }
void MakeGood(RegionalMarket& m,std::size_t good,double quantity) { m.Stock[good]+=quantity; m.Produced[good]+=quantity; }
bool AtWar(const Snapshot& s,Id region) { const Id d=s.Regions.at(region).DomainId; for(const auto& w:s.Politics.Wars) if(w.second.Active && (w.second.Attacker==d || w.second.Defender==d)) return true; return false; }
bool Hostile(const Snapshot& s,Id a,Id b) { a=s.Regions.at(a).DomainId; b=s.Regions.at(b).DomainId; for(const auto& w:s.Politics.Wars) if(w.second.Active && ((w.second.Attacker==a && w.second.Defender==b)||(w.second.Attacker==b && w.second.Defender==a))) return true; return false; }
void InitializeWants(Snapshot& s) {
    for(const auto& c:s.Characters) if(!s.Market.Wants.count(c.first)) {
        // Independent deterministic preferences, persistent through changes of office and liege.
        CharacterWants wants; auto draw=[&](unsigned salt) { std::uint64_t x=c.first*0x9e3779b97f4a7c15ULL+s.Seed+salt; x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL; x=(x^(x>>27))*0x94d049bb133111ebULL; return 0.2+0.8*static_cast<double>((x^(x>>31))%10000)/10000; };
        wants.Income=draw(1); wants.Status=draw(2); wants.Security=draw(3); wants.Autonomy=draw(4); s.Market.Wants.emplace(c.first,wants);
    }
}
}

double MarketMoney(const Snapshot& s) {
    double money=0; for(const auto& r:s.Market.Regions) money+=r.second.HouseholdCash+r.second.ProducerCash;
    for(const auto& f:s.Politics.Families) money+=f.second.Wealth;
    for(const auto& d:s.Evolution.Domains) money+=d.second.Treasury;
    return money;
}
bool CanBuyComforts(const Snapshot& s,Id character,double cost) {
    if(!s.Market.Policy.Enabled) return true;
    const Id region=HomeRegion(s,character); return region && s.Market.Regions.at(region).Stock[2]>=cost/2;
}
void BuyComforts(Snapshot& s,Id character,double cost) {
    if(!s.Market.Policy.Enabled) return;
    auto& r=s.Market.Regions.at(HomeRegion(s,character)); UseGood(r,2,cost/2); r.ProducerCash+=cost;
}
void CreditMarketServices(Snapshot& s,Id domain,double amount) {
    if(!s.Market.Policy.Enabled || amount<=0) return;
    std::vector<Id> regions; for(const auto& r:s.Regions) if(r.second.DomainId==domain) regions.push_back(r.first);
    double total=0; std::vector<double> weights;
    for(Id id:regions) { const auto& r=s.Market.Regions.at(id); const double weight=s.Market.Policy.ResponsiveBudgets ? std::max(1.0,Population(s,id))*(1+3*(1-r.WageCoverage)+3*(1-r.FoodCoverage)):1; weights.push_back(weight); total+=weight; }
    double left=amount;
    for(std::size_t i=0;i<regions.size();++i) { const double payment=std::min(left,i+1==regions.size()?left:amount*weights[i]/total); s.Market.Regions.at(regions[i]).ProducerCash+=payment; s.Market.Regions.at(regions[i]).ServiceReceipts+=payment; left-=payment; }
}
Result ValidateMarket(const Snapshot& s) {
    const auto& m=s.Market; const auto& p=m.Policy;
    if(!Range(p.ServiceIncomeShare,0.5,1) || !Range(p.IncomeSmoothingDays,1,365) || !Range(p.EmergencyReleaseDays,1,365) || !Range(p.MilitaryReserveDays,1,365)) return Bad("Invalid receipt budget policy.");
    if(m.BudgetStartedDay<0 || m.BudgetStartedDay>s.Day || !Range(m.BudgetOpeningTaxes,0,m.Taxes)) return Bad("Invalid budget epoch.");
    double budgetTaxes=m.BudgetOpeningTaxes;
    for(const auto& row:m.Budgets){const auto& b=row.second;
        if(!s.Evolution.Domains.count(row.first) || b.ObservedDays>static_cast<std::uint64_t>(s.Day-m.BudgetStartedDay)) return Bad("Invalid realm budget identity/time.");
        for(double value:{b.TaxReceipts,b.ServicePaid,b.EmergencyReleased,b.SurplusReleased,b.ServiceShortfall,b.ReceiptsToday,b.SmoothedReceipts,b.ServicesToday,b.DesiredServices,b.MilitaryDaily,b.MilitaryReserve,b.ReserveTarget,b.AvailableBeforeServices})if(!Range(value,0,1e24))return Bad("Invalid public budget amount.");
        if(b.EmergencyReleased+b.SurplusReleased>b.ServicePaid+1e-8*std::max(1.0,b.ServicePaid) || b.ServicesToday>b.AvailableBeforeServices+1e-8 || b.ReceiptsToday>b.TaxReceipts+1e-8) return Bad("Public budget attribution exceeds payments.");
        budgetTaxes+=b.TaxReceipts;
    }
    if(std::abs(budgetTaxes-m.Taxes)>1e-8*std::max(1.0,m.Taxes)) return Bad("Public budget receipts differ from actual taxes.");
    if(p.Version!=4 || !Range(p.InitialCashPerPerson,0.01,100) || !Range(p.DailyWage,0.00001,1) || !Range(p.TaxRate,0,0.7) || !Range(p.FoodBasePrice,0.00001,1) || !Range(p.SubsistenceShare,0,1) || !Range(p.TradeDailyFraction,0,1) || !Range(p.ReserveDays,1,365) || !Range(p.InvestmentFraction,0.001,0.5) || !p.ConstructionDays || p.ConstructionDays>3650) return Bad("Policy bounds/version.");
    if(!Range(p.ProducerReserveDays,1,365) || !Range(p.TreasuryReserveDays,1,3650) || !Range(p.BudgetReleaseDays,1,3650) || !Range(p.GoodsTargetDays,1,365) || !Range(p.NutritionLossDays,1,365) || !Range(p.NutritionRecoveryDays,1,365) || !Range(p.StarvationThreshold,0.1,1)) return Bad("Invalid stability policy.");
    for(std::size_t g=0;g<MarketGoodCount;++g) { if(!Range(p.Recipes[g].OutputPerWorker,0,1)) return Bad("Invalid recipe output."); for(std::size_t input=0;input<MarketGoodCount;++input) if(!Range(p.Recipes[g].Inputs[input],0,100) || (input>=g && p.Recipes[g].Inputs[input]!=0)) return Bad("Recipes require earlier-stage inputs; cycles rejected."); }
    if(m.DiagnosticsStartedDay<0 || m.DiagnosticsStartedDay>s.Day) return Bad("Invalid diagnostics epoch.");
    if(!p.Enabled) return m.DiagnosticsStartedDay==0 && m.BudgetStartedDay==0 && m.BudgetOpeningTaxes==0 && m.Budgets.empty() && m.Regions.empty() && m.Areas.empty() && m.Estates.empty() && m.Wants.empty() && m.Links.empty() && m.Readiness.empty() && m.InitialMoney==0 && m.FoodTraded==0 && m.Taxes==0 && m.Wages==0 && m.Rents==0 && m.Investment==0 && m.Inherited==0 && m.ArmyFood==0 ? Good():Bad("Disabled economy contains state.");
    if(!s.Politics.Policy.Enabled || m.Regions.size()!=s.Regions.size() || m.Areas.size()!=s.Evolution.Areas.size()) return Bad("Enabled markets need politics and complete regions/land.");
    if(!Range(m.InitialMoney,0,1e20) || std::abs(MarketMoney(s)-m.InitialMoney)>1e-7*std::max(1.0,m.InitialMoney)) return Bad("Currency conservation: total="+std::to_string(MarketMoney(s))+" initial="+std::to_string(m.InitialMoney));
    for(double n:{m.FoodTraded,m.Taxes,m.Wages,m.Rents,m.Investment,m.Inherited,m.ArmyFood}) if(!Range(n,0,1e24)) return Bad("Invalid cumulative market flow.");
    double imports=0,exports=0;
    for(const auto& a:m.Areas) { if(!s.Evolution.Areas.count(a.first) || !Range(a.second.Nutrition,0,1) || a.second.ShortageDays>static_cast<std::uint64_t>(s.Day) || !Range(a.second.LaborCapacity,1,1e12) || !Range(a.second.FoodImported,0,1e24) || !Range(a.second.FoodExported,0,1e24)) return Bad("Invalid labor/food account area="+std::to_string(a.first)); imports+=a.second.FoodImported; exports+=a.second.FoodExported; }
    for(const auto& a:m.Areas) for(double value:{a.second.SupplyShortagePersonDays,a.second.CashShortagePersonDays,a.second.NutritionDeficitPersonDays,a.second.ExpectedNutritionDeaths}) if(!Range(value,0,1e24)) return Bad("Invalid demographic/food exposure area="+std::to_string(a.first));
    for(const auto& a:m.Areas) if(a.second.DemographicBirths>2000000000000000000ULL || a.second.DemographicDeaths>2000000000000000000ULL) return Bad("Demographic diagnostic outside campaign bounds.");
    if(std::abs(imports-exports)>1e-8*std::max(1.0,imports)) return Bad("Food transfers do not balance.");
    for(const auto& row:m.Regions) { const auto& r=row.second;
        if(!s.Regions.count(row.first) || !Range(r.HouseholdCash,0,1e20) || !Range(r.ProducerCash,0,1e20) || !Range(r.FoodPrice,p.FoodBasePrice*0.49,p.FoodBasePrice*4.01) || !Range(r.FoodCoverage,0,1) || !Range(r.ArmySupply,0,1) || !Range(r.WageCoverage,0,1) || !Range(r.HarvestFactor,0,2) || r.ShockUntil<0 || r.TradeBlockedUntil<0) {
            std::ostringstream detail; detail<<std::setprecision(17)<<"region="<<row.first<<"; day="<<s.Day<<"; household="<<r.HouseholdCash<<"; producer="<<r.ProducerCash<<"; food_price="<<r.FoodPrice<<"; coverage="<<r.FoodCoverage<<"; wages="<<r.WageCoverage<<"; army_supply="<<r.ArmySupply; return Bad(detail.str());
        }
        for(double value:{r.UnpaidWages,r.ServiceReceipts,r.FoodPurchases}) if(!Range(value,0,1e24)) return Bad("Invalid regional payment diagnostic.");
        if(!Range(r.PublicCapital,0,1e18)) return Bad("Invalid public estate capital.");
        for(std::size_t g=0;g<MarketGoodCount;++g) if(!Range(r.DailyUse[g],0,1e20) || !Range(r.LastObservedUse[g],0,r.Used[g]) || !Range(r.Stock[g],0,1e20) || !Range(r.Produced[g],0,1e24) || !Range(r.Used[g],0,1e24) || !Range(r.Wasted[g],0,1e24) || std::abs(r.Produced[g]-r.Used[g]-r.Wasted[g]-r.Stock[g])>1e-8*std::max(1.0,r.Produced[g])) return Bad("Goods ledger region="+std::to_string(row.first));
    }
    for(const auto& a:m.Estates) if(!s.Politics.Families.count(a.first.first) || !s.Regions.count(a.first.second) || !Range(a.second.Capital,0,1e16) || !Range(a.second.PendingCapital,0,1e16) || a.second.ReadyDay<0 || a.second.LastInvestmentDay>s.Day) return Bad("Invalid estate asset.");
    for(const auto& w:m.Wants) if(!s.Characters.count(w.first) || !Range(w.second.Income,0,1) || !Range(w.second.Status,0,1) || !Range(w.second.Security,0,1) || !Range(w.second.Autonomy,0,1)) return Bad("Invalid character preferences.");
    for(const auto& r:m.Readiness) if(!s.Politics.Wars.count(r.first.first) || (s.Politics.Wars.at(r.first.first).Attacker!=r.first.second && s.Politics.Wars.at(r.first.first).Defender!=r.first.second) || !Range(r.second,0.1,1)) return Bad("Invalid army readiness.");
    std::set<std::pair<Id,Id>> links;
    for(const auto& l:m.Links) if(l.First>=l.Second || !m.Regions.count(l.First) || !m.Regions.count(l.Second) || !l.Distance || l.Distance>128 || !links.insert({l.First,l.Second}).second) return Bad("Invalid trade link.");
    return Good();
}
Result World::ConfigureMarket(const std::string& operation,const MarketRules& policy) {
    auto r=CheckOperation(operation); if(!r) return r;
    if(!policy.Enabled || StateData.Market.Policy.Enabled || !StateData.Politics.Policy.Enabled) return Bad("Enable markets once after politics.");
    auto next=BeginMutation(); auto& m=next.Market; m.Policy=policy; m.DiagnosticsStartedDay=next.Day; m.BudgetStartedDay=next.Day;
    for(const auto& a:next.Evolution.Areas) m.Areas[a.first].LaborCapacity=std::max(1.0,Population(next.Areas.at(a.first)));
    for(const auto& region:next.Regions) {
        const double n=Population(next,region.first); auto& market=m.Regions[region.first];
        market.HouseholdCash=n*policy.InitialCashPerPerson*0.6; market.ProducerCash=n*policy.InitialCashPerPerson*0.4; market.FoodPrice=policy.FoodBasePrice;
        const Id family=OwnerFamily(next,region.first); if(family) m.Estates[{family,region.first}].Capital=n*0.01;
        // Derive potential land trading connections once; daily security and scenario closures gate use.
        std::queue<std::pair<Id,unsigned>> queue; std::set<Id> seen; std::map<Id,unsigned> distances;
        for(const auto& a:next.Areas) if(a.second.RegionId==region.first) { queue.push({a.first,0}); seen.insert(a.first); }
        while(!queue.empty()) { const auto a=queue.front(); queue.pop(); if(a.second>=64) continue;
            for(Id id:next.Areas.at(a.first).Neighbors) if(!next.Areas.at(id).IsWater && seen.insert(id).second) {
                const Id other=next.Areas.at(id).RegionId; if(other && other!=region.first && !distances.count(other)) distances[other]=a.second+1;
                queue.push({id,a.second+1});
            }
        }
        for(const auto& d:distances) if(d.first>region.first) m.Links.push_back({region.first,d.first,d.second});
    }
    InitializeWants(next); m.InitialMoney=MarketMoney(next);
    next.Events.push_back({operation,"market_enabled",next.Day,0,0,"version=4; initial liquidity="+std::to_string(m.InitialMoney)});
    return Commit(std::move(next));
}

std::vector<double> AdvanceMarket(Snapshot& s) {
    auto& m=s.Market; const auto& p=m.Policy; InitializeWants(s);
    for(auto& row:m.Budgets)row.second.ReceiptsToday=0;
    std::map<Id,double> people, soldiers, stock, demand, coverage, investment, supplyGap, cashGap;
    for(auto& r:m.Regions) { r.second.PublicCapital*=0.99995; investment[r.first]=r.second.PublicCapital; }
    for(const auto& a:s.Areas) if(a.second.RegionId) people[a.second.RegionId]+=Population(a.second);
    for(const auto& a:s.Politics.Armies) if(!a.second.Expedition) soldiers[s.Areas.at(a.second.HomeArea).RegionId]+=a.second.Soldiers;
    for(const auto& t:s.Troops) soldiers[s.Areas.at(t.second.HomeAreaId).RegionId]+=1;
    for(auto& e:m.Estates) {
        if(e.second.PendingCapital>0 && e.second.ReadyDay<=s.Day) { e.second.Capital+=e.second.PendingCapital; e.second.PendingCapital=0; }
        e.second.Capital*=0.99995; investment[e.first.second]+=e.second.Capital;
    }
    const double season=1+s.Evolution.Policy.SeasonalAmplitude*std::sin(static_cast<double>(s.Day%365)*6.283185307179586/365);
    for(auto& row:s.Evolution.Areas) {
        const auto& area=s.Areas.at(row.first); auto& food=row.second; const Id region=area.RegionId;
        const double labor=std::min(1.2,Population(area)/m.Areas.at(row.first).LaborCapacity);
        const double harvest=region && m.Regions.at(region).ShockUntil>s.Day ? m.Regions.at(region).HarvestFactor:1;
        const double assets=region ? 1+std::min(0.4,investment[region]/std::max(1.0,people[region])*0.2):1;
        const double health=p.PersistentNutrition ? 0.5+0.5*m.Areas.at(row.first).Nutrition:1;
        const double output=food.ProductiveCapacity*season*labor*harvest*assets*health;
        food.Produced+=output; food.FoodStored+=output; if(region) stock[region]+=food.FoodStored;
    }
    for(auto& row:m.Regions) {
        const Id region=row.first; auto& r=row.second; const double n=people[region]; demand[region]=n+soldiers[region];
        const double wage=std::min(r.ProducerCash,n*p.DailyWage); r.ProducerCash-=wage;
        const double tax=wage*p.TaxRate; r.HouseholdCash+=wage-tax; ReceiveTax(s,region,tax); m.Wages+=wage;
        r.WageCoverage=n>0?wage/(n*p.DailyWage):1; r.UnpaidWages+=std::max(0.0,n*p.DailyWage-wage);
        // Small extensible recipe chain: materials -> tools and luxuries. Inventories are bounded.
        for(std::size_t g=0;g<MarketGoodCount;++g) {
            const auto& recipe=p.Recipes[g]; double output=n*recipe.OutputPerWorker;
            r.DailyUse[g]+=(std::max(0.0,r.Used[g]-r.LastObservedUse[g])-r.DailyUse[g])*0.03;
            r.LastObservedUse[g]=r.Used[g];
            // Keep startup/project inputs available, then replenish observed consumption.
            const double minimumStock=n*(g==0?0.02:g==1?0.005:0.03);
            const double target=std::max(minimumStock,r.DailyUse[g]*p.GoodsTargetDays);
            if(p.DemandProduction) output=std::min(output,std::max(0.0,target-r.Stock[g]));
            for(std::size_t input=0;input<MarketGoodCount;++input) if(recipe.Inputs[input]>0) output=std::min(output,r.Stock[input]/recipe.Inputs[input]);
            for(std::size_t input=0;input<MarketGoodCount;++input) if(recipe.Inputs[input]>0) UseGood(r,input,output*recipe.Inputs[input]);
            MakeGood(r,g,output);
        }
        const double reserve=p.ResponsiveBudgets ? n*p.DailyWage*p.ProducerReserveDays:0;
        const Id family=OwnerFamily(s,region); const double rent=family?std::min(std::max(0.0,r.ProducerCash-reserve),n*0.0001):0;
        if(rent>0) { r.ProducerCash-=rent; ReceiveFamily(s,family,rent*(1-p.TaxRate)); ReceiveTax(s,region,rent*p.TaxRate); m.Rents+=rent; }
        for(const auto& asset:m.Estates) if(asset.first.second==region) {
            const double dividend=std::min(std::max(0.0,r.ProducerCash-reserve),asset.second.Capital*0.001); r.ProducerCash-=dividend;
            ReceiveFamily(s,asset.first.first,dividend*(1-p.TaxRate)); ReceiveTax(s,region,dividend*p.TaxRate); m.Rents+=dividend;
        }
    }
    for(const auto& link:m.Links) {
        auto& a=m.Regions.at(link.First); auto& b=m.Regions.at(link.Second);
        if(a.TradeBlockedUntil>s.Day || b.TradeBlockedUntil>s.Day || Hostile(s,link.First,link.Second)) continue;
        Id from=link.First,to=link.Second;
        if(stock[from]/std::max(1.0,demand[from])<stock[to]/std::max(1.0,demand[to])) std::swap(from,to);
        auto& seller=m.Regions.at(from); auto& buyer=m.Regions.at(to);
        const double surplus=std::max(0.0,stock[from]-demand[from]*p.ReserveDays);
        const double deficit=std::max(0.0,demand[to]*p.ReserveDays-stock[to]);
        const double price=seller.FoodPrice*(1+0.01*link.Distance);
        const double volume=std::min({surplus,deficit,demand[to]*p.TradeDailyFraction/(1+link.Distance*0.03),buyer.ProducerCash/price});
        if(volume>0) { const double payment=std::min(buyer.ProducerCash,volume*price); buyer.ProducerCash-=payment; seller.ProducerCash+=payment; MoveFood(s,from,to,volume); stock[from]-=volume; stock[to]+=volume; m.FoodTraded+=volume; }
    }
    for(auto& row:m.Regions) {
        const Id region=row.first; auto& r=row.second; const double n=people[region];
        const double availableFood=Food(s,region);
        const double freeFood=ConsumeFood(s,region,n*p.SubsistenceShare);
        const double buy=std::min({std::max(0.0,n-freeFood),Food(s,region),r.HouseholdCash/r.FoodPrice});
        const double eaten=freeFood+ConsumeFood(s,region,buy); const double purchase=std::min(r.HouseholdCash,buy*r.FoodPrice); r.HouseholdCash-=purchase; r.ProducerCash+=purchase; r.FoodPurchases+=purchase;
        supplyGap[region]=n>0?std::max(0.0,n-availableFood)/n:0;
        cashGap[region]=n>0?std::max(0.0,std::min(n,availableFood)-eaten)/n:0;
        // Necessities and a month's cash buffer take precedence over discretionary consumption.
        const double reserve=n*(1-p.SubsistenceShare)*r.FoodPrice*30;
        const double comforts=eaten>=n*0.99?std::min({r.Stock[2],n*0.001,std::max(0.0,r.HouseholdCash-reserve)/1.2}):0;
        UseGood(r,2,comforts); r.HouseholdCash-=comforts*1.2; r.ProducerCash+=comforts*1.2;
        auto& d=s.Evolution.Domains.at(s.Regions.at(region).DomainId);
        const double army=std::min({soldiers[region],Food(s,region),d.Treasury/r.FoodPrice});
        ConsumeFood(s,region,army); const double cost=std::min(d.Treasury,army*r.FoodPrice); d.Treasury-=cost; d.Spent+=cost; r.ProducerCash+=cost; m.ArmyFood+=army;
        r.ArmySupply+=(soldiers[region]>0?army/soldiers[region]-r.ArmySupply:1-r.ArmySupply)*0.1;
        r.FoodCoverage=n>0?std::clamp(eaten/n,0.0,1.0):1; coverage[region]=r.FoodCoverage;
        const double days=Food(s,region)/std::max(1.0,demand[region]);
        const double target=p.FoodBasePrice*std::clamp(p.ReserveDays/std::max(1.0,days),0.5,4.0);
        r.FoodPrice+=(target-r.FoodPrice)*0.02;
        for(std::size_t g=0;g<MarketGoodCount;++g) { const double waste=std::max(0.0,r.Stock[g]-std::max(10.0,n)*0.5); r.Stock[g]-=waste; r.Wasted[g]+=waste; }
    }
    AdvancePublicBudgets(s);
    for(auto& row:s.Evolution.Domains) {
        double n=0,wellbeing=0;for(const auto& r:s.Regions)if(r.second.DomainId==row.first){n+=people[r.first];wellbeing+=people[r.first]*coverage[r.first];}
        const double target=std::clamp(0.25+0.5*(n>0?wellbeing/n:1)-p.TaxRate*0.15,0.0,1.0);
        row.second.Legitimacy+=(target-row.second.Legitimacy)*0.002;
    }
    BalanceRegionalStorage(s);
    std::vector<double> shortages;
    for(auto& row:s.Evolution.Areas) {
        auto& area=s.Areas.at(row.first); auto& food=row.second; const double n=Population(area); double shortage=0;
        if(area.RegionId) shortage=1-coverage[area.RegionId];
        else { const double consumed=std::min(n,food.FoodStored); food.FoodStored-=consumed; food.Consumed+=consumed; shortage=n>0?1-consumed/n:0; }
        const double waste=std::max(0.0,food.FoodStored-std::max(1.0,n)*s.Evolution.Policy.FoodReserveDays); food.FoodStored-=waste; food.Wasted+=waste;
        auto& health=m.Areas.at(row.first);
        health.SupplyShortagePersonDays+=n*(area.RegionId?supplyGap[area.RegionId]:shortage);
        health.CashShortagePersonDays+=n*(area.RegionId?cashGap[area.RegionId]:0);
        if(shortage>0.05) ++health.ShortageDays;
        if(p.PersistentNutrition) { const double target=1-shortage; health.Nutrition+=(target-health.Nutrition)/(target<health.Nutrition?p.NutritionLossDays:p.NutritionRecoveryDays); health.Nutrition=std::clamp(health.Nutrition,0.0,1.0); }
        health.NutritionDeficitPersonDays+=n*(1-health.Nutrition);
        area.FoodCapacity=static_cast<std::int64_t>(std::min(1e12,n*(1-shortage)+food.FoodStored)); shortages.push_back(shortage);
    }
    return shortages;
}

double PurchaseDefenseFood(Snapshot& s,Id region,Id family,double soldiers,double& cost) {
    auto& r=s.Market.Regions.at(region);auto& f=s.Politics.Families.at(family);
    const double available=std::max(0.0,Food(s,region)-Population(s,region)*s.Bandits.Policy.DefenseFoodReserveDays);
    const double food=ConsumeFood(s,region,std::min({soldiers,available,f.Wealth/r.FoodPrice}));
    cost=std::min(f.Wealth,food*r.FoodPrice);f.Wealth-=cost;f.Spent+=cost;r.ProducerCash+=cost;s.Market.ArmyFood+=food;
    return food;
}

Result World::InvestEstate(const std::string& operation,Id character,Id region) {
    auto r=CheckOperation(operation); if(!r) return r; const auto& s=StateData;
    if(!s.Market.Policy.Enabled || !s.Characters.count(character) || !s.Characters.at(character).Alive || s.Day-s.Characters.at(character).BornDay<s.Policy.AdultAgeDays || !s.Market.Regions.count(region) || !s.Politics.CharacterFamilies.count(character)) return Denied("Investment requires an adult family member and market.");
    const Id family=s.Politics.CharacterFamilies.at(character); if(HomeRegion(s,character)!=region && OwnerFamily(s,region)!=family) return Denied("Investment requires residence or a family holding.");
    const auto found=s.Market.Estates.find({family,region}); const double capital=found==s.Market.Estates.end()?0:found->second.Capital;
    if(found!=s.Market.Estates.end() && (found->second.PendingCapital>0 || s.Day-found->second.LastInvestmentDay<365)) return Denied("Estate project/cooldown already active.");
    const double cost=std::max(10.0,Population(s,region)*s.Market.Policy.InvestmentFraction);
    if(capital>std::max(10.0,Population(s,region))*2 || s.Politics.Families.at(family).Wealth<cost*2 || s.Market.Regions.at(region).Stock[0]<cost*0.2 || s.Market.Regions.at(region).Stock[1]<cost*0.05) return Denied("Investment lacks savings, inputs or productive headroom.");
    auto next=BeginMutation(); auto& f=next.Politics.Families.at(family); f.Wealth-=cost; f.Spent+=cost;
    auto& market=next.Market.Regions.at(region); market.ProducerCash+=cost; UseGood(market,0,cost*0.2); UseGood(market,1,cost*0.05);
    auto& asset=next.Market.Estates[{family,region}]; asset.PendingCapital=cost; asset.ReadyDay=next.Day+next.Market.Policy.ConstructionDays; asset.LastInvestmentDay=next.Day; next.Market.Investment+=cost;
    next.Events.push_back({operation,"estate_investment",next.Day,character,0,"region="+std::to_string(region)+"; cost="+std::to_string(cost)}); return Commit(std::move(next));
}
double EconomicSatisfaction(const Snapshot& s,Id character) {
    if(!s.Market.Policy.Enabled || !s.Market.Wants.count(character)) return 0;
    const Id region=HomeRegion(s,character); if(!region) return 0; const auto& r=s.Market.Regions.at(region); const auto& w=s.Market.Wants.at(character);
    const Id family=s.Politics.CharacterFamilies.at(character); const auto& f=s.Politics.Families.at(family);
    const double security=r.FoodCoverage-(AtWar(s,region)?0.4:0);
    const double income=std::min(1.0,f.Wealth/(50*DesiredHoldings(s,character)));
    return 0.15*w.Income*(income-0.5)+0.2*w.Security*(security-0.75)-0.15*w.Autonomy*s.Market.Policy.TaxRate;
}
Result AdvanceEstates(World& world) {
    auto& s=world.StateData; if(!s.Market.Policy.Enabled || s.Day%30) return Good();
    std::set<Id> living; for(const auto& c:s.Politics.CharacterFamilies) if(s.Characters.at(c.first).Alive) living.insert(c.second);
    for(auto& family:s.Politics.Families) if(!living.count(family.first)) {
        Id heir=0; // Prefer any surviving descendant through either parent, even in another house.
        for(const auto& c:s.Characters) if(c.second.Alive && s.Politics.CharacterFamilies.count(c.first)) {
            std::vector<Id> pending=c.second.Parents; std::set<Id> seen;
            while(!pending.empty()) { Id id=pending.back(); pending.pop_back(); if(!seen.insert(id).second) continue;
                if(id==family.first) { heir=s.Politics.CharacterFamilies.at(c.first); break; }
                const auto& parents=s.Characters.at(id).Parents; pending.insert(pending.end(),parents.begin(),parents.end());
            }
            if(heir) break;
        }
        const double value=family.second.Wealth;
        if(value>0) {
            family.second.Wealth=0; family.second.Spent+=value;
            if(heir) ReceiveFamily(s,heir,value);
            else { Id region=HomeRegion(s,family.first); if(!region) region=s.Regions.begin()->first; auto& d=s.Evolution.Domains.at(s.Regions.at(region).DomainId); d.Treasury+=value; d.Revenue+=value; }
            s.Market.Inherited+=value; s.Events.push_back({"market/"+std::to_string(s.Day)+"/inherit/"+std::to_string(family.first),"estate_inherited",s.Day,family.first,heir,"wealth="+std::to_string(value)});
        }
        std::vector<std::pair<Id,Id>> assets; for(const auto& a:s.Market.Estates) if(a.first.first==family.first) assets.push_back(a.first);
        for(const auto& key:assets) { const auto old=s.Market.Estates.at(key); Id owner=heir?heir:OwnerFamily(s,key.second); if(!living.count(owner)) owner=0;
            if(owner && owner!=family.first) { auto& dest=s.Market.Estates[{owner,key.second}]; dest.Capital+=old.Capital; dest.PendingCapital+=old.PendingCapital; dest.ReadyDay=std::max(dest.ReadyDay,old.ReadyDay); }
            else s.Market.Regions.at(key.second).PublicCapital+=old.Capital; // Unfinished orphan projects are abandoned; completed infrastructure remains.
            s.Market.Estates.erase(key);
        }
    }
    std::set<Id> proposed; std::vector<std::pair<Id,Id>> investments;
    for(const auto& c:s.Characters) if(c.second.Alive && s.Day-c.second.BornDay>=s.Policy.AdultAgeDays && s.Politics.CharacterFamilies.count(c.first) && std::find(s.Evolution.ProtectedCharacters.begin(),s.Evolution.ProtectedCharacters.end(),c.first)==s.Evolution.ProtectedCharacters.end()) {
        const Id f=s.Politics.CharacterFamilies.at(c.first), region=HomeRegion(s,c.first);
        if(region && !proposed.count(f)) {
            const auto& market=s.Market.Regions.at(region);
            const bool demand=!s.Market.Policy.DemandProduction || market.FoodCoverage<0.98 || Food(s,region)<Population(s,region)*s.Market.Policy.ReserveDays*1.25;
            if(demand) { proposed.insert(f); investments.push_back({c.first,region}); }
        }
    }
    for(const auto& i:investments) { auto result=world.InvestEstate("market/"+std::to_string(s.Day)+"/invest/"+std::to_string(i.first),i.first,i.second); if(!result && result.Code!="market_ineligible") return result; }
    return Good();
}
}
