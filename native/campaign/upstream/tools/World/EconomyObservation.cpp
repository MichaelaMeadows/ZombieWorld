#include "EconomyObservation.h"
#include "WorldMarket.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <functional>

namespace Birthright::Campaign::Testing {
namespace {
using Values=std::map<std::string,double>;
Values Totals(const Snapshot& s) {
    Values v{{"SupplyShortagePersonDays",0},{"CashShortagePersonDays",0},{"NutritionDeficitPersonDays",0},{"DemographicBirths",0},{"DemographicDeaths",0},{"ExpectedNutritionDeaths",0},{"UnpaidWages",0},{"ServiceReceipts",0},{"FoodPurchases",0},{"FoodProduced",0},{"FoodConsumed",0},{"FoodWasted",0},{"FoodSpoiled",0},{"FoodAbandoned",0},{"FoodCapacityOverflow",0},{"GoodsProduced",0},{"GoodsUsed",0},{"GoodsWasted",0},{"GoodsStored",0},{"UnclaimedSupplyShortagePersonDays",0}};
    for(const auto& row:s.Market.Areas) { const auto& a=row.second; v["SupplyShortagePersonDays"]+=a.SupplyShortagePersonDays;if(!s.Areas.at(row.first).RegionId)v["UnclaimedSupplyShortagePersonDays"]+=a.SupplyShortagePersonDays;v["CashShortagePersonDays"]+=a.CashShortagePersonDays;v["NutritionDeficitPersonDays"]+=a.NutritionDeficitPersonDays;v["DemographicBirths"]+=static_cast<double>(a.DemographicBirths);v["DemographicDeaths"]+=static_cast<double>(a.DemographicDeaths);v["ExpectedNutritionDeaths"]+=a.ExpectedNutritionDeaths; }
    for(const auto& row:s.Market.Regions) {const auto& r=row.second;v["UnpaidWages"]+=r.UnpaidWages;v["ServiceReceipts"]+=r.ServiceReceipts;v["FoodPurchases"]+=r.FoodPurchases;for(std::size_t g=0;g<MarketGoodCount;++g){v["GoodsProduced"]+=r.Produced[g];v["GoodsUsed"]+=r.Used[g];v["GoodsWasted"]+=r.Wasted[g];v["GoodsStored"]+=r.Stock[g];}}
    for(const auto& row:s.Evolution.Areas){v["FoodProduced"]+=row.second.Produced;v["FoodConsumed"]+=row.second.Consumed;v["FoodWasted"]+=row.second.Wasted;v["FoodSpoiled"]+=row.second.Spoiled;v["FoodCapacityOverflow"]+=row.second.Wasted-row.second.Spoiled;}
    for(const auto& row:s.Bandits.Camps){const auto& c=row.second;v["FoodConsumed"]+=c.FoodConsumed;v["FoodWasted"]+=c.FoodWasted;v["FoodSpoiled"]+=c.FoodSpoiled;v["FoodAbandoned"]+=c.FoodAbandoned;v["FoodCapacityOverflow"]+=std::max(0.0,c.FoodWasted-c.FoodSpoiled-c.FoodAbandoned);}
    for(const char* key:{"BudgetTaxReceipts","PublicServices","EmergencyRelease","SurplusRelease","ServiceShortfall","TreasuryRevenue","TreasurySpending"})v[key]=0;
    for(const auto& row:s.Market.Budgets){const auto& b=row.second;v["BudgetTaxReceipts"]+=b.TaxReceipts;v["PublicServices"]+=b.ServicePaid;v["EmergencyRelease"]+=b.EmergencyReleased;v["SurplusRelease"]+=b.SurplusReleased;v["ServiceShortfall"]+=b.ServiceShortfall;}
    for(const auto& row:s.Evolution.Domains){v["TreasuryRevenue"]+=row.second.Revenue;v["TreasurySpending"]+=row.second.Spent;}
    return v;
}
double People(const Area& a) {double n=0;for(const auto& c:a.Cohorts)n+=static_cast<double>(c.Population);return n;}
}
void ObserveEconomy(const Snapshot& now,const Snapshot& before,bool baseline,Values& m) {
    const auto current=Totals(now), previous=Totals(before);
    for(const char* key:{"publicReserveTarget","militaryReserve","availableAboveCommitments","publicReserveShortfall","smoothedTaxReceipts","dailyMilitaryCommitments"})m[key]=0;
    for(const auto& row:now.Market.Budgets)if(!now.Politics.RetiredDomains.count(row.first)){const auto& b=row.second;const double cash=now.Evolution.Domains.at(row.first).Treasury;
        m["publicReserveTarget"]+=b.ReserveTarget;m["militaryReserve"]+=b.MilitaryReserve;m["availableAboveCommitments"]+=std::max(0.0,cash-b.MilitaryReserve);m["publicReserveShortfall"]+=std::max(0.0,b.ReserveTarget-cash);m["smoothedTaxReceipts"]+=b.SmoothedReceipts;m["dailyMilitaryCommitments"]+=b.MilitaryDaily;}
    m["activeBanditCamps"]=0;m["banditStrength"]=0;m["banditFoodRaided"]=0;m["banditRaids"]=0;m["clearedBanditCamps"]=0;
    for(const auto& row:now.Bandits.Camps){const auto& c=row.second;if(c.Active){++m["activeBanditCamps"];m["banditStrength"]+=c.Strength;}else if(c.Outcome!=3) ++m["clearedBanditCamps"];m["banditFoodRaided"]+=c.FoodRaided;m["banditRaids"]+=static_cast<double>(c.Raids);}
    for(const char* key:{"dispersedBanditCamps","ownerClearedCamps","banditFoodStored","banditLootGrowth","banditStarvationLosses","activeExpeditions","defenseCost","defenseFood","defenseCasualties","defenseMustered"})m[key]=0;
    for(const auto& row:now.Bandits.Camps){const auto& c=row.second;m["dispersedBanditCamps"]+=c.Outcome==3;m["ownerClearedCamps"]+=c.Outcome==2;m["banditFoodStored"]+=c.FoodStored;m["banditLootGrowth"]+=c.LootGrowth;m["banditStarvationLosses"]+=c.StarvationLosses;}
    for(const auto& row:now.Bandits.Expeditions){const auto& x=row.second;m["activeExpeditions"]+=x.Phase!=ExpeditionPhase::Finished;m["defenseCost"]+=x.Cost;m["defenseFood"]+=x.Food;m["defenseCasualties"]+=x.Casualties;m["defenseMustered"]+=x.Mustered;}
    double previousRaids=0;for(const auto& row:before.Bandits.Camps)previousRaids+=row.second.FoodRaided;
    m["intervalBanditFoodRaided"]=baseline?0:m["banditFoodRaided"]-previousRaids;
    const double days=baseline?0:static_cast<double>(now.Day-before.Day);
    m["spoilageStartedDay"]=static_cast<double>(now.Evolution.SpoilageStartedDay);m["annualFoodSpoilage"]=now.Evolution.Policy.AnnualFoodSpoilage;m["intervalDays"]=days;m["diagnosticsStartedDay"]=static_cast<double>(now.Market.DiagnosticsStartedDay);
    for(const auto& row:current){m["total"+row.first]=row.second;m["interval"+row.first]=baseline?0:row.second-previous.at(row.first);}
    m["intervalFoodDeficitPersonDays"]=m["intervalSupplyShortagePersonDays"]+m["intervalCashShortagePersonDays"];
    m["averagePeopleMissingFoodEquivalent"]=days>0?m["intervalFoodDeficitPersonDays"]/days:0;
    const double availableGoods=previous.at("GoodsStored")+m["intervalGoodsProduced"];
    m["intervalGoodsWasteFraction"]=availableGoods>0?m["intervalGoodsWasted"]/availableGoods:0;
    m["economyEnabled"]=now.Market.Policy.Enabled?1:0;
    const double money=MarketMoney(now);double family=0,working=0;std::vector<double> estates;
    std::set<Id> living;for(const auto& c:now.Politics.CharacterFamilies)if(now.Characters.at(c.first).Alive)living.insert(c.second);
    for(const auto& f:now.Politics.Families){family+=f.second.Wealth;if(living.count(f.first))estates.push_back(f.second.Wealth);}
    for(const auto& r:now.Market.Regions)working+=r.second.HouseholdCash+r.second.ProducerCash;
    std::sort(estates.begin(),estates.end(),std::greater<double>());double top=0,active=0;for(double value:estates)active+=value;
    const auto count=static_cast<std::size_t>(std::ceil(estates.size()*0.1));for(std::size_t i=0;i<count;++i)top+=estates[i];
    m["familyWealthShare"]=money>0?family/money:0;m["workingCashShare"]=money>0?working/money:0;m["activeFamilyTopDecileShare"]=active>0?top/active:0;
    m["moneyConservationError"]=now.Market.Policy.Enabled?money-now.Market.InitialMoney:0;
    m["unclaimedPopulation"]=0;m["unclaimedWeightedNutrition"]=0;m["populationBelowNutrition90"]=0;
    m["worstPopulatedRegionFoodCoverage"]=1;m["regionsBelowFood95"]=0;
    for(const auto& row:now.Market.Regions){double n=0;for(const auto& a:now.Areas)if(a.second.RegionId==row.first)n+=People(a.second);if(n>0){m["worstPopulatedRegionFoodCoverage"]=std::min(m["worstPopulatedRegionFoodCoverage"],row.second.FoodCoverage);if(row.second.FoodCoverage<0.95)++m["regionsBelowFood95"];}}
    for(const auto& row:now.Market.Areas){const auto& area=now.Areas.at(row.first);const double n=People(area);if(row.second.Nutrition<0.9)m["populationBelowNutrition90"]+=n;if(!area.RegionId){m["unclaimedPopulation"]+=n;m["unclaimedWeightedNutrition"]+=n*row.second.Nutrition;}}
    if(m["unclaimedPopulation"]>0)m["unclaimedWeightedNutrition"]/=m["unclaimedPopulation"];
}
void WriteEconomyRegions(std::ostream& o,const Snapshot& now,const Snapshot& before,bool baseline) {
    o<<",\"economyRegions\":[";bool comma=false;
    for(const auto& row:now.Market.Regions){
        const Id id=row.first;const auto& r=row.second;double population=0,nutrition=0,stock=0,supply=0,cash=0;
        for(const auto& area:now.Areas)if(area.second.RegionId==id){const double n=People(area.second);population+=n;const auto& a=now.Market.Areas.at(area.first);nutrition+=n*a.Nutrition;stock+=now.Evolution.Areas.at(area.first).FoodStored;const auto old=before.Market.Areas.find(area.first);if(!baseline){supply+=a.SupplyShortagePersonDays-(old==before.Market.Areas.end()?0:old->second.SupplyShortagePersonDays);cash+=a.CashShortagePersonDays-(old==before.Market.Areas.end()?0:old->second.CashShortagePersonDays);}}
        const auto prior=before.Market.Regions.find(id);const double previousCash=prior==before.Market.Regions.end()?0:prior->second.ProducerCash;
        if(comma)o<<',';comma=true;
        o<<"{\"id\":"<<id<<",\"domainId\":"<<now.Regions.at(id).DomainId<<",\"population\":"<<population<<",\"foodCoverage\":"<<r.FoodCoverage<<",\"wageCoverage\":"<<r.WageCoverage<<",\"nutrition\":"<<(population>0?nutrition/population:1)<<",\"foodStockDays\":"<<(population>0?stock/population:0)<<",\"householdCash\":"<<r.HouseholdCash<<",\"producerCash\":"<<r.ProducerCash<<",\"producerWageReserveDays\":"<<(population>0?r.ProducerCash/(population*now.Market.Policy.DailyWage):0)<<",\"producerCashDelta\":"<<(baseline?0:r.ProducerCash-previousCash)<<",\"intervalSupplyShortagePersonDays\":"<<supply<<",\"intervalCashShortagePersonDays\":"<<cash<<'}';
    }o<<']';
    o<<",\"publicBudgets\":";ExportPublicBudgets(now,o);
}
}
