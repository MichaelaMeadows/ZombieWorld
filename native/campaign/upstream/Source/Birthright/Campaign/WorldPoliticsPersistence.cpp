#include "WorldPolitics.h"
#include <istream>
#include <ostream>

namespace Birthright::Campaign
{
void WritePolitics(const Snapshot& s,std::ostream& o)
{
    const auto& p=s.Politics; const auto& r=p.Policy;
    o<<"POLITICS_METADATA "<<r.Version<<' '<<r.Enabled<<' '<<r.TickDays<<' '<<r.PowerPerHolding<<' '<<r.FriendshipDecay<<' '<<r.LoyaltyResponse<<' '<<r.FactionThreshold<<' '<<r.ReconcileThreshold<<' '<<r.GrievanceTicks<<' '<<r.RebellionTicks<<' '<<r.RebellionShare<<' '<<r.FamilyIncomeShare<<' '<<r.LevyFraction<<' '<<r.MusterCost<<' '<<r.SoldierUpkeep<<' '<<r.CasualtyFraction<<' '<<r.WarRounds<<' '<<r.PeaceDays<<' '<<r.AnnualWarChance<<'\n';
    o<<r.RequireCurrentGrievance<<'\n';
    o<<p.NextFaction<<' '<<p.NextArmy<<' '<<p.NextWar<<' '<<p.Levied<<' '<<p.Casualties<<' '<<p.Returned<<'\n';
    o<<p.Families.size()<<'\n'; for(const auto& row:p.Families) { const auto& f=row.second; o<<row.first<<' '<<f.Founder<<' '<<f.Wealth<<' '<<f.Received<<' '<<f.Spent<<' '<<f.StandardOfLiving<<' '<<f.LastPurchaseDay<<'\n'; }
    o<<p.CharacterFamilies.size()<<'\n'; for(const auto& c:p.CharacterFamilies) o<<c.first<<' '<<c.second<<'\n';
    o<<p.Lords.size()<<'\n'; for(const auto& row:p.Lords) { const auto& l=row.second; o<<row.first<<' '<<l.Domain<<' '<<l.Liege<<' '<<l.Friendship<<' '<<l.Loyalty<<' '<<l.Grievance<<'\n'; }
    o<<p.Factions.size()<<'\n'; for(const auto& row:p.Factions) { const auto& f=row.second; o<<row.first<<' '<<f.Domain<<' '<<f.Leader<<' '<<f.CreatedDay<<' '<<static_cast<unsigned>(f.Status)<<' '<<f.Members.size(); for(Id id:f.Members) o<<' '<<id; o<<'\n'; }
    o<<p.Armies.size()<<'\n'; for(const auto& row:p.Armies) { const auto& a=row.second; o<<row.first<<' '<<a.War<<' '<<a.Domain<<' '<<a.Lord<<' '<<a.HomeArea<<' '<<a.Culture<<' '<<a.Soldiers<<' '<<a.Expedition<<'\n'; }
    o<<p.Wars.size()<<'\n'; for(const auto& row:p.Wars) { const auto& w=row.second; o<<row.first<<' '<<w.Attacker<<' '<<w.Defender<<' '<<w.TargetRegion<<' '<<w.StartedDay<<' '<<w.LastBattleDay<<' '<<w.Rounds<<' '<<w.Active<<' '<<w.Rebellion<<'\n'; }
    o<<p.LastAttention.size()<<'\n'; for(const auto& a:p.LastAttention) o<<a.first<<' '<<a.second<<'\n';
    o<<p.PeaceUntil.size()<<'\n'; for(const auto& a:p.PeaceUntil) o<<a.first<<' '<<a.second<<'\n';
    o<<p.RetiredDomains.size()<<'\n'; for(Id id:p.RetiredDomains) o<<id<<'\n';
}
Result ReadPolitics(std::istream& in,Snapshot& s)
{
    auto& p=s.Politics; auto& r=p.Policy; std::string tag; unsigned enabled=0; std::size_t n=0;
    const auto bad=[] { return Result{false,"politics_save_invalid","Malformed/oversized politics record."}; };
    const auto count=[&](std::size_t limit) { return static_cast<bool>(in>>n) && n<=limit; };
    if(!(in>>tag>>r.Version>>enabled>>r.TickDays>>r.PowerPerHolding>>r.FriendshipDecay>>r.LoyaltyResponse>>r.FactionThreshold>>r.ReconcileThreshold>>r.GrievanceTicks>>r.RebellionTicks>>r.RebellionShare>>r.FamilyIncomeShare>>r.LevyFraction>>r.MusterCost>>r.SoldierUpkeep>>r.CasualtyFraction>>r.WarRounds>>r.PeaceDays>>r.AnnualWarChance) || tag!="POLITICS_METADATA" || enabled>1) return bad();
    if(r.Version!=1 && r.Version!=2) return bad();
    const auto oldVersion=r.Version;
    if(oldVersion>=2) { unsigned current; if(!(in>>current) || current>1) return bad(); r.RequireCurrentGrievance=current!=0; }
    r.Version=2; r.Enabled=enabled!=0; if(!(in>>p.NextFaction>>p.NextArmy>>p.NextWar>>p.Levied>>p.Casualties>>p.Returned)) return bad();
    if(!count(1000000)) return bad(); for(std::size_t i=0;i<n;++i) { Id id; FamilyEstate f; if(!(in>>id>>f.Founder>>f.Wealth>>f.Received>>f.Spent>>f.StandardOfLiving>>f.LastPurchaseDay) || !p.Families.emplace(id,f).second) return bad(); }
    if(!count(s.Characters.size())) return bad(); for(std::size_t i=0;i<n;++i) { Id id,f; if(!(in>>id>>f) || !p.CharacterFamilies.emplace(id,f).second) return bad(); }
    if(!count(s.Characters.size())) return bad(); for(std::size_t i=0;i<n;++i) { Id id; LordRelationship l; if(!(in>>id>>l.Domain>>l.Liege>>l.Friendship>>l.Loyalty>>l.Grievance) || !p.Lords.emplace(id,l).second) return bad(); }
    if(!count(100000)) return bad(); for(std::size_t i=0;i<n;++i) { PoliticalFaction f; unsigned status; std::size_t members; if(!(in>>f.Key>>f.Domain>>f.Leader>>f.CreatedDay>>status>>members) || status>2 || members>s.Characters.size()) return bad(); f.Status=static_cast<FactionStatus>(status); for(std::size_t j=0;j<members;++j) { Id id; if(!(in>>id)) return bad(); f.Members.push_back(id); } if(!p.Factions.emplace(f.Key,f).second) return bad(); }
    if(!count(1000000)) return bad(); for(std::size_t i=0;i<n;++i) { ArmyContingent a; if(!(in>>a.Key>>a.War>>a.Domain>>a.Lord>>a.HomeArea>>a.Culture>>a.Soldiers) || (oldVersion>=2 && !(in>>a.Expedition)) || !p.Armies.emplace(a.Key,a).second) return bad(); }
    if(!count(100000)) return bad(); for(std::size_t i=0;i<n;++i) { CampaignWar w; unsigned active,rebel; if(!(in>>w.Key>>w.Attacker>>w.Defender>>w.TargetRegion>>w.StartedDay>>w.LastBattleDay>>w.Rounds>>active>>rebel) || active>1 || rebel>1) return bad(); w.Active=active!=0; w.Rebellion=rebel!=0; if(!p.Wars.emplace(w.Key,w).second) return bad(); }
    if(!count(s.Characters.size())) return bad(); for(std::size_t i=0;i<n;++i) { Id id; std::int64_t day; if(!(in>>id>>day) || !p.LastAttention.emplace(id,day).second) return bad(); }
    if(!count(s.Domains.size())) return bad(); for(std::size_t i=0;i<n;++i) { Id id; std::int64_t day; if(!(in>>id>>day) || !p.PeaceUntil.emplace(id,day).second) return bad(); }
    if(!count(s.Domains.size())) return bad(); for(std::size_t i=0;i<n;++i) { Id id; if(!(in>>id) || !p.RetiredDomains.insert(id).second) return bad(); }
    return {true,"accepted",""};
}
void ExportPolitics(const Snapshot& s,std::ostream& o)
{
    const auto& p=s.Politics; const auto& r=p.Policy;
    o<<",\n\"politics\":{\"version\":"<<r.Version<<",\"enabled\":"<<(r.Enabled?"true":"false")
        <<",\"policy\":{\"tickDays\":"<<r.TickDays<<",\"powerPerHolding\":"<<r.PowerPerHolding<<",\"friendshipDecay\":"<<r.FriendshipDecay
        <<",\"loyaltyResponse\":"<<r.LoyaltyResponse<<",\"factionThreshold\":"<<r.FactionThreshold<<",\"reconcileThreshold\":"<<r.ReconcileThreshold
        <<",\"grievanceTicks\":"<<r.GrievanceTicks<<",\"rebellionTicks\":"<<r.RebellionTicks<<",\"rebellionShare\":"<<r.RebellionShare
        <<",\"familyIncomeShare\":"<<r.FamilyIncomeShare<<",\"levyFraction\":"<<r.LevyFraction<<",\"musterCost\":"<<r.MusterCost
        <<",\"soldierUpkeep\":"<<r.SoldierUpkeep<<",\"casualtyFraction\":"<<r.CasualtyFraction<<",\"warRounds\":"<<r.WarRounds
        <<",\"peaceDays\":"<<r.PeaceDays<<",\"annualWarChance\":"<<r.AnnualWarChance<<",\"requireCurrentGrievance\":"<<(r.RequireCurrentGrievance?"true":"false")<<"},\"levied\":"<<p.Levied<<",\"casualties\":"<<p.Casualties<<",\"returned\":"<<p.Returned<<",\"families\":[";
    bool comma=false; for(const auto& row:p.Families) { if(comma)o<<','; comma=true; const auto& f=row.second; o<<"{\"id\":"<<row.first<<",\"founderId\":"<<f.Founder<<",\"wealth\":"<<f.Wealth<<",\"received\":"<<f.Received<<",\"spent\":"<<f.Spent<<",\"standardOfLiving\":"<<f.StandardOfLiving<<'}'; }
    o<<"],\"memberships\":["; comma=false; for(const auto& c:p.CharacterFamilies) { if(comma)o<<','; comma=true; o<<"{\"characterId\":"<<c.first<<",\"familyId\":"<<c.second<<'}'; }
    o<<"],\"lords\":["; comma=false; for(const auto& row:p.Lords) { if(comma)o<<','; comma=true; const auto& l=row.second; o<<"{\"characterId\":"<<row.first<<",\"domainId\":"<<l.Domain<<",\"liegeId\":"<<l.Liege<<",\"friendship\":"<<l.Friendship<<",\"loyalty\":"<<l.Loyalty<<",\"grievanceTicks\":"<<l.Grievance<<",\"desiredHoldings\":"<<DesiredHoldings(s,row.first)<<",\"heldRegions\":"<<HeldRegions(s,row.first)<<'}'; }
    o<<"],\"factions\":["; comma=false; for(const auto& row:p.Factions) { if(comma)o<<','; comma=true; const auto& f=row.second; o<<"{\"id\":"<<f.Key<<",\"domainId\":"<<f.Domain<<",\"leaderId\":"<<f.Leader<<",\"status\":"<<static_cast<unsigned>(f.Status)<<",\"members\":["; for(std::size_t i=0;i<f.Members.size();++i) { if(i)o<<','; o<<f.Members[i]; } o<<"]}"; }
    o<<"],\"armies\":["; comma=false; for(const auto& row:p.Armies) { if(comma)o<<','; comma=true; const auto& a=row.second; o<<"{\"id\":"<<a.Key<<",\"warId\":"<<a.War<<",\"domainId\":"<<a.Domain<<",\"lordId\":"<<a.Lord<<",\"homeAreaId\":"<<a.HomeArea<<",\"cultureId\":"<<a.Culture<<",\"soldiers\":"<<a.Soldiers<<",\"expeditionId\":"<<a.Expedition<<'}'; }
    o<<"],\"wars\":["; comma=false; for(const auto& row:p.Wars) { if(comma)o<<','; comma=true; const auto& w=row.second; o<<"{\"id\":"<<w.Key<<",\"attacker\":"<<w.Attacker<<",\"defender\":"<<w.Defender<<",\"targetRegion\":"<<w.TargetRegion<<",\"rounds\":"<<w.Rounds<<",\"active\":"<<(w.Active?"true":"false")<<",\"rebellion\":"<<(w.Rebellion?"true":"false")<<'}'; }
    o<<"],\"retiredDomains\":["; comma=false; for(Id id:p.RetiredDomains) { if(comma)o<<','; comma=true; o<<id; } o<<"]}";
}
}
