#include "WorldMarket.h"
#include <algorithm>
#include <sstream>

namespace Birthright::Campaign
{
PublicBudgetQuote QuotePublicBudget(const MarketRules& p,double treasury,double receipts,
    double smoothedReceipts,double militaryDaily,double militaryReserve)
{
    PublicBudgetQuote q;
    const double current=p.ServiceIncomeShare*std::max(0.0,receipts-militaryDaily);
    const double normal=p.ServiceIncomeShare*std::max(0.0,smoothedReceipts-militaryDaily);
    q.ReserveTarget=militaryReserve+normal*p.TreasuryReserveDays;
    // Save a bounded share of actual receipts. Existing obligations take priority;
    // service reserves cushion an income shock without promising unfunded payments.
    const double available=std::max(0.0,treasury-militaryReserve);
    q.Emergency=std::min(std::max(0.0,normal-current),std::max(0.0,available-current)/p.EmergencyReleaseDays);
    q.Surplus=std::max(0.0,treasury-q.ReserveTarget)/p.BudgetReleaseDays;
    q.Desired=std::max(current,normal)+q.Surplus;
    q.Services=std::min(available,current+q.Emergency+q.Surplus);
    // Attribute only actual released cash, even when affordability caps the quote.
    q.Emergency=std::min(q.Emergency,std::max(0.0,q.Services-current));
    q.Surplus=std::min(q.Surplus,std::max(0.0,q.Services-current-q.Emergency));
    return q;
}

void AdvancePublicBudgets(Snapshot& s)
{
    auto& m=s.Market;const auto& p=m.Policy;
    std::map<Id,double> upkeep,food,population;
    for(const auto& a:s.Areas)if(a.second.RegionId)for(const auto& c:a.second.Cohorts)
        population[s.Regions.at(a.second.RegionId).DomainId]+=static_cast<double>(c.Population);
    for(const auto& row:s.Politics.Armies)if(!row.second.Expedition){
        const auto& a=row.second;upkeep[a.Domain]+=a.Soldiers*s.Politics.Policy.SoldierUpkeep;
        food[a.Domain]+=a.Soldiers*m.Regions.at(s.Areas.at(a.HomeArea).RegionId).FoodPrice;
    }
    // Player warband food already uses the regional treasury supply path.
    for(const auto& row:s.Troops){const Id region=s.Areas.at(row.second.HomeAreaId).RegionId;
        if(region)food[s.Regions.at(region).DomainId]+=m.Regions.at(region).FoodPrice;}
    for(auto& row:s.Evolution.Domains){
        const Id id=row.first;auto& d=row.second;auto& b=m.Budgets[id];
        if(s.Politics.RetiredDomains.count(id)){
            b.ReceiptsToday=b.SmoothedReceipts=b.ServicesToday=b.DesiredServices=0;
            b.MilitaryDaily=b.MilitaryReserve=b.ReserveTarget=b.AvailableBeforeServices=0;
            continue;
        }
        b.SmoothedReceipts=b.ObservedDays ? b.SmoothedReceipts+(b.ReceiptsToday-b.SmoothedReceipts)/p.IncomeSmoothingDays : b.ReceiptsToday;
        ++b.ObservedDays;b.AvailableBeforeServices=d.Treasury;
        b.MilitaryDaily=upkeep[id]/s.Politics.Policy.TickDays+food[id];
        b.MilitaryReserve=upkeep[id]+food[id]*p.MilitaryReserveDays;
        PublicBudgetQuote q;
        if(p.ReceiptBudgets && p.ResponsiveBudgets)
            q=QuotePublicBudget(p,d.Treasury,b.ReceiptsToday,b.SmoothedReceipts,b.MilitaryDaily,b.MilitaryReserve);
        else {
            // Persisted control path reproduces BR-066 service allocation exactly.
            double daily=population[id]*p.DailyWage*p.TaxRate;
            q.ReserveTarget=(daily+b.MilitaryDaily)*p.TreasuryReserveDays;
            if(p.ResponsiveBudgets)daily+=std::max(0.0,d.Treasury-q.ReserveTarget)/p.BudgetReleaseDays;
            else daily*=0.55;
            q.Desired=daily;q.Services=std::min(d.Treasury,daily);
        }
        b.ServicesToday=q.Services;b.DesiredServices=q.Desired;b.ReserveTarget=q.ReserveTarget;
        b.ServicePaid+=q.Services;b.EmergencyReleased+=q.Emergency;b.SurplusReleased+=q.Surplus;
        b.ServiceShortfall+=std::max(0.0,q.Desired-q.Services);
        d.Treasury-=q.Services;d.Spent+=q.Services;CreditMarketServices(s,id,q.Services);
        if(s.Day%s.Politics.Policy.TickDays==0 && !s.Politics.RetiredDomains.count(id)){
            std::ostringstream detail;detail<<"domain="<<id<<"; receipts="<<b.ReceiptsToday<<"; smoothed="<<b.SmoothedReceipts
                <<"; services="<<q.Services<<"; desired="<<q.Desired<<"; military_daily="<<b.MilitaryDaily
                <<"; military_reserve="<<b.MilitaryReserve<<"; reserve_target="<<b.ReserveTarget<<"; treasury="<<d.Treasury
                <<"; reason="<<(!p.ReceiptBudgets || !p.ResponsiveBudgets?"legacy_budget":q.Services+1e-9<q.Desired?"income_or_commitment_shortfall":q.Surplus>0?"surplus_release":q.Emergency>0?"income_shock_buffer":"receipt_budget");
            s.Events.push_back({"budget/"+std::to_string(s.Day)+"/"+std::to_string(id),"public_budget",s.Day,s.Domains.at(id).HeadCharacterId,0,detail.str()});
        }
    }
}

void ExportPublicBudgets(const Snapshot& s,std::ostream& o)
{
    o<<"[";bool comma=false;
    for(const auto& row:s.Market.Budgets){if(comma)o<<',';comma=true;const auto& b=row.second;const auto& d=s.Evolution.Domains.at(row.first);
        o<<"{\"domainId\":"<<row.first<<",\"active\":"<<(s.Politics.RetiredDomains.count(row.first)?"false":"true")
            <<",\"treasury\":"<<d.Treasury<<",\"totalRevenue\":"<<d.Revenue<<",\"totalSpending\":"<<d.Spent
            <<",\"observedDays\":"<<b.ObservedDays<<",\"taxReceipts\":"<<b.TaxReceipts<<",\"servicePaid\":"<<b.ServicePaid
            <<",\"emergencyReleased\":"<<b.EmergencyReleased<<",\"surplusReleased\":"<<b.SurplusReleased<<",\"serviceShortfall\":"<<b.ServiceShortfall
            <<",\"receiptsToday\":"<<b.ReceiptsToday<<",\"smoothedReceipts\":"<<b.SmoothedReceipts<<",\"servicesToday\":"<<b.ServicesToday
            <<",\"desiredServices\":"<<b.DesiredServices<<",\"militaryDaily\":"<<b.MilitaryDaily<<",\"militaryReserve\":"<<b.MilitaryReserve
            <<",\"reserveTarget\":"<<b.ReserveTarget<<",\"availableBeforeServices\":"<<b.AvailableBeforeServices
            <<",\"availableAboveCommitments\":"<<std::max(0.0,d.Treasury-b.MilitaryReserve)<<",\"reserveShortfall\":"<<std::max(0.0,b.ReserveTarget-d.Treasury)<<'}';
    }o<<']';
}
}
