#include "WorldMarket.h"
#include <istream>
#include <ostream>
#include <set>

namespace Birthright::Campaign
{
void WriteMarket(const Snapshot& s,std::ostream& o) {
    const auto& m=s.Market; const auto& p=m.Policy;
    o<<"MARKET_METADATA "<<p.Version<<' '<<p.Enabled<<' '<<p.InitialCashPerPerson<<' '<<p.DailyWage<<' '<<p.TaxRate<<' '<<p.FoodBasePrice<<' '<<p.SubsistenceShare<<' '<<p.TradeDailyFraction<<' '<<p.ReserveDays<<' '<<p.InvestmentFraction<<' '<<p.ConstructionDays<<'\n';
    o<<m.InitialMoney<<' '<<m.FoodTraded<<' '<<m.Taxes<<' '<<m.Wages<<' '<<m.Rents<<' '<<m.Investment<<' '<<m.Inherited<<' '<<m.ArmyFood<<'\n';
    for(const auto& recipe:p.Recipes) { o<<recipe.OutputPerWorker; for(double input:recipe.Inputs)o<<' '<<input; o<<'\n'; }
    o<<m.Regions.size()<<'\n'; for(const auto& row:m.Regions) { const auto& r=row.second; o<<row.first<<' '<<r.HouseholdCash<<' '<<r.ProducerCash<<' '<<r.FoodPrice<<' '<<r.FoodCoverage<<' '<<r.WageCoverage<<' '<<r.ArmySupply<<' '<<r.PublicCapital<<' '<<r.HarvestFactor<<' '<<r.ShockUntil<<' '<<r.TradeBlockedUntil; for(std::size_t g=0;g<MarketGoodCount;++g) o<<' '<<r.Stock[g]<<' '<<r.Produced[g]<<' '<<r.Used[g]<<' '<<r.Wasted[g]; o<<'\n'; }
    o<<m.Areas.size()<<'\n'; for(const auto& a:m.Areas) o<<a.first<<' '<<a.second.LaborCapacity<<' '<<a.second.FoodImported<<' '<<a.second.FoodExported<<'\n';
    o<<m.Estates.size()<<'\n'; for(const auto& a:m.Estates) o<<a.first.first<<' '<<a.first.second<<' '<<a.second.Capital<<' '<<a.second.PendingCapital<<' '<<a.second.ReadyDay<<' '<<a.second.LastInvestmentDay<<'\n';
    o<<m.Wants.size()<<'\n'; for(const auto& w:m.Wants) o<<w.first<<' '<<w.second.Income<<' '<<w.second.Status<<' '<<w.second.Security<<' '<<w.second.Autonomy<<'\n';
    o<<m.Readiness.size()<<'\n'; for(const auto& r:m.Readiness) o<<r.first.first<<' '<<r.first.second<<' '<<r.second<<'\n';
    o<<m.Links.size()<<'\n'; for(const auto& l:m.Links) o<<l.First<<' '<<l.Second<<' '<<l.Distance<<'\n';
    o<<"MARKET_STABILITY "<<p.ResponsiveBudgets<<' '<<p.PersistentNutrition<<' '<<p.DemandProduction<<' '<<p.ProducerReserveDays<<' '<<p.TreasuryReserveDays<<' '<<p.BudgetReleaseDays<<' '<<p.GoodsTargetDays<<' '<<p.NutritionLossDays<<' '<<p.NutritionRecoveryDays<<' '<<p.StarvationThreshold<<'\n';
    o<<m.Regions.size()<<'\n'; for(const auto& row:m.Regions) { o<<row.first; for(std::size_t g=0;g<MarketGoodCount;++g) o<<' '<<row.second.DailyUse[g]<<' '<<row.second.LastObservedUse[g]; o<<'\n'; }
    o<<m.Areas.size()<<'\n'; for(const auto& row:m.Areas) o<<row.first<<' '<<row.second.Nutrition<<' '<<row.second.ShortageDays<<'\n';
    o<<"MARKET_DIAGNOSTICS "<<m.DiagnosticsStartedDay<<'\n';
    o<<m.Regions.size()<<'\n'; for(const auto& row:m.Regions) o<<row.first<<' '<<row.second.UnpaidWages<<' '<<row.second.ServiceReceipts<<' '<<row.second.FoodPurchases<<'\n';
    o<<m.Areas.size()<<'\n'; for(const auto& row:m.Areas) { const auto& a=row.second; o<<row.first<<' '<<a.SupplyShortagePersonDays<<' '<<a.CashShortagePersonDays<<' '<<a.NutritionDeficitPersonDays<<' '<<a.DemographicBirths<<' '<<a.DemographicDeaths<<' '<<a.ExpectedNutritionDeaths<<'\n'; }
    o<<"MARKET_BUDGETS "<<p.ReceiptBudgets<<' '<<p.ServiceIncomeShare<<' '<<p.IncomeSmoothingDays<<' '<<p.EmergencyReleaseDays<<' '<<p.MilitaryReserveDays<<' '<<m.BudgetStartedDay<<' '<<m.BudgetOpeningTaxes<<'\n';
    o<<m.Budgets.size()<<'\n';for(const auto& row:m.Budgets){const auto& b=row.second;
        o<<row.first<<' '<<b.ObservedDays<<' '<<b.TaxReceipts<<' '<<b.ServicePaid<<' '<<b.EmergencyReleased<<' '<<b.SurplusReleased<<' '<<b.ServiceShortfall<<' '<<b.ReceiptsToday<<' '<<b.SmoothedReceipts<<' '<<b.ServicesToday<<' '<<b.DesiredServices<<' '<<b.MilitaryDaily<<' '<<b.MilitaryReserve<<' '<<b.ReserveTarget<<' '<<b.AvailableBeforeServices<<'\n';
    }



}
Result ReadMarket(std::istream& in,Snapshot& s) {
    auto& m=s.Market; auto& p=m.Policy; std::string tag; unsigned enabled=0; std::size_t n=0;
    const auto bad=[] { return Result{false,"market_save_invalid","Malformed/oversized market record."}; };
    const auto count=[&](std::size_t limit) { return static_cast<bool>(in>>n) && n<=limit; };
    if(!(in>>tag>>p.Version>>enabled>>p.InitialCashPerPerson>>p.DailyWage>>p.TaxRate>>p.FoodBasePrice>>p.SubsistenceShare>>p.TradeDailyFraction>>p.ReserveDays>>p.InvestmentFraction>>p.ConstructionDays) || tag!="MARKET_METADATA" || enabled>1 || p.Version<1 || p.Version>4) return bad();
    p.Enabled=enabled!=0; if(!(in>>m.InitialMoney>>m.FoodTraded>>m.Taxes>>m.Wages>>m.Rents>>m.Investment>>m.Inherited>>m.ArmyFood)) return bad();
    for(auto& recipe:p.Recipes) { if(!(in>>recipe.OutputPerWorker)) return bad(); for(auto& input:recipe.Inputs) if(!(in>>input)) return bad(); }
    if(!count(s.Regions.size())) return bad(); for(std::size_t i=0;i<n;++i) { Id id; RegionalMarket r; if(!(in>>id>>r.HouseholdCash>>r.ProducerCash>>r.FoodPrice>>r.FoodCoverage>>r.WageCoverage>>r.ArmySupply>>r.PublicCapital>>r.HarvestFactor>>r.ShockUntil>>r.TradeBlockedUntil)) return bad(); for(std::size_t g=0;g<MarketGoodCount;++g) if(!(in>>r.Stock[g]>>r.Produced[g]>>r.Used[g]>>r.Wasted[g])) return bad(); if(!m.Regions.emplace(id,r).second) return bad(); }
    if(!count(s.Areas.size())) return bad(); for(std::size_t i=0;i<n;++i) { Id id; MarketArea a; if(!(in>>id>>a.LaborCapacity>>a.FoodImported>>a.FoodExported) || !m.Areas.emplace(id,a).second) return bad(); }
    if(!count(1000000)) return bad(); for(std::size_t i=0;i<n;++i) { Id family,region; EstateAsset a; if(!(in>>family>>region>>a.Capital>>a.PendingCapital>>a.ReadyDay>>a.LastInvestmentDay) || !m.Estates.emplace(std::make_pair(family,region),a).second) return bad(); }
    if(!count(s.Characters.size())) return bad(); for(std::size_t i=0;i<n;++i) { Id id; CharacterWants w; if(!(in>>id>>w.Income>>w.Status>>w.Security>>w.Autonomy) || !m.Wants.emplace(id,w).second) return bad(); }
    if(!count(200000)) return bad(); for(std::size_t i=0;i<n;++i) { Id war,domain; double value; if(!(in>>war>>domain>>value) || !m.Readiness.emplace(std::make_pair(war,domain),value).second) return bad(); }
    if(!count(1000000)) return bad(); for(std::size_t i=0;i<n;++i) { TradeLink l; if(!(in>>l.First>>l.Second>>l.Distance)) return bad(); m.Links.push_back(l); }
    if(p.Version>=2) {
        unsigned budgets=0,nutrition=0,demand=0;
        if(!(in>>tag>>budgets>>nutrition>>demand>>p.ProducerReserveDays>>p.TreasuryReserveDays>>p.BudgetReleaseDays>>p.GoodsTargetDays>>p.NutritionLossDays>>p.NutritionRecoveryDays>>p.StarvationThreshold) || tag!="MARKET_STABILITY" || budgets>1 || nutrition>1 || demand>1) return bad();
        p.ResponsiveBudgets=budgets!=0; p.PersistentNutrition=nutrition!=0; p.DemandProduction=demand!=0;
        std::set<Id> seen;
        if(!count(m.Regions.size()) || n!=m.Regions.size()) return bad();
        for(std::size_t i=0;i<n;++i) { Id id; if(!(in>>id) || !m.Regions.count(id) || !seen.insert(id).second) return bad(); auto& r=m.Regions.at(id); for(std::size_t g=0;g<MarketGoodCount;++g) if(!(in>>r.DailyUse[g]>>r.LastObservedUse[g])) return bad(); }
        seen.clear();
        if(!count(m.Areas.size()) || n!=m.Areas.size()) return bad();
        for(std::size_t i=0;i<n;++i) { Id id; if(!(in>>id) || !m.Areas.count(id) || !seen.insert(id).second) return bad(); auto& a=m.Areas.at(id); if(!(in>>a.Nutrition>>a.ShortageDays)) return bad(); }
    } else {
        // Historical consumption is not one day's demand; start migration with neutral health.
        for(auto& row:m.Regions) row.second.LastObservedUse=row.second.Used;
    }
    if(p.Version>=3) {
        if(!(in>>tag>>m.DiagnosticsStartedDay) || tag!="MARKET_DIAGNOSTICS") return bad();
        std::set<Id> seen;
        if(!count(m.Regions.size()) || n!=m.Regions.size()) return bad();
        for(std::size_t i=0;i<n;++i) { Id id; if(!(in>>id) || !m.Regions.count(id) || !seen.insert(id).second) return bad(); auto& r=m.Regions.at(id); if(!(in>>r.UnpaidWages>>r.ServiceReceipts>>r.FoodPurchases)) return bad(); }
        seen.clear();
        if(!count(m.Areas.size()) || n!=m.Areas.size()) return bad();
        for(std::size_t i=0;i<n;++i) { Id id; if(!(in>>id) || !m.Areas.count(id) || !seen.insert(id).second) return bad(); auto& a=m.Areas.at(id); if(!(in>>a.SupplyShortagePersonDays>>a.CashShortagePersonDays>>a.NutritionDeficitPersonDays>>a.DemographicBirths>>a.DemographicDeaths>>a.ExpectedNutritionDeaths)) return bad(); }
    } else m.DiagnosticsStartedDay=p.Enabled?s.Day:0;
    if(p.Version>=4){
        unsigned receipts=0;
        if(!(in>>tag>>receipts>>p.ServiceIncomeShare>>p.IncomeSmoothingDays>>p.EmergencyReleaseDays>>p.MilitaryReserveDays>>m.BudgetStartedDay>>m.BudgetOpeningTaxes) || tag!="MARKET_BUDGETS" || receipts>1) return bad();
        p.ReceiptBudgets=receipts!=0;
        if(!count(s.Evolution.Domains.size()))return bad();
        for(std::size_t i=0;i<n;++i){Id id;PublicBudget b;
            if(!(in>>id>>b.ObservedDays>>b.TaxReceipts>>b.ServicePaid>>b.EmergencyReleased>>b.SurplusReleased>>b.ServiceShortfall>>b.ReceiptsToday>>b.SmoothedReceipts>>b.ServicesToday>>b.DesiredServices>>b.MilitaryDaily>>b.MilitaryReserve>>b.ReserveTarget>>b.AvailableBeforeServices) || !m.Budgets.emplace(id,b).second)return bad();
        }
    }else{m.BudgetStartedDay=p.Enabled?s.Day:0;m.BudgetOpeningTaxes=m.Taxes;}
    p.Version=4;
    return {true,"accepted",""};
}
void ExportMarket(const Snapshot& s,std::ostream& o) {
    const auto& m=s.Market; const auto& p=m.Policy;
    o<<",\n\"market\":{\"enabled\":"<<(p.Enabled?"true":"false")<<",\"policy\":{\"version\":"<<p.Version<<",\"initialCashPerPerson\":"<<p.InitialCashPerPerson<<",\"dailyWage\":"<<p.DailyWage<<",\"taxRate\":"<<p.TaxRate<<",\"foodBasePrice\":"<<p.FoodBasePrice<<",\"subsistenceShare\":"<<p.SubsistenceShare<<",\"tradeDailyFraction\":"<<p.TradeDailyFraction<<",\"reserveDays\":"<<p.ReserveDays<<",\"investmentFraction\":"<<p.InvestmentFraction<<",\"constructionDays\":"<<p.ConstructionDays<<",\"responsiveBudgets\":"<<(p.ResponsiveBudgets?"true":"false")<<",\"persistentNutrition\":"<<(p.PersistentNutrition?"true":"false")<<",\"demandProduction\":"<<(p.DemandProduction?"true":"false")<<",\"producerReserveDays\":"<<p.ProducerReserveDays<<",\"treasuryReserveDays\":"<<p.TreasuryReserveDays<<",\"budgetReleaseDays\":"<<p.BudgetReleaseDays<<",\"goodsTargetDays\":"<<p.GoodsTargetDays<<",\"nutritionLossDays\":"<<p.NutritionLossDays<<",\"nutritionRecoveryDays\":"<<p.NutritionRecoveryDays<<",\"starvationThreshold\":"<<p.StarvationThreshold<<",\"receiptBudgets\":"<<(p.ReceiptBudgets?"true":"false")<<",\"serviceIncomeShare\":"<<p.ServiceIncomeShare<<",\"incomeSmoothingDays\":"<<p.IncomeSmoothingDays<<",\"emergencyReleaseDays\":"<<p.EmergencyReleaseDays<<",\"militaryReserveDays\":"<<p.MilitaryReserveDays<<"},\"money\":"<<MarketMoney(s)<<",\"initialMoney\":"<<m.InitialMoney<<",\"diagnosticsStartedDay\":"<<m.DiagnosticsStartedDay<<",\"foodTraded\":"<<m.FoodTraded<<",\"taxes\":"<<m.Taxes<<",\"wages\":"<<m.Wages<<",\"rents\":"<<m.Rents<<",\"investment\":"<<m.Investment<<",\"inherited\":"<<m.Inherited<<",\"armyFood\":"<<m.ArmyFood<<",\"regions\":[";
    bool comma=false; for(const auto& row:m.Regions) { if(comma)o<<','; comma=true; const auto& r=row.second; o<<"{\"id\":"<<row.first<<",\"householdCash\":"<<r.HouseholdCash<<",\"unpaidWages\":"<<r.UnpaidWages<<",\"serviceReceipts\":"<<r.ServiceReceipts<<",\"foodPurchases\":"<<r.FoodPurchases<<",\"producerCash\":"<<r.ProducerCash<<",\"foodPrice\":"<<r.FoodPrice<<",\"foodCoverage\":"<<r.FoodCoverage<<",\"wageCoverage\":"<<r.WageCoverage<<",\"armySupply\":"<<r.ArmySupply<<",\"publicCapital\":"<<r.PublicCapital<<",\"harvestFactor\":"<<r.HarvestFactor<<",\"shockUntil\":"<<r.ShockUntil<<",\"tradeBlockedUntil\":"<<r.TradeBlockedUntil<<",\"goods\":["; for(std::size_t g=0;g<MarketGoodCount;++g) { if(g)o<<','; o<<"{\"kind\":"<<g<<",\"stock\":"<<r.Stock[g]<<",\"produced\":"<<r.Produced[g]<<",\"used\":"<<r.Used[g]<<",\"wasted\":"<<r.Wasted[g]<<",\"dailyUse\":"<<r.DailyUse[g]<<'}'; } o<<"]}"; }
    o<<"],\"budgetStartedDay\":"<<m.BudgetStartedDay<<",\"budgetOpeningTaxes\":"<<m.BudgetOpeningTaxes<<",\"publicBudgets\":";ExportPublicBudgets(s,o);
    o<<",\"estates\":["; comma=false; for(const auto& a:m.Estates) { if(comma)o<<','; comma=true; o<<"{\"family\":"<<a.first.first<<",\"region\":"<<a.first.second<<",\"capital\":"<<a.second.Capital<<",\"pending\":"<<a.second.PendingCapital<<",\"readyDay\":"<<a.second.ReadyDay<<'}'; }
    o<<"],\"wants\":["; comma=false; for(const auto& w:m.Wants) { if(comma)o<<','; comma=true; o<<"{\"character\":"<<w.first<<",\"income\":"<<w.second.Income<<",\"status\":"<<w.second.Status<<",\"security\":"<<w.second.Security<<",\"autonomy\":"<<w.second.Autonomy<<'}'; }
    o<<"],\"readiness\":["; comma=false; for(const auto& r:m.Readiness) { if(comma)o<<','; comma=true; o<<"{\"war\":"<<r.first.first<<",\"domain\":"<<r.first.second<<",\"value\":"<<r.second<<'}'; }
    o<<"],\"links\":["; comma=false; for(const auto& l:m.Links) { if(comma)o<<','; comma=true; o<<"{\"from\":"<<l.First<<",\"to\":"<<l.Second<<",\"distance\":"<<l.Distance<<'}'; }
    o<<"],\"recipes\":["; for(std::size_t g=0;g<MarketGoodCount;++g) { if(g)o<<','; o<<"{\"good\":"<<g<<",\"outputPerWorker\":"<<p.Recipes[g].OutputPerWorker<<",\"inputs\":["; for(std::size_t i=0;i<MarketGoodCount;++i) { if(i)o<<','; o<<p.Recipes[g].Inputs[i]; } o<<"]}"; }
    o<<"],\"areaAccounts\":["; comma=false; for(const auto& a:m.Areas) { if(comma)o<<','; comma=true; o<<"{\"area\":"<<a.first<<",\"laborCapacity\":"<<a.second.LaborCapacity<<",\"foodImported\":"<<a.second.FoodImported<<",\"foodExported\":"<<a.second.FoodExported<<",\"nutrition\":"<<a.second.Nutrition<<",\"shortageDays\":"<<a.second.ShortageDays<<",\"supplyShortagePersonDays\":"<<a.second.SupplyShortagePersonDays<<",\"cashShortagePersonDays\":"<<a.second.CashShortagePersonDays<<",\"nutritionDeficitPersonDays\":"<<a.second.NutritionDeficitPersonDays<<",\"demographicBirths\":"<<a.second.DemographicBirths<<",\"demographicDeaths\":"<<a.second.DemographicDeaths<<",\"expectedNutritionDeaths\":"<<a.second.ExpectedNutritionDeaths<<'}'; } o<<"]}";
}
}
