#include "WorldPolitics.h"
#include <algorithm>
#include <cmath>

namespace Birthright::Campaign
{
std::uint64_t ForceStrength(const Snapshot& s, Id war, Id domain, Id expedition)
{
    std::uint64_t n=0;
    for(const auto& row:s.Politics.Armies) {
        const auto& a=row.second;
        if(a.War==war && a.Domain==domain && a.Expedition==expedition) n+=a.Soldiers;
    }
    return n;
}
std::uint64_t StrategicLosses(std::uint64_t own, double enemy, double ownReady, double enemyReady, double fraction)
{
    const double ratio=own ? std::min(2.0,enemy*enemyReady/(own*std::max(0.1,ownReady))) : 0;
    return std::min(own,static_cast<std::uint64_t>(std::ceil(own*fraction*ratio)));
}
void EnlistContingent(Snapshot& s, Id war, Id expedition, Id domain, Id lord, Id home, Id culture, std::uint32_t n)
{
    auto& cohorts=s.Areas.at(home).Cohorts;
    auto c=std::find_if(cohorts.begin(),cohorts.end(),[&](const Cohort& v){return v.CultureId==culture;});
    c->Population-=n;
    ArmyContingent a; a.Key=s.Politics.NextArmy++; a.War=war; a.Expedition=expedition;
    a.Domain=domain; a.Lord=lord; a.HomeArea=home; a.Culture=culture; a.Soldiers=n;
    s.Politics.Armies.emplace(a.Key,a); s.Politics.Levied+=n;
}
void ApplyForceLosses(Snapshot& s, Id war, Id domain, std::uint64_t loss, bool deaths, Id expedition)
{
    const auto total=ForceStrength(s,war,domain,expedition); loss=std::min(loss,total);
    std::uint64_t cumulative=0,allocated=0;
    if(!total || !loss) return;
    for(auto i=s.Politics.Armies.begin();i!=s.Politics.Armies.end();) {
        auto& a=i->second;
        if(a.War!=war || a.Domain!=domain || a.Expedition!=expedition) { ++i; continue; }
        cumulative+=a.Soldiers;
        const auto target=cumulative==total?loss:static_cast<std::uint64_t>(std::floor(static_cast<long double>(loss)*cumulative/total));
        const auto take=static_cast<std::uint32_t>(target-allocated); allocated=target; a.Soldiers-=take;
        if(deaths) s.Politics.Casualties+=take;
        else {
            auto& cohorts=s.Areas.at(a.HomeArea).Cohorts;
            auto c=std::find_if(cohorts.begin(),cohorts.end(),[&](const Cohort& v){return v.CultureId==a.Culture;});
            if(c==cohorts.end()) { Cohort v; v.CultureId=a.Culture; v.Population=take; cohorts.push_back(v); }
            else c->Population+=take;
            s.Politics.Returned+=take;
        }
        if(!a.Soldiers) i=s.Politics.Armies.erase(i); else ++i;
    }
}
}
