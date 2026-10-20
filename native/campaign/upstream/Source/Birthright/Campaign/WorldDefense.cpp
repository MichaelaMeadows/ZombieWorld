#include "WorldBandits.h"
#include "WorldPolitics.h"
#include "WorldMarket.h"
#include "WorldTravel.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace Birthright::Campaign
{
namespace
{
Result No(const std::string& reason){return {false,"defense_ineligible",reason};}
double People(const Area& a){double n=0;for(const auto& c:a.Cohorts)n+=static_cast<double>(c.Population);return n;}
Id Owner(const Snapshot& s,Id region){return s.Entities.at(s.Regions.at(region).OwnerEntityId).HeadCharacterId;}
bool AtWar(const Snapshot& s,Id domain){for(const auto& w:s.Politics.Wars)if(w.second.Active && (w.second.Attacker==domain || w.second.Defender==domain))return true;return false;}
bool LivingAdult(const Snapshot& s,Id c){return s.Characters.count(c) && s.Characters.at(c).Alive && s.Day-s.Characters.at(c).BornDay>=s.Policy.AdultAgeDays;}
std::int64_t LegDays(const Snapshot& s,Id a,Id b){return std::max<std::int64_t>(1,static_cast<std::int64_t>(std::ceil(TravelDurationDays(s,a,b))));}
void DefenseEvent(Snapshot& s,const char* kind,Id expedition,Id lord,const std::string& detail){s.Events.push_back({"defense/"+std::to_string(s.Day)+"/"+kind+"/"+std::to_string(expedition),kind,s.Day,lord,0,detail});}
struct Plan {Id Region=0,Domain=0,Family=0,Home=0;unsigned Soldiers=0;std::vector<Id> Route;};
Result Quote(const Snapshot& s,Id lord,Id camp,Plan& plan)
{
    const auto& b=s.Bandits;const auto& p=b.Policy;
    if(!p.Enabled || !s.Market.Policy.Enabled || !s.Politics.Policy.Enabled)return No("systems_disabled");
    if(b.Expeditions.size()>=100000 || b.NextExpedition>=1000000000ULL)return No("history_capacity");
    const auto found=b.Camps.find(camp);
    if(found==b.Camps.end() || !found->second.Active || !found->second.LastTargetArea)return No("no_recent_raid_target");
    const auto& c=found->second;
    if(b.Engagement.CampId==camp)return No("player_engaged");
    if(s.Day-c.LastRaidDay>p.ResponseDays*2 || s.Day<c.SuppressedUntil)return No("threat_not_current");
    const Id region=s.Areas.at(c.LastTargetArea).RegionId;
    if(!region || Owner(s,region)!=lord || !LivingAdult(s,lord) || !s.Politics.CharacterFamilies.count(lord))return No("no_authorized_adult_owner");
    if(std::find(s.Evolution.ProtectedCharacters.begin(),s.Evolution.ProtectedCharacters.end(),lord)!=s.Evolution.ProtectedCharacters.end())return No("player_controlled_owner");
    plan.Region=region;plan.Domain=s.Regions.at(region).DomainId;plan.Family=s.Politics.CharacterFamilies.at(lord);
    if(AtWar(s,plan.Domain))return No("realm_at_war");
    for(const auto& row:b.Expeditions){const auto& x=row.second;
        if(x.Phase!=ExpeditionPhase::Finished && (x.Camp==camp || x.Region==region || x.Family==plan.Family))return No("force_already_committed");
        if(x.Phase==ExpeditionPhase::Finished && x.Region==region && s.Day-x.FinishedDay<p.DefenseCooldownDays)return No("recovery_cooldown");
    }
    unsigned committed=0;for(const auto& a:s.Politics.Armies)if(a.second.Lord==lord)committed+=a.second.Soldiers;
    const auto capacity=TroopCapacity(s,lord)*20;
    const unsigned need=static_cast<unsigned>(std::ceil(std::max(3.0,c.Strength)*p.DefenseStrengthRatio))+2;
    if(committed>=capacity || need>capacity-committed)return No("command_capacity");
    double stock=0,pop=0;for(const auto& a:s.Areas)if(a.second.RegionId==region){stock+=s.Evolution.Areas.at(a.first).FoodStored;pop+=People(a.second);}
    if(stock<pop*p.DefenseFoodReserveDays+need)return No("civilian_food_reserve");
    double best=1e30;std::string reason="local_manpower_or_route";
    for(Id home:BanditReach(s,c.AreaId,p.RaidHops)){
        const auto& a=s.Areas.at(home);if(a.RegionId!=region)continue;
        unsigned available=0;for(const auto& cohort:a.Cohorts)available+=static_cast<unsigned>(std::min(100000.0,std::floor(cohort.Population*s.Politics.Policy.LevyFraction)));
        if(available<need)continue;
        std::vector<Id> route;if(!FindRoute(s,home,c.AreaId,route))continue;route.insert(route.begin(),home);
        bool permitted=true;double days=0;
        for(std::size_t i=0;i<route.size();++i){const Id r=s.Areas.at(route[i]).RegionId;
            if(r && s.Regions.at(r).DomainId!=plan.Domain)permitted=false;
            if(i)days+=LegDays(s,route[i-1],route[i]);
        }
        if(!permitted || days>30)continue;
        const double duration=2*days+p.DefenseBattleDays*p.DefenseMaxRounds+2;
        const double budget=need*(s.Politics.Policy.MusterCost+duration*(s.Politics.Policy.SoldierUpkeep/s.Politics.Policy.TickDays+s.Market.Policy.FoodBasePrice*4));
        const auto wealth=s.Politics.Families.at(plan.Family).Wealth;
        if(budget>std::min(wealth*0.5,std::max(0.0,wealth-p.DefenseWealthReserve))){reason="family_campaign_budget";continue;}
        if(days<best){best=days;plan.Home=home;plan.Route=std::move(route);plan.Soldiers=need;}
    }
    return plan.Home?Result{true,"accepted",""}:No(reason);
}
void BeginReturn(Snapshot& s,CampExpedition& x,const std::string& why)
{
    x.Phase=ExpeditionPhase::Returning;
    x.NextActionDay=s.Day+(x.Position?LegDays(s,x.Route[x.Position],x.Route[x.Position-1]):1);
    DefenseEvent(s,"defense_returning",x.Key,x.Lord,"camp="+std::to_string(x.Camp)+"; reason="+why);
}
void Finish(Snapshot& s,CampExpedition& x)
{
    const auto survivors=ForceStrength(s,0,x.Domain,x.Key);
    ApplyForceLosses(s,0,x.Domain,survivors,false,x.Key);x.Phase=ExpeditionPhase::Finished;x.FinishedDay=s.Day;
    DefenseEvent(s,"defense_returned",x.Key,x.Lord,"camp="+std::to_string(x.Camp)+"; survivors="+std::to_string(survivors)+"; casualties="+std::to_string(x.Casualties)+"; cost="+std::to_string(x.Cost)+"; food="+std::to_string(x.Food));
}
}
Result World::DispatchCampExpedition(const std::string& operation,Id owner,Id camp)
{
    auto valid=CheckOperation(operation);if(!valid)return valid;
    Plan plan;valid=Quote(StateData,owner,camp,plan);if(!valid)return valid;
    auto next=BeginMutation();auto& b=next.Bandits;
    CampExpedition x;x.Key=b.NextExpedition++;x.Camp=camp;x.Domain=plan.Domain;x.Region=plan.Region;x.Lord=owner;x.Family=plan.Family;x.HomeArea=plan.Home;x.Route=plan.Route;x.Mustered=plan.Soldiers;x.StartedDay=next.Day;
    x.NextActionDay=next.Day+(x.Route.size()>1?LegDays(next,x.Route[0],x.Route[1]):1);
    auto& family=next.Politics.Families.at(x.Family);x.Cost=x.Mustered*next.Politics.Policy.MusterCost;
    family.Wealth-=x.Cost;family.Spent+=x.Cost;next.Market.Regions.at(x.Region).ProducerCash+=x.Cost;next.Market.Regions.at(x.Region).ServiceReceipts+=x.Cost;
    unsigned left=x.Mustered;
    for(const auto& c:next.Areas.at(x.HomeArea).Cohorts){const auto take=static_cast<unsigned>(std::min<double>(left,std::floor(c.Population*next.Politics.Policy.LevyFraction)));if(!take)continue;
        EnlistContingent(next,0,x.Key,x.Domain,x.Lord,x.HomeArea,c.CultureId,take);left-=take;
    }
    b.Expeditions.emplace(x.Key,x);
    next.Events.push_back({operation,"defense_dispatched",next.Day,owner,0,"expedition="+std::to_string(x.Key)+"; camp="+std::to_string(camp)+"; region="+std::to_string(x.Region)+"; family="+std::to_string(x.Family)+"; soldiers="+std::to_string(x.Mustered)+"; cost="+std::to_string(x.Cost)+"; first_arrival="+std::to_string(x.NextActionDay)});
    return Commit(std::move(next));
}
Result AdvanceDefense(World& world)
{
    auto& s=world.StateData;auto& b=s.Bandits;const auto p=b.Policy;
    if(!s.Market.Policy.Enabled || !s.Politics.Policy.Enabled)return {true,"accepted",""};
    for(auto& row:b.Expeditions){auto& x=row.second;if(x.Phase==ExpeditionPhase::Finished)continue;
        const auto n=ForceStrength(s,0,x.Domain,x.Key);
        if(!n){Finish(s,x);continue;}
        auto& f=s.Politics.Families.at(x.Family);
        const double due=n*s.Politics.Policy.SoldierUpkeep/s.Politics.Policy.TickDays,paid=std::min(f.Wealth,due);
        f.Wealth-=paid;f.Spent+=paid;s.Market.Regions.at(x.Region).ProducerCash+=paid;s.Market.Regions.at(x.Region).ServiceReceipts+=paid;x.Cost+=paid;
        double foodCost=0;const double food=PurchaseDefenseFood(s,x.Region,x.Family,static_cast<double>(n),foodCost);x.Food+=food;x.Cost+=foodCost;
        x.Readiness=std::clamp(x.Readiness*0.8+0.2*(paid/due)*(food/n),0.1,1.0);
        if(s.Day<x.NextActionDay)continue;
        if(x.Phase==ExpeditionPhase::Returning){if(x.Position)--x.Position;if(!x.Position){Finish(s,x);continue;}x.NextActionDay=s.Day+LegDays(s,x.Route[x.Position],x.Route[x.Position-1]);continue;}
        if(x.Phase==ExpeditionPhase::Outbound && x.Position+1<x.Route.size())++x.Position;
        auto& camp=b.Camps.at(x.Camp);
        if(!camp.Active || b.Engagement.CampId==x.Camp || !LivingAdult(s,x.Lord) || Owner(s,x.Region)!=x.Lord || s.Regions.at(x.Region).DomainId!=x.Domain || AtWar(s,x.Domain) || x.Readiness<0.5){BeginReturn(s,x,!camp.Active?"camp_resolved":x.Readiness<0.5?"insufficient_pay_or_food":"authority_or_commitment_changed");continue;}
        bool foreign=false;for(Id a:x.Route){const Id r=s.Areas.at(a).RegionId;if(r && s.Regions.at(r).DomainId!=x.Domain)foreign=true;}
        if(foreign){BeginReturn(s,x,"route_became_foreign");continue;}
        if(x.Phase==ExpeditionPhase::Outbound){
            if(x.Position+1<x.Route.size()){x.NextActionDay=s.Day+LegDays(s,x.Route[x.Position],x.Route[x.Position+1]);continue;}
            x.Phase=ExpeditionPhase::Fighting;x.NextActionDay=s.Day+p.DefenseBattleDays;DefenseEvent(s,"defense_arrived",x.Key,x.Lord,"camp="+std::to_string(x.Camp));continue;
        }
        const double enemy=camp.Strength;
        const auto loss=StrategicLosses(n,enemy,x.Readiness,1,s.Politics.Policy.CasualtyFraction);
        const double banditLoss=static_cast<double>(StrategicLosses(static_cast<std::uint64_t>(std::ceil(enemy)),static_cast<double>(n),1,x.Readiness,s.Politics.Policy.CasualtyFraction));
        ApplyForceLosses(s,0,x.Domain,loss,true,x.Key);x.Casualties+=static_cast<unsigned>(loss);camp.Strength=std::max(0.0,enemy-banditLoss);++x.Rounds;
        const auto remaining=ForceStrength(s,0,x.Domain,x.Key);
        DefenseEvent(s,"defense_battle",x.Key,x.Lord,"camp="+std::to_string(x.Camp)+"; loss="+std::to_string(loss)+"; soldiers="+std::to_string(remaining)+"; bandit_strength="+std::to_string(camp.Strength)+"; readiness="+std::to_string(x.Readiness));
        if(camp.Strength<0.5 || !remaining || x.Rounds>=p.DefenseMaxRounds){
            const bool won=camp.Strength<0.5 || remaining*x.Readiness>camp.Strength;
            if(won){CloseBanditCamp(s,x.Camp,2);DefenseEvent(s,"bandit_owner_cleared",x.Key,x.Lord,"camp="+std::to_string(x.Camp));}
            else {camp.SuppressedUntil=s.Day+p.RaidDays;camp.LastRaidDay=s.Day;}
            BeginReturn(s,x,won?"victory":"defeat");
        }else x.NextActionDay=s.Day+p.DefenseBattleDays;
    }
    if(p.OwnerDefense && s.Day%p.ResponseDays==0){
        std::vector<Id> camps;for(const auto& c:b.Camps)if(c.second.Active && c.second.LastTargetArea)camps.push_back(c.first);
        for(Id camp:camps){const Id region=s.Areas.at(b.Camps.at(camp).LastTargetArea).RegionId;const Id owner=region?Owner(s,region):0;
            auto result=world.DispatchCampExpedition("defense/"+std::to_string(s.Day)+"/dispatch/"+std::to_string(camp),owner,camp);
            if(!result){if(result.Code!="defense_ineligible")return result;DefenseEvent(s,"defense_deferred",camp,owner,"camp="+std::to_string(camp)+"; region="+std::to_string(region)+"; reason="+result.Detail);}
        }
    }
    return {true,"accepted",""};
}
Result ValidateDefense(const Snapshot& s)
{
    const auto& b=s.Bandits;const auto bad=[](){return Result{false,"defense_invalid","Expedition identity, route, time or manpower ledger"};};
    if(!b.NextExpedition || b.Expeditions.size()>100000)return bad();std::set<Id> camps,regions,families;
    for(const auto& row:b.Expeditions){const auto& x=row.second;
        if(!x.Key || x.Key!=row.first || x.Key>=b.NextExpedition || !b.Camps.count(x.Camp) || !s.Domains.count(x.Domain) || !s.Regions.count(x.Region) || !s.Characters.count(x.Lord) || !s.Politics.Families.count(x.Family) || !s.Areas.count(x.HomeArea) || x.Route.empty() || x.Route.front()!=x.HomeArea || x.Route.back()!=b.Camps.at(x.Camp).AreaId || x.Position>=x.Route.size() || static_cast<unsigned>(x.Phase)>3 || x.Rounds>b.Policy.DefenseMaxRounds || !x.Mustered || x.Mustered>100000 || x.Casualties>x.Mustered || x.StartedDay<0 || x.StartedDay>s.Day || x.NextActionDay<x.StartedDay || x.NextActionDay>s.Day+100000 || x.FinishedDay<0 || x.FinishedDay>s.Day || !std::isfinite(x.Readiness) || x.Readiness<0.1 || x.Readiness>1 || !std::isfinite(x.Cost) || x.Cost<0 || !std::isfinite(x.Food) || x.Food<0)return bad();
        for(std::size_t i=0;i<x.Route.size();++i){if(!s.Areas.count(x.Route[i]) || s.Areas.at(x.Route[i]).IsWater)return bad();if(i){const auto& neighbors=s.Areas.at(x.Route[i-1]).Neighbors;if(std::find(neighbors.begin(),neighbors.end(),x.Route[i])==neighbors.end())return bad();}}
        const auto n=ForceStrength(s,0,x.Domain,x.Key);
        if(!s.Politics.CharacterFamilies.count(x.Lord) || s.Politics.CharacterFamilies.at(x.Lord)!=x.Family)return bad();
        for(const auto& army:s.Politics.Armies) if(army.second.Expedition==x.Key && (army.second.Lord!=x.Lord || army.second.HomeArea!=x.HomeArea))return bad();
        if(x.Phase!=ExpeditionPhase::Finished){if(!camps.insert(x.Camp).second || !regions.insert(x.Region).second || !families.insert(x.Family).second || n+x.Casualties!=x.Mustered || x.FinishedDay)return bad();}
        else if(n || x.FinishedDay<x.StartedDay)return bad();
    }
    return {true,"accepted",""};
}
}
