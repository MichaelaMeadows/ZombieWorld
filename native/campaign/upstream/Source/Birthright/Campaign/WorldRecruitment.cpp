#include "World.h"
#include "PlayerProgression.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace Birthright::Campaign {
double CommandLevels(const Snapshot& s, Id leader) {
    const auto p=s.Characters.find(leader); if (p==s.Characters.end()) return 0;
    double total=0, strongest=0;
    for (const auto& a:p->second.Affinities) { const double level=std::floor(Power(a,s.Policy)+1.e-8); total+=level; strongest=std::max(strongest,level); }
    return strongest+.3*(total-strongest);
}
std::uint32_t TroopCapacity(const Snapshot& s, Id leader) { return std::max(10u,static_cast<std::uint32_t>(std::floor(CommandLevels(s,leader)*5+1.e-8))); }
std::uint32_t TroopCount(const Snapshot& s, Id leader) {
    return static_cast<std::uint32_t>(std::count_if(s.Troops.begin(),s.Troops.end(),[&](const auto& t){return t.second.LeaderId==leader;}));
}
namespace {
double Approval(const Snapshot& s, const Character& p, Id culture, std::string& reason) {
    if (p.CultureId==culture) { reason="Shared culture"; return 1; }
    std::vector<Id> pending=p.Parents; std::set<Id> seen;
    while (!pending.empty()) {
        const auto id=pending.back(); pending.pop_back();
        if (!seen.insert(id).second) continue;
        const auto a=s.Characters.find(id); if (a==s.Characters.end()) continue;
        if (a->second.CultureId==culture) { reason="Respected local ancestry"; return .75; }
        pending.insert(pending.end(),a->second.Parents.begin(),a->second.Parents.end());
    }
    const auto patron=s.Cultures.at(culture).Patron;
    const auto d=static_cast<std::size_t>(patron);
    if (d<DeityCount && (p.Affinities[d].BirthrightPower>0 || p.Birthright[d].FullPower>0)) {
        reason="Recognized divine lineage"; return .5;
    }
    reason="Your lineage is not yet trusted"; return 0;
}
bool SameRegion(const Area& here, const Area& other) { return here.RegionId ? here.RegionId==other.RegionId : here.Key==other.Key; }
}
std::vector<RecruitmentOffer> LocalRecruitment(const Snapshot& s, Id leader) {
    std::vector<RecruitmentOffer> offers;
    const auto p=s.Characters.find(leader); if (p==s.Characters.end() || !p->second.Alive) return offers;
    const auto location=s.Areas.find(p->second.AreaId); if (location==s.Areas.end() || location->second.IsWater) return offers;
    const auto& here=location->second;
    for (const auto& local:here.Cohorts) {
        if (local.Population<=0) continue;
        RecruitmentOffer offer; offer.CultureId=local.CultureId;
        const double approval=Approval(s,p->second,local.CultureId,offer.Approval);
        double willing=0;
        for (const auto& a:s.Areas) if (!a.second.IsWater && SameRegion(here,a.second))
            for (const auto& c:a.second.Cohorts) if (c.CultureId==local.CultureId)
                willing+=static_cast<double>(c.Population)*s.Cultures.at(c.CultureId).Occupations.Roles[1]*.01*
                    (1-a.second.Unrest)*(.5+.5*a.second.Prosperity);
        const auto stock=static_cast<std::uint32_t>(std::floor(std::min(20.0,willing*approval)));
        std::uint64_t used=0;
        for (const auto& u:s.RecruitmentUses) if (u.CultureId==local.CultureId && SameRegion(here,s.Areas.at(u.AreaId))) used+=u.Count;
        offer.Available=static_cast<std::uint32_t>(std::min<std::uint64_t>(local.Population,stock>used ? stock-used : 0));
        offers.push_back(offer);
    }
    return offers;
}
Result ValidateWarbands(const Snapshot& s) {
    if (!s.NextTroopId || s.Troops.size()>1000000 || s.RecruitmentUses.size()>1000000)
        return {false,"warband_invalid","Invalid troop sequence or oversized roster."};
    std::map<Id,std::uint32_t> counts;
    for (const auto& t:s.Troops) {
        const auto& u=t.second;
        if (!u.Key || u.Key!=t.first || u.Key>=s.NextTroopId || !s.Characters.count(u.LeaderId) ||
            !s.Cultures.count(u.CultureId) || !s.Areas.count(u.HomeAreaId) || s.Areas.at(u.HomeAreaId).IsWater || u.Kind!=TroopKind::Peasant)
            return {false,"troop_invalid",std::to_string(t.first)};
        ++counts[u.LeaderId];
    }
    for (const auto& c:counts) if (c.second>TroopCapacity(s,c.first))
        return {false,"warband_capacity",std::to_string(c.first)};
    std::set<std::pair<Id,Id>> pools;
    for (const auto& u:s.RecruitmentUses)
        if (!s.Areas.count(u.AreaId) || s.Areas.at(u.AreaId).IsWater || !s.Cultures.count(u.CultureId) || !u.Count || u.Count>1000000 || !pools.emplace(u.AreaId,u.CultureId).second)
            return {false,"recruitment_pool_invalid",std::to_string(u.AreaId)};
    return {true,"accepted",""};
}
Result World::RecruitPeasants(const std::string& operation, Id leader, Id expectedArea, Id culture, std::uint32_t count) {
    const auto checked=CheckOperation(operation); if (!checked) return checked;
    if (!StateData.Bandits.Engagement.EncounterId.empty()) return {false,"bandit_engaged",operation};
    const auto p=StateData.Characters.find(leader);
    if (p==StateData.Characters.end() || !p->second.Alive || p->second.AreaId!=expectedArea || !count || count>1000000)
        return {false,"recruitment_target_invalid",std::to_string(leader)};
    const auto offers=LocalRecruitment(StateData,leader);
    const auto o=std::find_if(offers.begin(),offers.end(),[&](const auto& v){return v.CultureId==culture;});
    if (o==offers.end() || count>o->Available) return {false,"no_willing_peasants",std::to_string(culture)};
    if (count>TroopCapacity(StateData,leader)-TroopCount(StateData,leader)) return {false,"warband_full",std::to_string(leader)};
    if (StateData.Troops.size()+count>1000000 || StateData.NextTroopId>std::numeric_limits<Id>::max()-count)
        return {false,"troop_id_exhausted",std::to_string(leader)};
    auto next=StateData;
    auto& cohorts=next.Areas.at(expectedArea).Cohorts;
    const auto local=std::find_if(cohorts.begin(),cohorts.end(),[&](const auto& c){return c.CultureId==culture;});
    local->Population-=count;
    auto used=std::find_if(next.RecruitmentUses.begin(),next.RecruitmentUses.end(),[&](const auto& u){return u.AreaId==expectedArea && u.CultureId==culture;});
    if (used==next.RecruitmentUses.end()) next.RecruitmentUses.push_back({expectedArea,culture,count}); else used->Count+=count;
    for (std::uint32_t i=0;i<count;++i) { const auto id=next.NextTroopId++; next.Troops.emplace(id,Troop{id,leader,culture,expectedArea,TroopKind::Peasant}); }
    RewardPlayerHarmony(next,leader,5.0*count);
    next.Events.push_back({operation,"recruit_peasants",next.Day,leader,0,"area="+std::to_string(expectedArea)+" culture="+std::to_string(culture)+" count="+std::to_string(count)});
    return Commit(std::move(next));
}
Result World::DismissTroop(const std::string& operation, Id leader, Id troop) {
    const auto checked=CheckOperation(operation); if (!checked) return checked;
    const auto t=StateData.Troops.find(troop); const auto p=StateData.Characters.find(leader);
    if (p==StateData.Characters.end() || !p->second.Alive || t==StateData.Troops.end() || t->second.LeaderId!=leader)
        return {false,"troop_not_owned",std::to_string(troop)};
    auto next=StateData; next.Troops.erase(troop);
    next.Events.push_back({operation,"dismiss_troop",next.Day,leader,0,"troop="+std::to_string(troop)});
    return Commit(std::move(next));
}
}
