#include "Simulation.h"
#include "WorldPolitics.h"
#include "EmpireGeneration.h"
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace Birthright::Campaign::Testing
{
namespace
{
void Check(bool value,const std::string& detail) { if(!value) throw std::runtime_error(detail); }
void Accept(const Result& r) { Check(r.Accepted,r.Code+": "+r.Detail); }
std::string SaveText(const World& w) { std::ostringstream o; Accept(Save(w.State(),o)); return o.str(); }
World Fixture(unsigned regions=4)
{
    std::vector<GeographicArea> graph; Accept(MakeGridGeography(16,8,42,graph));
    EmpireGeneration g; g.EmpireCount=2; g.RegionsPerEmpire=regions; g.ClaimRadius=100; g.InfluenceRadius=100; g.Seeds={{1,Deity::Harmony},{128,Deity::Strife}};
    Rules r; r.DailyBirthRate=0; r.DailyDeathRate=0; r.DailyStewardFavor=0;
    Snapshot s; Accept(GenerateEmpires(graph,g,r,s)); s.Connectivity.MigrationDailyFraction=0;
    for(auto& c:s.Characters) c.second.BornDay=-s.Policy.AdultAgeDays-1;
    World w; Accept(w.Restore(s)); EvolutionRules e; e.Enabled=true; e.AnnualBirthChance=0; Accept(w.ConfigureEvolution("evolution",e));
    PoliticsRules p; p.Enabled=true; p.AnnualWarChance=0; p.RebellionShare=0.1; Accept(w.ConfigurePolitics("politics",p));
    s=w.State(); for(auto& d:s.Evolution.Domains) d.second.Treasury=d.second.Revenue=100000; Accept(w.Restore(s)); return w;
}
std::vector<Id> Lords(const Snapshot& s,Id domain) { std::vector<Id> ids; for(const auto& l:s.Politics.Lords) if(l.second.Domain==domain) ids.push_back(l.first); return ids; }
void AtDay(World& w,std::int64_t day) { auto s=w.State(); s.Day=day; Accept(w.Restore(s)); }
Id Target(const Snapshot& s,Id domain) { for(const auto& r:s.Regions) if(r.second.DomainId==domain) return r.first; return 0; }
}
void AddPoliticsTests(std::vector<std::pair<std::string,std::function<void()>>>& cases)
{
    cases.push_back({"BR-066 recovered territorial supporters cannot reuse old faction age",[] {
        auto w=Fixture();auto s=w.State();const auto lords=Lords(s,1);
        for(Id id:lords){auto& l=s.Politics.Lords.at(id);l.Loyalty=0.1;l.Friendship=0;l.Grievance=30;}Accept(w.Restore(s));
        for(Id id:lords)Accept(w.JoinFaction("join/"+std::to_string(id),id));AtDay(w,360);
        s=w.State();for(Id id:lords){s.Politics.Lords.at(id).Loyalty=0.5;s.Politics.Lords.at(id).Grievance=0;}Accept(w.Restore(s));
        const auto before=SaveText(w);Check(!w.Rebel("recovered",1) && SaveText(w)==before,"Recovered territory supplied a rebellion");
        auto historical=w;s=historical.State();s.Politics.Policy.RequireCurrentGrievance=false;Accept(historical.Restore(s));Accept(historical.Rebel("old-policy",1));
        s=w.State();for(Id id:lords){s.Politics.Lords.at(id).Loyalty=0.1;s.Politics.Lords.at(id).Grievance=30;}Accept(w.Restore(s));Accept(w.Rebel("neglected",1));
    }});
    cases.push_back({"BR-058 travel preserves landed allegiance and policy corruption is atomic",[] {
        auto w=Fixture(); auto s=w.State(); const Id ruler=s.Domains.at(1).HeadCharacterId, lord=Lords(s,1).front();
        Check(HeldRegions(s,lord)>0,"Fixture lord lacks territory"); s.Characters.at(lord).AreaId=s.Domains.at(2).CapitalAreaId; Accept(w.Restore(s));
        Accept(w.AdvanceDays(30)); Check(w.State().Politics.Lords.at(lord).Domain==1,"Travel switched allegiance");
        Check(!w.BefriendLord("foreign-liege",w.State().Domains.at(2).HeadCharacterId,lord),"Foreign ruler befriended vassal as liege");
        Check(w.State().Politics.Lords.at(lord).Liege==ruler,"Travel replaced liege");
        const auto before=SaveText(w); s=w.State(); s.Politics.Policy.LevyFraction=1; Check(!w.Restore(s),"Unbounded levy accepted"); Check(SaveText(w)==before,"Invalid policy mutated state");
    }});
    cases.push_back({"BR-058 growing power raises expectations and friendship consumes budget/attention",[] {
        auto w=Fixture(); const Id ruler=w.State().Domains.at(1).HeadCharacterId, lord=Lords(w.State(),1).front();
        const double before=DesiredHoldings(w.State(),lord); Accept(w.AwardFavor("power",lord,Deity::Harmony,100000,"ambition test"));
        Check(DesiredHoldings(w.State(),lord)>before,"Power did not increase wants");
        const auto loyalty=w.State().Politics.Lords.at(lord).Loyalty, treasury=w.State().Evolution.Domains.at(1).Treasury;
        Accept(w.BefriendLord("friend",ruler,lord)); Check(w.State().Politics.Lords.at(lord).Loyalty>loyalty && w.State().Evolution.Domains.at(1).Treasury<treasury,"Friendship had no cost/effect");
        const auto after=SaveText(w); Check(!w.BefriendLord("again",ruler,lord),"Attention cooldown bypassed"); Check(!w.BefriendLord("friend",ruler,lord),"Replay accepted"); Check(SaveText(w)==after,"Rejected action changed state");
    }});
    cases.push_back({"BR-058 territory grants require personal demesne and improve lord satisfaction",[] {
        auto w=Fixture(); auto s=w.State(); const Id ruler=s.Domains.at(1).HeadCharacterId, lord=Lords(s,1).back(), region=Target(s,1);
        Check(!w.GrantRegion("not-yours",ruler,lord,region),"Ruler gave away another lord's land");
        s.Entities.at(s.Regions.at(region).OwnerEntityId).HeadCharacterId=ruler; Accept(w.Restore(s)); const auto before=HeldRegions(w.State(),lord);
        Accept(w.GrantRegion("grant",ruler,lord,region)); Check(HeldRegions(w.State(),lord)==before+1 && w.State().Politics.Lords.at(lord).Grievance==0,"Grant lost ownership/satisfaction"); Accept(Validate(w.State()));
    }});
    cases.push_back({"BR-058 family wealth accumulates spends and survives a parent's death",[] {
        auto w=Fixture(); Accept(SeedBirthrightFamilies(w,"family")); Accept(w.AdvanceDays(360));
        double wealth=0,spent=0; for(const auto& f:w.State().Politics.Families) { wealth+=f.second.Wealth; spent+=f.second.Spent; }
        Check(wealth>0 && spent>0,"Households did not earn/spend");
        const auto child=w.State().Characters.rbegin()->second; const Id family=w.State().Politics.CharacterFamilies.at(child.Key), parent=child.Parents.front();
        Check(family==w.State().Politics.CharacterFamilies.at(parent),"Child did not share lineage estate"); const auto balance=w.State().Politics.Families.at(family).Wealth;
        Accept(w.RecordDeath("death",parent,"test inheritance")); Check(w.State().Politics.Families.at(family).Wealth==balance,"Death deleted family wealth");
        auto corrupt=w.State(); corrupt.Politics.Families.at(family).Wealth+=1; Check(!w.Restore(corrupt),"Unfunded wealth accepted");
    }});
    cases.push_back({"BR-058 loyal lords reject factions and reconciliation dissolves grievances",[] {
        auto w=Fixture(); const Id lord=Lords(w.State(),1).front(); Check(!w.JoinFaction("loyal",lord),"Loyal lord formed faction");
        auto s=w.State(); auto& l=s.Politics.Lords.at(lord); l.Loyalty=0.1; l.Friendship=0; l.Grievance=20; Accept(w.Restore(s)); Accept(w.JoinFaction("join",lord));
        s=w.State(); s.Politics.Lords.at(lord).Loyalty=0.95; s.Politics.Lords.at(lord).Friendship=1; Accept(w.Restore(s)); Accept(w.AdvanceDays(30));
        Check(w.State().Politics.Factions.begin()->second.Status==FactionStatus::Resolved,"Reconciled faction persisted");
    }});
    cases.push_back({"BR-058 powerful neglected lords rebel while maintained friendships prevent rebellion",[] {
        auto maintained=Fixture(); const auto lords=Lords(maintained.State(),1);
        for(Id lord:lords) Accept(maintained.AwardFavor("ambition/"+std::to_string(lord),lord,Deity::Harmony,100000,"ambition scenario"));
        auto neglected=maintained; auto s=neglected.State();
        // A protected ruler takes no autonomous political actions; all other rules stay identical.
        s.Evolution.ProtectedCharacters.push_back(s.Domains.at(1).HeadCharacterId); Accept(neglected.Restore(s));
        Accept(maintained.AdvanceDays(3650)); Accept(neglected.AdvanceDays(3650));
        const auto rebellions=[](const World& world) { unsigned n=0; for(const auto& event:world.State().Events) if(event.Kind=="rebellion") ++n; return n; };
        Check(rebellions(neglected)>0,"Sustained neglect never produced rebellion");
        Check(rebellions(maintained)==0,"Maintained relations failed to prevent rebellion");
    }});
    cases.push_back({"BR-058 sustained territorial faction creates a realm and a rebellion war",[] {
        auto w=Fixture(); auto s=w.State(); const auto lords=Lords(s,1);
        for(Id id:lords) { s.Politics.Lords.at(id).Loyalty=0.1; s.Politics.Lords.at(id).Friendship=0; s.Politics.Lords.at(id).Grievance=30; } Accept(w.Restore(s));
        for(Id id:lords) Accept(w.JoinFaction("join/"+std::to_string(id),id));
        Check(!w.Rebel("too-soon",1),"Faction rebelled without organizing"); AtDay(w,360); Accept(w.Rebel("rebel",1));
        Check(w.State().Domains.size()==3 && w.State().Politics.Wars.at(1).Rebellion && w.State().Politics.Factions.at(1).Status==FactionStatus::Rebelled,"Rebellion did not create real territory/war"); Accept(Validate(w.State()));
        Check(!w.Rebel("twice",1),"Rebellion applied twice");
    }});
    cases.push_back({"BR-058 war levies conserve people and casualties never return on demobilization",[] {
        auto w=Fixture(1); auto s=w.State();
        for(auto& a:s.Areas) { const Id d=a.second.RegionId ? s.Regions.at(a.second.RegionId).DomainId : 0; for(auto& c:a.second.Cohorts) c.Population=0; a.second.Cohorts.front().Population=d==1?1000:100; }
        Accept(w.Restore(s)); const auto initial=w.Inspect().Population;
        const Id first=w.State().Domains.at(1).HeadCharacterId, second=w.State().Domains.at(2).HeadCharacterId;
        Accept(w.DeclareWar("war",first,Target(w.State(),2))); Accept(w.RaiseArmy("raise-a",first,1)); Accept(w.RaiseArmy("raise-b",second,1));
        Check(static_cast<std::uint64_t>(initial-w.Inspect().Population)==w.State().Politics.Levied,"Levies duplicated population");
        const auto before=SaveText(w); Check(!w.RaiseArmy("second-muster",first,1),"Army raised twice"); Check(!w.ResolveWarRound("early",1),"Early battle accepted"); Check(SaveText(w)==before,"Rejected military operation mutated state");
        for(int i=1;i<=6 && w.State().Politics.Wars.at(1).Active;++i) { AtDay(w,i*30); Accept(w.ResolveWarRound("battle/"+std::to_string(i),1)); }
        Check(!w.State().Politics.Wars.at(1).Active && w.State().Politics.Armies.empty() && w.State().Politics.Casualties>0,"War did not resolve with casualties");
        Check(static_cast<std::uint64_t>(initial-w.Inspect().Population)==w.State().Politics.Casualties,"Dead soldiers returned or survivors vanished");
        Check(w.State().Politics.RetiredDomains.count(2)==1 && w.State().Cultures.at(2).OriginDomainId==2,"Conquest lost historical domain identity");
        Check(!w.DeclareWar("during-truce",first,Target(w.State(),1)),"Invalid war accepted"); Accept(Validate(w.State()));
    }});
    cases.push_back({"BR-059 reconquest replaces rebel ownership claims without corrupting the map",[] {
        auto w=Fixture(); auto s=w.State(); const auto lords=Lords(s,1); const Id ruler=s.Domains.at(1).HeadCharacterId;
        for(Id id:lords) { s.Politics.Lords.at(id).Loyalty=0.1; s.Politics.Lords.at(id).Friendship=0; s.Politics.Lords.at(id).Grievance=30; } Accept(w.Restore(s));
        for(Id id:lords) Accept(w.JoinFaction("join/"+std::to_string(id),id)); AtDay(w,360); Accept(w.Rebel("rebel",1));
        Accept(w.RaiseArmy("restore-army",ruler,1)); AtDay(w,390); Accept(w.ResolveWarRound("restore-territory",1));
        Check(!w.State().Politics.Wars.at(1).Active,"Undefended reconquest did not resolve"); Accept(Validate(w.State()));
        for(const auto& area:w.State().Areas) if(area.second.RegionId) Check(area.second.Claimants.size()<2,"Recognized holding retained disputed ownership");
    }});
    cases.push_back({"BR-058 denied muster and empty war stay atomic and resolve without population creation",[] {
        auto w=Fixture(1); auto s=w.State(); for(auto& a:s.Areas) for(auto& c:a.second.Cohorts) c.Population=0; Accept(w.Restore(s));
        const Id ruler=w.State().Domains.at(1).HeadCharacterId; Accept(w.DeclareWar("war",ruler,Target(w.State(),2))); const auto before=SaveText(w);
        Check(!w.RaiseArmy("empty",ruler,1),"Raised an empty population"); Check(SaveText(w)==before,"Denied levy mutated public state");
        Accept(w.AdvanceDays(60)); Check(!w.State().Politics.Wars.at(1).Active && w.State().Politics.Levied==0,"Unfunded internal muster broke advance");
    }});
    cases.push_back({"BR-058 political save resume and batching preserve exact state",[] {
        auto a=Fixture(1); const Id ruler=a.State().Domains.at(1).HeadCharacterId; Accept(a.DeclareWar("war",ruler,Target(a.State(),2))); auto b=a;
        Accept(a.AdvanceDays(720)); Accept(b.AdvanceDays(113)); Snapshot s; std::istringstream in(SaveText(b)); Accept(Load(in,s)); Accept(b.Restore(s));
        for(unsigned i=0;i<607;++i) Accept(b.AdvanceDays(1)); Check(SaveText(a)==SaveText(b),"Political state changed across cadence/resume");
        auto legacy=SaveText(Fixture()); legacy.replace(0,std::string("BIRTHRIGHT_WORLD 11").size(),"BIRTHRIGHT_WORLD 8"); legacy.erase(legacy.find("POLITICS_METADATA")); std::istringstream old(legacy); Accept(Load(old,s)); Check(s.Version==11 && !s.Politics.Policy.Enabled && s.Evolution.Policy.Enabled,"Schema-eight migration changed legacy policy");
    }});
    cases.push_back({"BR-058 three seeds sustain political and economic invariants for fifty years",[] {
        for(unsigned seed:{7U,42U,91U}) { auto w=Fixture(); auto s=w.State(); s.Seed=seed; s.Politics.Policy.AnnualWarChance=0.3; Accept(w.Restore(s)); Accept(w.AdvanceDays(18250)); Accept(Validate(w.State()));
            Check(w.State().Domains.size()>w.State().Politics.RetiredDomains.size() && w.Inspect().Population>0,"All political society vanished");
            Check(w.State().Politics.Casualties+w.State().Politics.Returned<=w.State().Politics.Levied,"Military ledger broke");
        }
    }});
}
}
