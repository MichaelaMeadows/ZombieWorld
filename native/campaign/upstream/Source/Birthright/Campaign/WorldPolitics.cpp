#include "WorldPolitics.h"
#include "WorldMarket.h"
#include "WorldTraversal.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <sstream>

namespace Birthright::Campaign
{
namespace
{
Result Ok() { return {true,"accepted",""}; }
Result No(const std::string& text) { return {false,"politics_ineligible",text}; }
bool Bounded(double n, double lo, double hi) { return std::isfinite(n) && n >= lo && n <= hi; }
bool Adult(const Snapshot& s, Id id) { return s.Characters.count(id) && s.Characters.at(id).Alive && s.Day-s.Characters.at(id).BornDay >= s.Policy.AdultAgeDays; }
bool Protected(const Snapshot& s, Id id) { const auto& p=s.Evolution.ProtectedCharacters; return std::find(p.begin(),p.end(),id)!=p.end(); }
Id DomainAt(const Snapshot& s, Id area) { const Id r=s.Areas.at(area).RegionId; return r ? s.Regions.at(r).DomainId : 0; }
Id DomainOf(const Snapshot& s, Id id) {
    // Offices and holdings establish allegiance; travel alone cannot change a lord's liege.
    for(const auto& d:s.Domains) if(d.second.HeadCharacterId==id) return d.first;
    for(const auto& r:s.Regions) if(s.Entities.at(r.second.OwnerEntityId).HeadCharacterId==id) return r.second.DomainId;
    return DomainAt(s,s.Characters.at(id).AreaId);
}
bool Active(const Snapshot& s, Id d) { return s.Domains.count(d) && !s.Politics.RetiredDomains.count(d); }
bool Ruler(const Snapshot& s, Id d, Id c) { return Active(s,d) && Adult(s,c) && s.Domains.at(d).HeadCharacterId==c; }
double Population(const Snapshot& s, Id region) { double n=0; for(const auto& a:s.Areas) if(a.second.RegionId==region) for(const auto& c:a.second.Cohorts) n+=static_cast<double>(c.Population); return n; }
std::vector<Id> Regions(const Snapshot& s, Id domain) { std::vector<Id> ids; for(const auto& r:s.Regions) if(r.second.DomainId==domain) ids.push_back(r.first); return ids; }
bool Fighting(const Snapshot& s, Id d) { for(const auto& w:s.Politics.Wars) if(w.second.Active && (w.second.Attacker==d || w.second.Defender==d)) return true; return false; }
bool Peace(const Snapshot& s, Id d) { const auto p=s.Politics.PeaceUntil.find(d); return p==s.Politics.PeaceUntil.end() || p->second<=s.Day; }
Id Owner(const Snapshot& s, Id region) { return s.Entities.at(s.Regions.at(region).OwnerEntityId).HeadCharacterId; }
std::string Op(const Snapshot& s, const std::string& kind, Id id) { return "politics/"+std::to_string(s.Day)+"/"+kind+"/"+std::to_string(id); }
void EventFor(Snapshot& s,const std::string& op,const std::string& kind,Id actor,Id other,const std::string& detail) { s.Events.push_back({op,kind,s.Day,actor,other,detail}); }
void SyncFamilies(Snapshot& s)
{
    for(const auto& row:s.Characters) if(!s.Politics.CharacterFamilies.count(row.first)) {
        Id root=row.first;
        while(!s.Characters.at(root).Parents.empty()) root=s.Characters.at(root).Parents.front();
        s.Politics.CharacterFamilies[row.first]=root;
        if(!s.Politics.Families.count(root)) { FamilyEstate f; f.Founder=root; s.Politics.Families.emplace(root,f); }
    }
}
void SyncLords(Snapshot& s)
{
    for(auto i=s.Politics.Lords.begin();i!=s.Politics.Lords.end();) {
        if(!Adult(s,i->first) || Protected(s,i->first) || !DomainOf(s,i->first) || Ruler(s,DomainOf(s,i->first),i->first)) i=s.Politics.Lords.erase(i); else ++i;
    }
    for(const auto& c:s.Characters) if(Adult(s,c.first) && !Protected(s,c.first)) {
        const Id d=DomainOf(s,c.first); if(!d || Ruler(s,d,c.first)) continue;
        const Id liege=s.Domains.at(d).HeadCharacterId;
        auto& l=s.Politics.Lords[c.first];
        if(l.Domain!=d || l.Liege!=liege) { const auto old=l; l=LordRelationship{}; l.Domain=d; l.Liege=liege;
            if(s.Market.Policy.Enabled && old.Domain==d) { l.Loyalty=old.Loyalty*0.75+0.25*0.5; l.Friendship=old.Friendship*0.5; l.Grievance=old.Grievance/2; }
        }
    }
}
bool Reachable(const Snapshot& s,Id attacker,Id region)
{
    // A land frontier can be reached through unclaimed cells, never water or a third realm.
    std::queue<std::pair<Id,unsigned>> pending; std::set<Id> seen;
    for(const auto& a:s.Areas) if(!a.second.IsWater && DomainAt(s,a.first)==attacker) { pending.push({a.first,0}); seen.insert(a.first); }
    while(!pending.empty()) { const auto item=pending.front(); pending.pop(); if(item.second>=128) continue;
        for(Id id:s.Areas.at(item.first).Neighbors) {
            const auto& a=s.Areas.at(id); if(a.IsWater || !seen.insert(id).second) continue;
            if(a.RegionId==region) return true;
            const Id d=DomainAt(s,id); if(d==0 || d==attacker) pending.push({id,item.second+1});
        }
    }
    return false;
}
bool Attention(const Snapshot& s,Id ruler) { auto p=s.Politics.LastAttention.find(ruler); return p==s.Politics.LastAttention.end() || s.Day-p->second>=s.Politics.Policy.TickDays; }
double HouseholdCost(const Snapshot& s,Id family) { double cost=1; for(const auto& c:s.Politics.CharacterFamilies) if(c.second==family && s.Characters.at(c.first).Alive) cost+=1+DesiredHoldings(s,c.first); return cost; }
void TransferRegion(Snapshot& s,Id region,Id recipient,bool preserveLord=false)
{
    const Id previous=s.Regions.at(region).DomainId; s.Regions.at(region).DomainId=recipient;
    const Id previousHead=s.Domains.at(previous).HeadCharacterId;
    // Claimants describe current disputed ownership, not historical title holders.
    // Repeated conquest/reconquest must leave one recognized owner, never two claims.
    for(auto& a:s.Areas) if(a.second.RegionId==region) a.second.Claimants={recipient};
    auto& old=s.Domains.at(previous);
    if(old.CapitalAreaId && s.Regions.at(s.Areas.at(old.CapitalAreaId).RegionId).DomainId!=previous) old.CapitalAreaId=0;
    const auto remaining=Regions(s,previous);
    if(remaining.empty()) {
        old.HeadCharacterId=0; old.CapitalAreaId=0; old.InfluenceBudget=0; s.Politics.RetiredDomains.insert(previous);
        auto& from=s.Evolution.Domains.at(previous); auto& to=s.Evolution.Domains.at(recipient);
        to.Treasury+=from.Treasury; to.Revenue+=from.Treasury; from.Spent+=from.Treasury; from.Treasury=0;
    } else if(!old.CapitalAreaId) {
        for(const auto& a:s.Areas) if(a.second.RegionId==remaining.front()) { old.CapitalAreaId=a.first; break; }
    }
    if(old.HeadCharacterId && DomainAt(s,s.Characters.at(old.HeadCharacterId).AreaId)!=previous) s.Characters.at(old.HeadCharacterId).AreaId=old.CapitalAreaId;
    const Id lord=Owner(s,region);
    if(!preserveLord || (lord && lord==previousHead)) {
        const Id previousEntity=s.Regions.at(region).OwnerEntityId;
        const Id entity=s.Entities.rbegin()->first+1;
        PoliticalEntity owner; owner.Key=entity; owner.Name="Conquered holding "+std::to_string(entity); owner.HeadCharacterId=s.Domains.at(recipient).HeadCharacterId;
        s.Entities.emplace(entity,owner); s.Regions.at(region).OwnerEntityId=entity;
        bool used=false; for(const auto& r:s.Regions) if(r.second.OwnerEntityId==previousEntity) used=true;
        if(!used) s.Entities.erase(previousEntity);
    } else if(lord) for(const auto& a:s.Areas) if(a.second.RegionId==region) { s.Characters.at(lord).AreaId=a.first; break; }
}
std::uint64_t Strength(const Snapshot& s,Id war,Id domain) { std::uint64_t n=0; for(const auto& a:s.Politics.Armies) if(a.second.War==war && a.second.Domain==domain) n+=a.second.Soldiers; return n; }
double PotentialArmy(const Snapshot& s,Id domain) {
    std::map<Id,double> levies; double privateFunds=0; std::set<Id> families;
    for(const auto& r:s.Regions) if(r.second.DomainId==domain) {
        const Id lord=Owner(s,r.first); if(!Adult(s,lord)) continue;
        const auto l=s.Politics.Lords.find(lord); if(!Ruler(s,domain,lord) && (l==s.Politics.Lords.end() || l->second.Loyalty<0.4)) continue;
        levies[lord]+=Population(s,r.first)*s.Politics.Policy.LevyFraction;
        auto f=s.Politics.CharacterFamilies.find(lord); if(f!=s.Politics.CharacterFamilies.end() && families.insert(f->second).second) privateFunds+=s.Politics.Families.at(f->second).Wealth;
    }
    double n=0; for(const auto& l:levies) n+=std::min(l.second,static_cast<double>(TroopCapacity(s,l.first))*20);
    return std::min(n,(privateFunds+s.Evolution.Domains.at(domain).Treasury)/s.Politics.Policy.MusterCost);
}
// Allocate an aggregate result proportionally, rounding only cumulative totals.
// Splitting a force into more area/culture records cannot multiply total casualties.
void RemoveSoldiers(Snapshot& s,Id war,Id domain,std::uint64_t loss,bool deaths) {
    ApplyForceLosses(s,war,domain,loss,deaths);
}

void ReturnArmy(Snapshot& s,Id war)
{
    for(auto a=s.Politics.Armies.begin();a!=s.Politics.Armies.end();) {
        if(a->second.War!=war) { ++a; continue; }
        auto& cohorts=s.Areas.at(a->second.HomeArea).Cohorts;
        auto found=std::find_if(cohorts.begin(),cohorts.end(),[&](const Cohort& c){return c.CultureId==a->second.Culture;});
        if(found==cohorts.end()) { Cohort c; c.CultureId=a->second.Culture; c.Population=a->second.Soldiers; cohorts.push_back(c); }
        else found->Population+=a->second.Soldiers;
        s.Politics.Returned+=a->second.Soldiers; a=s.Politics.Armies.erase(a);
    }
}
}

double DesiredHoldings(const Snapshot& s,Id id)
{
    double power=0; for(const auto& a:s.Characters.at(id).Affinities) power+=Power(a,s.Policy);
    return std::clamp(1+power/s.Politics.Policy.PowerPerHolding,1.0,20.0);
}
std::size_t HeldRegions(const Snapshot& s,Id character) { std::size_t n=0; for(const auto& r:s.Regions) if(Owner(s,r.first)==character) ++n; return n; }

Result ValidatePolitics(const Snapshot& s)
{
    const auto& p=s.Politics; const auto& r=p.Policy;
    if(r.Version!=2 || !r.TickDays || r.TickDays>365 || !Bounded(r.PowerPerHolding,1,10000) || !Bounded(r.FriendshipDecay,0,1) ||
        !Bounded(r.LoyaltyResponse,0.001,1) || !Bounded(r.FactionThreshold,0,1) || !Bounded(r.ReconcileThreshold,r.FactionThreshold+0.01,1) ||
        !r.GrievanceTicks || r.GrievanceTicks>120 || r.RebellionTicks<r.GrievanceTicks || r.RebellionTicks>240 ||
        !Bounded(r.RebellionShare,0.01,0.9) || !Bounded(r.FamilyIncomeShare,0,0.8) || !Bounded(r.LevyFraction,0.0001,0.1) ||
        !Bounded(r.MusterCost,0.001,100) || !Bounded(r.SoldierUpkeep,0.001,100) || !Bounded(r.CasualtyFraction,0.01,0.5) ||
        !r.WarRounds || r.WarRounds>36 || r.PeaceDays<r.TickDays || r.PeaceDays>36500 || !Bounded(r.AnnualWarChance,0,1)) return No("Invalid politics policy bounds/version.");
    if(r.Enabled && !s.Evolution.Policy.Enabled) return No("Politics requires the economic evolution policy.");
    if(!r.Enabled && (!p.Families.empty() || !p.CharacterFamilies.empty() || !p.Lords.empty() || !p.Factions.empty() || !p.Armies.empty() || !p.Wars.empty() || !p.RetiredDomains.empty())) return No("Disabled politics contains active state.");
    if(p.Families.size()>1000000 || p.Factions.size()>100000 || p.Armies.size()>1000000 || p.Wars.size()>100000 || !p.NextArmy || !p.NextFaction || !p.NextWar) return No("Politics record/sequence limit.");
    for(const auto& f:p.Families) if(f.first!=f.second.Founder || !s.Characters.count(f.first) || !Bounded(f.second.Wealth,0,1e14) || !Bounded(f.second.Received,0,1e22) || !Bounded(f.second.Spent,0,1e22) || !Bounded(f.second.StandardOfLiving,0,1) || f.second.LastPurchaseDay>s.Day || std::abs(f.second.Received-f.second.Spent-f.second.Wealth)>1e-8*std::max(1.0,f.second.Received)) return No("Family ledger invalid: "+std::to_string(f.first));
    for(const auto& c:p.CharacterFamilies) if(!s.Characters.count(c.first) || !p.Families.count(c.second)) return No("Invalid family membership.");
    for(const auto& l:p.Lords) if(!s.Characters.count(l.first) || !s.Domains.count(l.second.Domain) || (l.second.Liege && !s.Characters.count(l.second.Liege)) || !Bounded(l.second.Loyalty,0,1) || !Bounded(l.second.Friendship,0,1) || l.second.Grievance>100000000) return No("Invalid lord relationship.");
    for(const auto& f:p.Factions) {
        const auto& v=f.second; std::set<Id> members;
        if(v.Key!=f.first || !v.Key || v.Key>=p.NextFaction || !s.Domains.count(v.Domain) || !s.Characters.count(v.Leader) || v.CreatedDay<0 || v.CreatedDay>s.Day || static_cast<unsigned>(v.Status)>2) return No("Invalid faction.");
        for(Id id:v.Members) if(!s.Characters.count(id) || !members.insert(id).second) return No("Invalid faction membership.");
    }
    std::set<Id> engaged;
    for(const auto& w:p.Wars) {
        const auto& v=w.second;
        if(v.Key!=w.first || !v.Key || v.Key>=p.NextWar || v.Attacker==v.Defender || !s.Domains.count(v.Attacker) || !s.Domains.count(v.Defender) || !s.Regions.count(v.TargetRegion) || v.StartedDay<0 || v.StartedDay>s.Day || v.LastBattleDay<v.StartedDay || v.LastBattleDay>s.Day || v.Rounds>r.WarRounds) return No("Invalid war record.");
        if(v.Active && (!Active(s,v.Attacker) || !Active(s,v.Defender) || !engaged.insert(v.Attacker).second || !engaged.insert(v.Defender).second || s.Regions.at(v.TargetRegion).DomainId!=v.Defender)) return No("Invalid concurrent war/target.");
    }
    std::uint64_t soldiers=0;
    for(const auto& a:p.Armies) {
        const auto& v=a.second;
        if(v.Key!=a.first || !v.Key || v.Key>=p.NextArmy || ((v.War==0)==(v.Expedition==0)) || (v.War && (!p.Wars.count(v.War) || !p.Wars.at(v.War).Active || (v.Domain!=p.Wars.at(v.War).Attacker && v.Domain!=p.Wars.at(v.War).Defender))) || (v.Expedition && (!s.Bandits.Expeditions.count(v.Expedition) || s.Bandits.Expeditions.at(v.Expedition).Phase==ExpeditionPhase::Finished || s.Bandits.Expeditions.at(v.Expedition).Domain!=v.Domain)) || !s.Characters.count(v.Lord) || !s.Areas.count(v.HomeArea) || s.Areas.at(v.HomeArea).IsWater || !s.Cultures.count(v.Culture) || !v.Soldiers || v.Soldiers>1000000) return No("Invalid levy contingent.");
        soldiers+=v.Soldiers;
    }
    if(p.Casualties>p.Levied || p.Returned>p.Levied-p.Casualties || p.Levied-p.Casualties-p.Returned!=soldiers) return No("Levy population ledger does not balance.");
    for(Id id:p.RetiredDomains) if(!s.Domains.count(id) || s.Domains.at(id).HeadCharacterId || s.Domains.at(id).CapitalAreaId || s.Domains.at(id).InfluenceBudget || !Regions(s,id).empty()) return No("Retired domain owns authority/territory.");
    for(const auto& a:p.LastAttention) if(!s.Characters.count(a.first) || a.second<0 || a.second>s.Day) return No("Invalid attention clock.");
    for(const auto& a:p.PeaceUntil) if(!s.Domains.count(a.first) || a.second<0 || a.second>s.Day+36500) return No("Invalid peace clock.");
    return Ok();
}

Result World::ConfigurePolitics(const std::string& operation,const PoliticsRules& policy)
{
    auto result=CheckOperation(operation); if(!result) return result;
    if(!policy.Enabled || StateData.Politics.Policy.Enabled || !StateData.Evolution.Policy.Enabled) return No("Enable politics once on an evolved world.");
    auto next=BeginMutation(); next.Politics.Policy=policy; SyncFamilies(next); SyncLords(next);
    EventFor(next,operation,"politics_enabled",0,0,"Version=1; families, obligations, factions and strategic wars enabled");
    return Commit(std::move(next));
}
Result World::BefriendLord(const std::string& operation,Id liege,Id lord)
{
    auto result=CheckOperation(operation); if(!result) return result;
    const auto& s=StateData; auto l=s.Politics.Lords.find(lord);
    if(!s.Politics.Policy.Enabled || l==s.Politics.Lords.end() || !Adult(s,lord) || l->second.Liege!=liege || DomainOf(s,lord)!=l->second.Domain || !Ruler(s,l->second.Domain,liege) || !Attention(s,liege)) return No("Friendship requires a current liege, living vassal and available ruler attention.");
    const double cost=10*DesiredHoldings(s,lord);
    if(s.Evolution.Domains.at(l->second.Domain).Treasury<cost) return No("Friendship visit lacks treasury funds.");
    const Id domain=l->second.Domain; auto next=BeginMutation(); auto& money=next.Evolution.Domains.at(domain); money.Treasury-=cost; money.Spent+=cost;
    CreditMarketServices(next,domain,cost);
    auto& relation=next.Politics.Lords.at(lord); relation.Friendship=std::min(1.0,relation.Friendship+0.2); relation.Loyalty=std::min(1.0,relation.Loyalty+0.08);
    next.Politics.LastAttention[liege]=next.Day;
    EventFor(next,operation,"friendship",liege,lord,"cost="+std::to_string(cost)); return Commit(std::move(next));
}
Result World::GrantRegion(const std::string& operation,Id liege,Id lord,Id region)
{
    auto result=CheckOperation(operation); if(!result) return result; const auto& s=StateData;
    if(!s.Politics.Policy.Enabled || !s.Regions.count(region) || !s.Politics.Lords.count(lord) || !Adult(s,lord) || !Attention(s,liege)) return No("Grant requires a living lord, region and available ruler attention.");
    const auto& r=s.Regions.at(region);
    if(!Ruler(s,r.DomainId,liege) || Owner(s,region)!=liege || DomainOf(s,lord)!=r.DomainId || s.Politics.Lords.at(lord).Domain!=r.DomainId || s.Politics.Lords.at(lord).Liege!=liege) return No("A liege may grant only personal demesne to a lord of the same realm.");
    auto next=BeginMutation(); const Id previous=next.Regions.at(region).OwnerEntityId; const Id entity=next.Entities.rbegin()->first+1;
    if(!entity) return No("Entity ID exhausted.");
    PoliticalEntity e; e.Key=entity; e.Name="Holding "+std::to_string(entity); e.HeadCharacterId=lord; next.Entities.emplace(entity,e); next.Regions.at(region).OwnerEntityId=entity;
    bool used=false; for(const auto& row:next.Regions) if(row.second.OwnerEntityId==previous) used=true; if(!used) next.Entities.erase(previous);
    auto& l=next.Politics.Lords.at(lord); l.Loyalty=std::min(1.0,l.Loyalty+0.2); l.Friendship=std::min(1.0,l.Friendship+0.15); l.Grievance=0; next.Politics.LastAttention[liege]=next.Day;
    EventFor(next,operation,"territory_grant",liege,lord,"region="+std::to_string(region)); return Commit(std::move(next));
}
Result World::SpendFamilyWealth(const std::string& operation,Id character)
{
    auto result=CheckOperation(operation); if(!result) return result; const auto& s=StateData;
    if(!s.Politics.Policy.Enabled || !Adult(s,character) || !s.Politics.CharacterFamilies.count(character)) return No("Family spending requires a living adult member.");
    const Id family=s.Politics.CharacterFamilies.at(character); const auto& f=s.Politics.Families.at(family); const double cost=HouseholdCost(s,family);
    if(s.Day-f.LastPurchaseDay<s.Politics.Policy.TickDays || f.Wealth<cost || !CanBuyComforts(s,character,cost)) return No("Household purchase lacks wealth/goods or is on cooldown.");
    auto next=BeginMutation(); auto& estate=next.Politics.Families.at(family); estate.Wealth-=cost; estate.Spent+=cost; estate.StandardOfLiving=std::min(1.0,estate.StandardOfLiving+0.15); estate.LastPurchaseDay=next.Day;
    BuyComforts(next,character,cost);
    EventFor(next,operation,"household_spending",character,0,"family="+std::to_string(family)+"; cost="+std::to_string(cost)); return Commit(std::move(next));
}
Result World::JoinFaction(const std::string& operation,Id character)
{
    auto result=CheckOperation(operation); if(!result) return result; const auto& s=StateData; const auto l=s.Politics.Lords.find(character);
    if(!s.Politics.Policy.Enabled || !Adult(s,character) || l==s.Politics.Lords.end() || l->second.Loyalty>s.Politics.Policy.FactionThreshold || l->second.Grievance<s.Politics.Policy.GrievanceTicks || !Active(s,l->second.Domain) || DomainOf(s,character)!=l->second.Domain || s.Domains.at(l->second.Domain).HeadCharacterId!=l->second.Liege) return No("Faction requires sustained disloyalty to an active liege.");
    Id faction=0; for(const auto& f:s.Politics.Factions) if(f.second.Domain==l->second.Domain && f.second.Status==FactionStatus::Organizing) { faction=f.first; if(std::find(f.second.Members.begin(),f.second.Members.end(),character)!=f.second.Members.end()) return No("Already a faction member."); }
    const Id domain=l->second.Domain; auto next=BeginMutation(); if(!faction) { faction=next.Politics.NextFaction++; PoliticalFaction f; f.Key=faction; f.Domain=domain; f.Leader=character; f.CreatedDay=next.Day; next.Politics.Factions.emplace(faction,f); }
    next.Politics.Factions.at(faction).Members.push_back(character);
    EventFor(next,operation,"faction_joined",character,0,"faction="+std::to_string(faction)+"; domain="+std::to_string(domain)); return Commit(std::move(next));
}
Result World::DeclareWar(const std::string& operation,Id ruler,Id targetRegion)
{
    auto result=CheckOperation(operation); if(!result) return result; const auto& s=StateData;
    if(!s.Politics.Policy.Enabled || !Adult(s,ruler) || !s.Regions.count(targetRegion)) return No("War requires a living ruler and existing target region.");
    const Id attacker=DomainOf(s,ruler), defender=s.Regions.at(targetRegion).DomainId;
    if(!Ruler(s,attacker,ruler) || attacker==defender || !Active(s,defender) || Fighting(s,attacker) || Fighting(s,defender) || !Peace(s,attacker) || !Peace(s,defender) || !Reachable(s,attacker,targetRegion)) return No("War requires peace cooldown, a reachable land frontier and unengaged realms.");
    if(s.Evolution.Domains.at(attacker).Treasury<100) return No("War declaration requires 100 treasury units.");
    if(s.Market.Policy.Enabled && s.Evolution.Domains.at(attacker).Treasury<100+PotentialArmy(s,attacker)*s.Politics.Policy.SoldierUpkeep*2) return No("War declaration lacks two rounds of campaign reserves.");
    auto next=BeginMutation(); auto& treasury=next.Evolution.Domains.at(attacker); treasury.Treasury-=100; treasury.Spent+=100;
    CreditMarketServices(next,attacker,100);
    CampaignWar w; w.Key=next.Politics.NextWar++; w.Attacker=attacker; w.Defender=defender; w.TargetRegion=targetRegion; w.StartedDay=w.LastBattleDay=next.Day; next.Politics.Wars.emplace(w.Key,w);
    EventFor(next,operation,"war_declared",ruler,0,"war="+std::to_string(w.Key)+"; defender="+std::to_string(defender)+"; target_region="+std::to_string(targetRegion)); return Commit(std::move(next));
}
Result World::Rebel(const std::string& operation,Id faction)
{
    auto result=CheckOperation(operation); if(!result) return result; const auto& s=StateData; const auto found=s.Politics.Factions.find(faction);
    if(!s.Politics.Policy.Enabled || found==s.Politics.Factions.end() || found->second.Status!=FactionStatus::Organizing) return No("Rebellion requires an organizing faction.");
    const auto f=found->second; const Id domain=f.Domain;
    const auto leader=s.Politics.Lords.find(f.Leader);
    if(leader==s.Politics.Lords.end() || DomainOf(s,f.Leader)!=domain || leader->second.Loyalty>=s.Politics.Policy.ReconcileThreshold || std::find(f.Members.begin(),f.Members.end(),f.Leader)==f.Members.end()) return No("Faction leader no longer supports rebellion against this realm.");
    if(!Active(s,domain) || Fighting(s,domain) || !Peace(s,domain) || !Adult(s,f.Leader) || s.Day-f.CreatedDay<static_cast<std::int64_t>(s.Politics.Policy.RebellionTicks)*s.Politics.Policy.TickDays) return No("Rebellion is not ready or realm is already at war.");
    const auto all=Regions(s,domain); std::vector<Id> claims; double total=0, supporters=0;
    for(Id r:all) { const double n=Population(s,r); total+=n; const Id owner=Owner(s,r); auto l=s.Politics.Lords.find(owner);
        const bool capital=s.Domains.at(domain).CapitalAreaId && s.Areas.at(s.Domains.at(domain).CapitalAreaId).RegionId==r;
        if(!capital && l!=s.Politics.Lords.end() && (s.Politics.Policy.RequireCurrentGrievance ? (l->second.Loyalty<s.Politics.Policy.FactionThreshold && l->second.Grievance>=s.Politics.Policy.GrievanceTicks) : l->second.Loyalty<s.Politics.Policy.ReconcileThreshold) && std::find(f.Members.begin(),f.Members.end(),owner)!=f.Members.end()) { claims.push_back(r); supporters+=n; }
    }
    if(claims.empty() || claims.size()==all.size() || supporters<std::max(1.0,total)*s.Politics.Policy.RebellionShare) return No("Faction lacks sufficient non-capital territory and support.");
    const Id rebel=s.Domains.rbegin()->first+1; if(!rebel) return No("Domain ID exhausted.");
    auto next=BeginMutation();
    auto d=next.Domains.at(domain); d.Key=rebel; d.Name="Rebel realm "+std::to_string(rebel); d.HeadCharacterId=f.Leader;
    for(const auto& a:next.Areas) if(a.second.RegionId==claims.front()) { d.CapitalAreaId=a.first; break; }
    next.Domains.emplace(rebel,d); next.Evolution.Domains.emplace(rebel,DomainEconomy{});
    for(Id r:claims) TransferRegion(next,r,rebel,true);
    for(Id r:Regions(next,domain)) if(std::find(f.Members.begin(),f.Members.end(),Owner(next,r))!=f.Members.end()) next.Entities.at(next.Regions.at(r).OwnerEntityId).HeadCharacterId=next.Domains.at(domain).HeadCharacterId;
    next.Characters.at(f.Leader).AreaId=d.CapitalAreaId;
    next.Politics.Factions.at(faction).Status=FactionStatus::Rebelled;
    CampaignWar war; war.Key=next.Politics.NextWar++; war.Attacker=domain; war.Defender=rebel; war.TargetRegion=claims.front(); war.StartedDay=war.LastBattleDay=next.Day; war.Rebellion=true; next.Politics.Wars.emplace(war.Key,war);
    SyncLords(next); EventFor(next,operation,"rebellion",f.Leader,0,"faction="+std::to_string(faction)+"; domain="+std::to_string(domain)+"; new_domain="+std::to_string(rebel)+"; war="+std::to_string(war.Key)+"; support_share="+std::to_string(supporters/std::max(1.0,total))+"; faction_age_days="+std::to_string(next.Day-f.CreatedDay)+"; current_grievance="+std::to_string(s.Politics.Policy.RequireCurrentGrievance));
    result=UpdateInfluenceTargets(next); if(!result) return {false,"politics_transition_invalid",result.Code+": "+result.Detail}; return Commit(std::move(next));
}
Result World::RaiseArmy(const std::string& operation,Id ruler,Id war)
{
    auto result=CheckOperation(operation); if(!result) return result; const auto& s=StateData; const auto found=s.Politics.Wars.find(war);
    if(!s.Politics.Policy.Enabled || !Adult(s,ruler) || found==s.Politics.Wars.end() || !found->second.Active) return No("Muster requires an active war and living ruler.");
    const Id domain=DomainOf(s,ruler); if(!Ruler(s,domain,ruler) || (found->second.Attacker!=domain && found->second.Defender!=domain)) return No("Ruler does not command a war participant.");
    if(Strength(s,war,domain)) return No("Realm already has levies in this war.");
    auto next=BeginMutation(); SyncFamilies(next); std::uint64_t raised=0; std::map<Id,std::uint32_t> lordTotals;
    for(const auto& a:next.Politics.Armies) lordTotals[a.second.Lord]+=a.second.Soldiers;
    for(auto& a:next.Areas) if(a.second.RegionId && DomainAt(next,a.first)==domain) {
        const Id lord=Owner(next,a.second.RegionId); if(!Adult(next,lord) || Protected(next,lord)) continue;
        auto loyalty=next.Politics.Lords.find(lord); if(lord!=ruler && (loyalty==next.Politics.Lords.end() || loyalty->second.Loyalty<0.4)) continue;
        const auto limit=std::min(100000u,TroopCapacity(next,lord)*20u); if(lordTotals[lord]>=limit) continue;
        for(auto& c:a.second.Cohorts) {
            auto n=static_cast<std::uint32_t>(std::min<double>(limit-lordTotals[lord],std::floor(static_cast<double>(c.Population)*next.Politics.Policy.LevyFraction)));
            auto& treasury=next.Evolution.Domains.at(domain); auto& family=next.Politics.Families.at(next.Politics.CharacterFamilies.at(lord));
            n=std::min(n,static_cast<std::uint32_t>(std::min(100000.0,std::floor((treasury.Treasury+family.Wealth)/next.Politics.Policy.MusterCost)))); if(!n) continue;
            const double cost=n*next.Politics.Policy.MusterCost, privateCost=std::min(cost,family.Wealth);
            const double publicCost=std::min(treasury.Treasury,cost-privateCost);
            family.Wealth-=privateCost; family.Spent+=privateCost; treasury.Treasury-=publicCost; treasury.Spent+=publicCost;
            CreditMarketServices(next,domain,cost);
            raised+=n; lordTotals[lord]+=n;
            EnlistContingent(next,war,0,domain,lord,a.first,c.CultureId,n);
        }
    }
    if(!raised) { if(Batched) StateData=std::move(next); return No("No willing, funded lords have available population to levy."); }
    EventFor(next,operation,"army_raised",ruler,0,"war="+std::to_string(war)+"; soldiers="+std::to_string(raised)+"; lords="+std::to_string(lordTotals.size())); return Commit(std::move(next));
}
Result World::ResolveWarRound(const std::string& operation,Id war)
{
    auto result=CheckOperation(operation); if(!result) return result; const auto& s=StateData; const auto found=s.Politics.Wars.find(war);
    if(!s.Politics.Policy.Enabled || found==s.Politics.Wars.end() || !found->second.Active || s.Day-found->second.LastBattleDay<s.Politics.Policy.TickDays) return No("War round is not due.");
    auto next=BeginMutation(); auto& w=next.Politics.Wars.at(war); const auto& p=next.Politics.Policy;
    for(Id domain:{w.Attacker,w.Defender}) {
        const auto n=Strength(next,war,domain); auto& money=next.Evolution.Domains.at(domain); const double due=static_cast<double>(n)*p.SoldierUpkeep;
        const double paid=std::min(money.Treasury,due); money.Treasury-=paid; money.Spent+=paid;
        CreditMarketServices(next,domain,paid);
        if(next.Market.Policy.Enabled) {
            double supply=0; for(const auto& a:next.Politics.Armies) if(a.second.War==war && a.second.Domain==domain) { const Id region=next.Areas.at(a.second.HomeArea).RegionId; supply+=a.second.Soldiers*(region?next.Market.Regions.at(region).ArmySupply:0); }
            auto inserted=next.Market.Readiness.emplace(std::make_pair(war,domain),1.0); auto& readiness=inserted.first->second;
            const double funded=due>0?paid/due:1; const double fed=n>0?supply/static_cast<double>(n):1;
            readiness=std::clamp(readiness*0.5+0.5*funded*fed,0.1,1.0);
            RemoveSoldiers(next,war,domain,static_cast<std::uint64_t>(std::floor(n*0.25*(1-funded*fed))),false);
        }
        else if(paid<due) for(auto a=next.Politics.Armies.begin();a!=next.Politics.Armies.end();) {
            if(a->second.War!=war || a->second.Domain!=domain) { ++a; continue; }
            auto& cohorts=next.Areas.at(a->second.HomeArea).Cohorts; auto c=std::find_if(cohorts.begin(),cohorts.end(),[&](const Cohort& v){return v.CultureId==a->second.Culture;});
            if(c==cohorts.end()) { Cohort v; v.CultureId=a->second.Culture; v.Population=a->second.Soldiers; cohorts.push_back(v); } else c->Population+=a->second.Soldiers;
            next.Politics.Returned+=a->second.Soldiers; a=next.Politics.Armies.erase(a);
        }
    }
    const auto left=Strength(next,war,w.Attacker), right=Strength(next,war,w.Defender);
    const auto ready=[&](Id d) { const auto i=next.Market.Readiness.find({war,d}); return i==next.Market.Readiness.end()?1:i->second; };
    const auto losses=[&](std::uint64_t own,std::uint64_t enemy,Id self,Id other) { return StrategicLosses(own,static_cast<double>(enemy),ready(self),ready(other),p.CasualtyFraction); };
    const auto lostLeft=losses(left,right,w.Attacker,w.Defender), lostRight=losses(right,left,w.Defender,w.Attacker);
    RemoveSoldiers(next,war,w.Attacker,lostLeft,true); RemoveSoldiers(next,war,w.Defender,lostRight,true);
    ++w.Rounds; w.LastBattleDay=next.Day;
    const bool finish=!left || !right || w.Rounds>=p.WarRounds || !Strength(next,war,w.Attacker) || !Strength(next,war,w.Defender);
    const Id actor=next.Domains.at(w.Attacker).HeadCharacterId ? next.Domains.at(w.Attacker).HeadCharacterId : next.Characters.begin()->first;
    EventFor(next,operation,finish ? "war_ended" : "war_battle",actor,0,"war="+std::to_string(war)+"; attacker_strength="+std::to_string(Strength(next,war,w.Attacker))+"; defender_strength="+std::to_string(Strength(next,war,w.Defender)));
    if(finish) {
        const bool conquered=Strength(next,war,w.Attacker)>Strength(next,war,w.Defender); ReturnArmy(next,war); w.Active=false;
        if(conquered) {
            const auto targets=w.Rebellion ? Regions(next,w.Defender) : std::vector<Id>{w.TargetRegion};
            for(Id r:targets) TransferRegion(next,r,w.Attacker);
            next.Events.back().Detail+="; territory_to="+std::to_string(w.Attacker);
            if(next.Politics.RetiredDomains.count(w.Defender)) next.Events.back().Detail+="; retired_domain="+std::to_string(w.Defender);
        }
        next.Politics.PeaceUntil[w.Attacker]=next.Day+p.PeaceDays; next.Politics.PeaceUntil[w.Defender]=next.Day+p.PeaceDays;
        SyncLords(next); result=UpdateInfluenceTargets(next); if(!result) return result;
    }
    return Commit(std::move(next));
}

Result AdvancePolitics(World& world)
{
    auto& s=world.StateData; if(!s.Politics.Policy.Enabled) return Ok(); SyncFamilies(s);
    const auto policy=s.Politics.Policy; if(s.Day%policy.TickDays) return Ok(); SyncLords(s);
    // Treasury payments are transfers: family receipts exactly match public spending.
    for(auto& estate:s.Politics.Families) estate.second.StandardOfLiving*=0.9;
    for(const auto& d:s.Domains) if(!s.Market.Policy.Enabled && Active(s,d.first)) {
        std::map<Id,double> weights; double total=0,pop=0;
        for(Id r:Regions(s,d.first)) pop+=Population(s,r);
        for(const auto& c:s.Characters) if(Adult(s,c.first) && DomainOf(s,c.first)==d.first && !Protected(s,c.first)) { const double weight=0.25+static_cast<double>(HeldRegions(s,c.first)); weights[s.Politics.CharacterFamilies.at(c.first)]+=weight; total+=weight; }
        auto& money=s.Evolution.Domains.at(d.first); const double budget=std::min(money.Treasury,pop*s.Evolution.Policy.TaxPerPerson*policy.TickDays*policy.FamilyIncomeShare);
        double paid=0; if(total>0) for(const auto& f:weights) { const double amount=std::min(budget-paid,budget*f.second/total); auto& estate=s.Politics.Families.at(f.first); estate.Wealth+=amount; estate.Received+=amount; paid+=amount; }
        money.Treasury-=paid; money.Spent+=paid;
    }
    std::vector<Id> adults; for(const auto& c:s.Characters) if(Adult(s,c.first) && !Protected(s,c.first)) adults.push_back(c.first);
    std::set<Id> bought;
    for(Id id:adults) { const Id family=s.Politics.CharacterFamilies.at(id); if(!bought.insert(family).second) continue; const auto& f=s.Politics.Families.at(family);
        if(f.Wealth>=HouseholdCost(s,family) && s.Day-f.LastPurchaseDay>=policy.TickDays && CanBuyComforts(s,id,HouseholdCost(s,family))) { auto r=world.SpendFamilyWealth(Op(s,"household",family),id); if(!r) return r; }
    }
    for(auto& row:s.Politics.Lords) {
        auto& l=row.second; l.Friendship=std::max(0.0,l.Friendship-policy.FriendshipDecay);
        double holdingValue=static_cast<double>(HeldRegions(s,row.first));
        if(s.Market.Policy.Enabled) { double total=0; for(const auto& r:s.Regions) total+=Population(s,r.first); const double average=total/std::max<std::size_t>(1,s.Regions.size()); holdingValue=0;
            for(const auto& r:s.Regions) if(Owner(s,r.first)==row.first) holdingValue+=std::clamp(Population(s,r.first)/std::max(1.0,average),0.25,3.0)*(0.5+0.5*s.Market.Regions.at(r.first).FoodCoverage);
        }
        const double satisfaction=std::min(1.0,holdingValue/DesiredHoldings(s,row.first));
        const double living=s.Politics.Families.at(s.Politics.CharacterFamilies.at(row.first)).StandardOfLiving;
        const double status=s.Market.Wants.count(row.first)?0.5+s.Market.Wants.at(row.first).Status:1;
        const double target=std::clamp(0.25+0.5*l.Friendship+0.2*s.Evolution.Domains.at(l.Domain).Legitimacy+0.35*status*(2*satisfaction-1)+0.1*living+EconomicSatisfaction(s,row.first),0.0,1.0);
        l.Loyalty+=(target-l.Loyalty)*policy.LoyaltyResponse;
        l.Grievance=l.Loyalty<policy.FactionThreshold ? std::min(100000000u,l.Grievance+1) : 0;
        if(l.Grievance==1 || l.Grievance==policy.GrievanceTicks) EventFor(s,Op(s,"pressure",row.first),"lord_pressure",row.first,l.Liege,"domain="+std::to_string(l.Domain)+"; loyalty="+std::to_string(l.Loyalty)+"; target="+std::to_string(target)+"; friendship="+std::to_string(l.Friendship)+"; holding_value="+std::to_string(holdingValue)+"; desired="+std::to_string(DesiredHoldings(s,row.first))+"; satisfaction="+std::to_string(satisfaction)+"; economic="+std::to_string(EconomicSatisfaction(s,row.first))+"; grievance_ticks="+std::to_string(l.Grievance));
    }
    std::vector<Id> domains; for(const auto& d:s.Domains) if(Active(s,d.first)) domains.push_back(d.first);
    for(Id d:domains) {
        const Id ruler=s.Domains.at(d).HeadCharacterId; if(!Adult(s,ruler) || Protected(s,ruler) || !Attention(s,ruler)) continue;
        Id unhappy=0; double loyalty=0.75; for(const auto& l:s.Politics.Lords) if(l.second.Domain==d && l.second.Loyalty<loyalty) { unhappy=l.first; loyalty=l.second.Loyalty; }
        if(!unhappy) continue;
        bool granted=false; if(DesiredHoldings(s,unhappy)>HeldRegions(s,unhappy)+0.5 && HeldRegions(s,ruler)>1) for(Id r:Regions(s,d)) if(Owner(s,r)==ruler) {
            auto accepted=world.GrantRegion(Op(s,"grant",ruler),ruler,unhappy,r); if(!accepted) return accepted; granted=true; break;
        }
        if(!granted && s.Evolution.Domains.at(d).Treasury>=10*DesiredHoldings(s,unhappy)) { auto accepted=world.BefriendLord(Op(s,"friendship",ruler),ruler,unhappy); if(!accepted) return accepted; }
        else if(!granted) EventFor(s,Op(s,"maintenance",d),"loyalty_maintenance_deferred",ruler,unhappy,"domain="+std::to_string(d)+"; reason=no_spare_holding_and_visit_unfunded; treasury="+std::to_string(s.Evolution.Domains.at(d).Treasury)+"; visit_cost="+std::to_string(10*DesiredHoldings(s,unhappy)));
    }
    for(Id id:adults) {
        const auto l=s.Politics.Lords.find(id); if(l==s.Politics.Lords.end() || l->second.Grievance<policy.GrievanceTicks || l->second.Loyalty>policy.FactionThreshold) continue;
        bool member=false; for(const auto& f:s.Politics.Factions) if(f.second.Status==FactionStatus::Organizing && std::find(f.second.Members.begin(),f.second.Members.end(),id)!=f.second.Members.end()) member=true;
        if(!member) { auto r=world.JoinFaction(Op(s,"faction",id),id); if(!r) return r; }
    }
    std::vector<Id> factions; for(const auto& f:s.Politics.Factions) if(f.second.Status==FactionStatus::Organizing) factions.push_back(f.first);
    for(Id id:factions) {
        auto& f=s.Politics.Factions.at(id);
        f.Members.erase(std::remove_if(f.Members.begin(),f.Members.end(),[&](Id c){auto l=s.Politics.Lords.find(c); return !Adult(s,c) || l==s.Politics.Lords.end() || l->second.Domain!=f.Domain || l->second.Loyalty>=policy.ReconcileThreshold;}),f.Members.end());
        if(f.Members.empty() || !Active(s,f.Domain)) { f.Status=FactionStatus::Resolved; continue; }
        if(std::find(f.Members.begin(),f.Members.end(),f.Leader)==f.Members.end()) f.Leader=f.Members.front();
        // Ineligibility is an ordinary policy outcome; command validation remains authoritative.
        auto result=world.Rebel(Op(s,"rebel",id),id); if(!result && result.Code!="politics_ineligible") return result;
        if(!result && s.Day%365<policy.TickDays) EventFor(s,Op(s,"rebel-deferred",id),"rebellion_deferred",f.Leader,0,"faction="+std::to_string(id)+"; reason="+result.Detail);
    }
    // Record the first binding eligibility gate, not an inferred historical cause.
    for(Id d:domains) if(Active(s,d)) {
        const Id ruler=s.Domains.at(d).HeadCharacterId;
        const auto decision=[&](const std::string& reason){EventFor(s,Op(s,"war-decision",d),"war_decision",ruler,0,"domain="+std::to_string(d)+"; treasury="+std::to_string(s.Evolution.Domains.at(d).Treasury)+"; reason="+reason);};
        if(Fighting(s,d)){decision("active_war");continue;}
        if(!Peace(s,d)){decision("peace_cooldown");continue;}
        if(!Adult(s,ruler) || Protected(s,ruler)){decision("ruler_unavailable");continue;}
        if(s.Evolution.Domains.at(d).Treasury<200){decision("minimum_treasury");continue;}
        if(s.Market.Policy.Enabled && s.Evolution.Domains.at(d).Treasury<100+PotentialArmy(s,d)*policy.SoldierUpkeep*2){decision("campaign_reserves");continue;}
        const std::uint64_t mix=(static_cast<std::uint64_t>(s.Seed)*1664525ULL+d*1013904223ULL+static_cast<std::uint64_t>(s.Day)*2654435761ULL)%100000;
        if(static_cast<double>(mix)/100000>=1-std::pow(1-policy.AnnualWarChance,static_cast<double>(policy.TickDays)/365)){decision("opportunity_draw");continue;}
        double own=0;for(Id r:Regions(s,d))own+=Population(s,r);
        unsigned busy=0,strong=0,unreachable=0;bool declared=false;
        for(const auto& region:s.Regions){const Id other=region.second.DomainId;if(other==d || !Active(s,other))continue;
            if(Fighting(s,other) || !Peace(s,other)){++busy;continue;}
            double enemy=0;for(Id r:Regions(s,other))enemy+=Population(s,r);
            if(s.Market.Policy.Enabled ? PotentialArmy(s,other)>PotentialArmy(s,d)*1.1 : enemy>own*1.1){++strong;continue;}
            if(!Reachable(s,d,region.first)){++unreachable;continue;}
            auto r=world.DeclareWar(Op(s,"war",d),ruler,region.first);if(!r)return r;declared=true;break;
        }
        decision(declared?"declared":"no_target; busy_regions="+std::to_string(busy)+"; stronger_regions="+std::to_string(strong)+"; unreachable_regions="+std::to_string(unreachable));
    }
    std::vector<Id> wars; for(const auto& w:s.Politics.Wars) if(w.second.Active) wars.push_back(w.first);
    for(Id id:wars) {
        const auto war=s.Politics.Wars.at(id);
        for(Id d:{war.Attacker,war.Defender}) if(!Strength(s,id,d)) {
            const Id ruler=s.Domains.at(d).HeadCharacterId; if(!Adult(s,ruler) || Protected(s,ruler)) continue;
            auto r=world.RaiseArmy(Op(s,"muster-"+std::to_string(id),d),ruler,id); if(!r && r.Code!="politics_ineligible") return r;
        }
        if(s.Day-war.LastBattleDay>=policy.TickDays) { auto r=world.ResolveWarRound(Op(s,"battle",id),id); if(!r) return r; }
    }
    return Ok();
}
}
