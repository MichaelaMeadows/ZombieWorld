#include "PlayerProgression.h"
#include "EmpireGeneration.h"
#include "Simulation.h"
#include "WorldBandits.h"
#include "WorldEvolution.h"
#include "WorldPolitics.h"
#include "WorldMarket.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace Birthright::Campaign::Testing
{
// Test-only host substitute exercises the same sealed-result authority boundary.
struct CampCombatProbe
{
    static CampCombatEvidence Report(const CampEngagement &e, unsigned dead, bool alive = true)
    {
        return {e.EncounterId, e.CampId, e.Enemies, dead, alive};
    }
};
namespace
{
void Check(bool ok, const std::string &reason)
{
    if (!ok)
        throw std::runtime_error(reason);
}
void Accept(const Result &r)
{
    Check(r.Accepted, r.Code + ": " + r.Detail);
}
std::string Saved(const World &w)
{
    std::ostringstream o;
    Accept(Save(w.State(), o));
    return o.str();
}
World Fixture(unsigned seed = 42)
{
    std::vector<GeographicArea> graph;
    Accept(MakeGridGeography(12, 8, seed, graph));
    EmpireGeneration g;
    g.Seed = seed;
    g.EmpireCount = 2;
    g.RegionsPerEmpire = 2;
    g.ClaimRadius = 3;
    g.InfluenceRadius = 4;
    g.Seeds = {{1, Deity::Harmony}, {96, Deity::Strife}};
    Snapshot s;
    Accept(GenerateEmpires(graph, g, {}, s));
    World w;
    Accept(w.Restore(s));
    EvolutionRules e;
    e.Enabled = true;
    Accept(w.ConfigureEvolution("evolve", e));
    PoliticsRules politics;
    politics.Enabled = true;
    Accept(w.ConfigurePolitics("politics", politics));
    MarketRules market;
    market.Enabled = true;
    Accept(w.ConfigureMarket("market", market));
    BanditRules b;
    b.Enabled = true;
    b.MaxCamps = 4;
    b.OwnerDefense = false; // These BR-064 fixtures isolate unattended raids and player intervention.
    Accept(w.ConfigureBandits("bandits", b));
    return w;
}
Id LeaderAtCamp(World &w, Id camp)
{
    auto s = w.State();
    auto id = s.Characters.begin()->first;
    s.Characters.at(id).AreaId = s.Bandits.Camps.at(camp).AreaId;
    s.Characters.at(id).Alive = true;
    Accept(w.Restore(s));
    return id;
}
double Raided(const World &w)
{
    double n = 0;
    for (const auto &c : w.State().Bandits.Camps)
        n += c.second.FoodRaided;
    return n;
}
World DefenseFixture(unsigned seed=42)
{
    auto w=Fixture(seed);Accept(w.AdvanceDays(90));auto s=w.State();
    s.Policy.DailyBirthRate=0;s.Policy.DailyDeathRate=0;
    for(const auto& region:s.Regions){const Id lord=s.Entities.at(region.second.OwnerEntityId).HeadCharacterId;
        if(!s.Politics.CharacterFamilies.count(lord))continue;
        auto& cash=s.Market.Regions.at(region.first).HouseholdCash;const double amount=std::min(cash*0.5,1000.0);cash-=amount;
        auto& family=s.Politics.Families.at(s.Politics.CharacterFamilies.at(lord));family.Wealth+=amount;family.Received+=amount;
    }
    Accept(w.Restore(s));return w;
}
Id DispatchFirst(World& w)
{
    for(const auto& row:w.State().Bandits.Camps){const auto& c=row.second;if(!c.Active || !c.LastTargetArea)continue;
        const auto region=w.State().Areas.at(c.LastTargetArea).RegionId;const Id owner=w.State().Entities.at(w.State().Regions.at(region).OwnerEntityId).HeadCharacterId;
        auto r=w.DispatchCampExpedition("test-dispatch/"+std::to_string(c.Key),owner,c.Key);if(r)return w.State().Bandits.NextExpedition-1;
    }
    throw std::runtime_error("Fixture had no eligible funded local force");
}
} // namespace
void AddBanditTests(std::vector<std::pair<std::string, std::function<void()>>> &cases)
{
    cases.push_back({"BR013 certified camp favor obligations replay and battle lock", [] {
        for (int mode=0; mode<3; ++mode) {
            auto w=Fixture(); Accept(w.AdvanceDays(90)); auto state=w.State(); Check(!state.Bandits.Camps.empty(),"need camp");
            const auto camp=state.Bandits.Camps.begin()->first;
            Character p; p.Key=10000; p.Name="Player"; p.AreaId=state.Bandits.Camps.at(camp).AreaId; p.CultureId=state.Cultures.begin()->first; p.BornDay=-6570;
            state.Characters.emplace(p.Key,p); Accept(w.Restore(state));
            Accept(w.ChooseOrigin("origin",p.Key,mode==0?"ember_heir":"concord_heir"));
            if(mode==2) { state=w.State(); Troop t; t.Key=state.NextTroopId++;t.LeaderId=p.Key;t.CultureId=p.CultureId;t.HomeAreaId=p.AreaId;state.Troops.emplace(t.Key,t);Accept(w.Restore(state)); }
            Accept(w.BeginCampAssault("BR013-assault",p.Key,camp)); const auto engagement=w.State().Bandits.Engagement;
            Check(!w.RespecDivineNodes("blocked",p.Key) && !w.EquipPlayerItem("blocked-equip",p.Key,EquipmentSlot::Armor,0),"battle editing guard");
            Check(!w.TendWarband("battle-care",p.Key,p.AreaId),"BR079 battle care guard");
            if(mode==2) { WarbandBattleInput input;input.WorldId=w.State().WorldId;input.EncounterId=engagement.EncounterId;input.LeaderId=p.Key;input.AreaId=p.AreaId;input.Troops={w.State().Troops.begin()->first}; WarbandBattleResult result;result.EncounterId=input.EncounterId;result.Casualties=input.Troops;Accept(w.RecordTroopBattleCasualties("BR013/arbitrary-host-operation",input,result)); }
            const auto proof=CampCombatProbe::Report(engagement,engagement.Enemies);Accept(w.ApplyCampCombat(proof));
            Check(w.State().Characters.at(p.Key).Affinities[mode==0?0:1].EarnedFavor==(mode==0?75:0),"favor or Harmony obligation");
            const auto saved=Saved(w);Check(!w.ApplyCampCombat(proof) && Saved(w)==saved,"reward replay atomicity");
        }
    }});

    cases.push_back({"BR-066 mutual destruction attributes camp loss to the paid expedition",[] {
        auto w=DefenseFixture();const Id id=DispatchFirst(w);auto s=w.State();auto& x=s.Bandits.Expeditions.at(id);
        const auto losses=ForceStrength(s,0,x.Domain,id)-1;ApplyForceLosses(s,0,x.Domain,losses,true,id);x.Casualties+=static_cast<unsigned>(losses);
        x.Phase=ExpeditionPhase::Fighting;x.Position=static_cast<unsigned>(x.Route.size()-1);x.NextActionDay=s.Day+1;s.Bandits.Camps.at(x.Camp).Strength=0.8;Accept(w.Restore(s));
        Accept(w.AdvanceDays(2));const auto& done=w.State().Bandits.Expeditions.at(id);
        Check(done.Casualties==done.Mustered && done.Phase==ExpeditionPhase::Finished && w.State().Bandits.Camps.at(done.Camp).Outcome==2,"Mutual destruction misattributed or survivors invented");Accept(Validate(w.State()));
    }});
    cases.push_back({"BR-066 local command rejects unfunded foreign and wartime dispatch atomically",[] {
        auto w=DefenseFixture();auto probe=w;const Id id=DispatchFirst(probe);const auto x=probe.State().Bandits.Expeditions.at(id);
        auto before=Saved(w);Check(!w.DispatchCampExpedition("foreign",999999,x.Camp) && Saved(w)==before,"Foreign owner accepted");
        auto s=w.State();auto& f=s.Politics.Families.at(x.Family);s.Market.Regions.at(x.Region).HouseholdCash+=f.Wealth;f.Spent+=f.Wealth;f.Wealth=0;Accept(w.Restore(s));
        before=Saved(w);Check(!w.DispatchCampExpedition("unfunded",x.Lord,x.Camp) && Saved(w)==before,"Unfunded muster changed state");
        w=DefenseFixture();s=w.State();CampaignWar war;war.Key=s.Politics.NextWar++;war.Attacker=x.Domain;
        for(const auto& r:s.Regions)if(r.second.DomainId!=x.Domain){war.Defender=r.second.DomainId;war.TargetRegion=r.first;break;}
        war.StartedDay=war.LastBattleDay=s.Day;s.Politics.Wars.emplace(war.Key,war);Accept(w.Restore(s));before=Saved(w);
        Check(!w.DispatchCampExpedition("war-commitment",x.Lord,x.Camp) && Saved(w)==before,"Defense ignored an existing war");
        auto corrupt=probe.State();corrupt.Bandits.Expeditions.at(id).Route.front()=999999;before=Saved(probe);
        Check(!probe.Restore(corrupt) && Saved(probe)==before,"Invalid dispatch route restored");
    }});
    cases.push_back({"BR-066 stronger camp defeats an expedition with permanent defender losses",[] {
        auto w=DefenseFixture();const Id id=DispatchFirst(w);auto s=w.State();const Id camp=s.Bandits.Expeditions.at(id).Camp;s.Bandits.Camps.at(camp).Strength=24;Accept(w.Restore(s));
        Accept(w.AdvanceDays(60));const auto& x=w.State().Bandits.Expeditions.at(id);
        Check(x.Phase==ExpeditionPhase::Finished && x.Casualties>0 && w.State().Bandits.Camps.at(camp).Outcome!=2,"Outmatched force received free victory");Accept(Validate(w.State()));
    }});
    cases.push_back({"BR-066 version one raids migrate without invented captured stores",[] {
        auto w=Fixture();Accept(w.AdvanceDays(90));auto s=w.State();
        for(const auto& taken:s.Bandits.FoodTakenByArea)s.Evolution.Areas.at(taken.first).Consumed+=taken.second;
        s.Bandits.FoodTakenByArea.clear();for(auto& row:s.Bandits.Camps){auto& c=row.second;c.LegacyFoodConsumed=c.FoodRaided;c.FoodStored=c.FoodConsumed=c.FoodSpoiled=c.FoodWasted=c.FoodAbandoned=0;}
        Accept(w.Restore(s));auto text=Saved(w);const auto start=text.find("BANDITS_METADATA"),end=text.find("POLITICS_METADATA",start);
        std::istringstream current(text.substr(start,end-start));std::ostringstream old;std::string line,token;std::getline(current,line);std::istringstream header(line);unsigned field=0;
        while(header>>token){if(field==1)token="1";if(field==11)token="0.01";old<<(field?" ":"")<<token;++field;}old<<'\n';std::getline(current,line);
        for(std::size_t i=0;i<s.Bandits.Camps.size();++i){std::getline(current,line);std::istringstream camp(line);for(unsigned f=0;f<10;++f){camp>>token;old<<(f?" ":"")<<token;}old<<'\n';}
        std::getline(current,line);old<<line<<'\n';text.replace(start,end-start,old.str());Snapshot loaded;std::istringstream in(text);Accept(Load(in,loaded));
        Check(loaded.Bandits.Policy.Version==2 && loaded.Bandits.Policy.DailyGrowth==0 && loaded.Bandits.Expeditions.empty(),"Legacy policy migration failed");
        for(const auto& row:loaded.Bandits.Camps)Check(row.second.FoodStored==0 && row.second.LegacyFoodConsumed==row.second.FoodRaided,"Historical loot was recreated");
        Accept(w.Restore(loaded));Accept(w.AdvanceDays(30));Accept(Validate(w.State()));
    }});
    cases.push_back({"BR-066 raiding resources fund growth and empty raids cannot",[] {
        auto good=Fixture();auto bad=good;auto s=bad.State();s.Bandits.Policy.FoodPerStrength=0;Accept(bad.Restore(s));
        Accept(good.AdvanceDays(365));Accept(bad.AdvanceDays(365));double growth=0,failedGrowth=0;unsigned dispersed=0;
        for(const auto& c:good.State().Bandits.Camps)growth+=c.second.LootGrowth;
        for(const auto& c:bad.State().Bandits.Camps){failedGrowth+=c.second.LootGrowth;dispersed+=c.second.Outcome==3;}
        Check(growth>0 && failedGrowth==0 && Raided(bad)==0 && dispersed>0,"Empty raids grew or unsupplied camps persisted indefinitely");
        double balance=0;for(const auto& a:good.State().Evolution.Areas)balance+=a.second.Produced-a.second.Consumed-a.second.Wasted-a.second.FoodStored;
        for(const auto& c:good.State().Bandits.Camps)balance-=c.second.FoodConsumed+c.second.FoodWasted+c.second.FoodStored;
        Check(std::abs(balance)<0.01,"Captured food was minted or counted twice");Accept(Validate(good.State()));Accept(Validate(bad.State()));
    }});
    cases.push_back({"BR-066 local forces pay muster travel fight and return real survivors",[] {
        auto w=DefenseFixture();const auto before=w.Inspect().Population;const auto money=MarketMoney(w.State());const auto id=DispatchFirst(w);const auto x=w.State().Bandits.Expeditions.at(id);
        Check(before-w.Inspect().Population==x.Mustered && x.Cost>0 && x.NextActionDay>w.State().Day,"Free or instantaneous expedition");
        const auto saved=Saved(w);Check(!w.DispatchCampExpedition("duplicate",x.Lord,x.Camp) && Saved(w)==saved,"Duplicate expedition mutated state");
        Accept(w.AdvanceDays(60));const auto& done=w.State().Bandits.Expeditions.at(id);
        Check(done.Phase==ExpeditionPhase::Finished && done.Food>0 && done.Cost>x.Cost && done.Casualties>0,"No supplied strategic conflict/return");
        Check(w.State().Bandits.Camps.at(x.Camp).Outcome==2,"Capable paid owner failed to clear camp");
        Check(before-w.Inspect().Population==done.Casualties,"Soldiers duplicated or casualties returned");Check(std::abs(MarketMoney(w.State())-money)<1e-6,"Defense minted currency");Accept(Validate(w.State()));
    }});
    cases.push_back({"BR-066 unpaid expeditions retreat and save resume preserves movement",[] {
        auto w=DefenseFixture();const Id id=DispatchFirst(w);auto other=w;
        Accept(w.AdvanceDays(60));Snapshot loaded;std::istringstream in(Saved(other));Accept(Load(in,loaded));Accept(other.Restore(loaded));
        for(unsigned day=0;day<60;++day)Accept(other.AdvanceDays(1));Check(Saved(w)==Saved(other) && w.State().Bandits.Expeditions.at(id).Phase==ExpeditionPhase::Finished,"Defense cadence/save diverged");
        auto poor=DefenseFixture();const Id pid=DispatchFirst(poor);auto s=poor.State();auto& f=s.Politics.Families.at(s.Bandits.Expeditions.at(pid).Family);
        s.Market.Regions.at(s.Bandits.Expeditions.at(pid).Region).HouseholdCash+=f.Wealth;f.Spent+=f.Wealth;f.Wealth=0;Accept(poor.Restore(s));
        for(unsigned day=0;day<60;++day){auto cashless=poor.State();for(auto& region:cashless.Market.Regions){region.second.HouseholdCash+=region.second.ProducerCash;region.second.ProducerCash=0;}Accept(poor.Restore(cashless));Accept(poor.AdvanceDays(1));}
        Check(poor.State().Bandits.Expeditions.at(pid).Phase==ExpeditionPhase::Finished,"Unfunded force stranded");
        Check(poor.State().Bandits.Camps.at(poor.State().Bandits.Expeditions.at(pid).Camp).Outcome!=2,"Unfunded force cleared camp for free");Accept(Validate(poor.State()));
    }});
    cases.push_back({"BR-066 player clearing an expedition target cannot duplicate its outcome",[] {
        auto w=DefenseFixture();const Id id=DispatchFirst(w);const auto camp=w.State().Bandits.Expeditions.at(id).Camp;const auto leader=LeaderAtCamp(w,camp);
        Accept(w.BeginCampAssault("player-first",leader,camp));const auto e=w.State().Bandits.Engagement;Accept(w.ApplyCampCombat(CampCombatProbe::Report(e,e.Enemies)));
        Accept(w.AdvanceDays(60));Check(w.State().Bandits.Camps.at(camp).Outcome==1 && w.State().Bandits.Expeditions.at(id).Phase==ExpeditionPhase::Finished,"NPC overwrote player result or failed to return");Accept(Validate(w.State()));
    }});
    cases.push_back({"BR-066 capable owners reduce raids across varied seeds",[] {
        std::ofstream out("Artifacts/World/br-066-defense-impact.jsonl");
        for(unsigned seed:{7U,42U,91U}){auto owner=DefenseFixture(seed);auto ignored=owner;auto s=owner.State();s.Bandits.Policy.OwnerDefense=true;Accept(owner.Restore(s));
            const auto baseline=Raided(owner);Accept(owner.AdvanceDays(365));Accept(ignored.AdvanceDays(365));unsigned clears=0;double cost=0;for(const auto& c:owner.State().Bandits.Camps)clears+=c.second.Outcome==2;for(const auto& x:owner.State().Bandits.Expeditions)cost+=x.second.Cost;
            Check(clears>0 && cost>0 && Raided(owner)-baseline<Raided(ignored)-baseline,"Paid defense lacked loose directional impact");Accept(Validate(owner.State()));
            out<<"{\"seed\":"<<seed<<",\"cleared\":"<<clears<<",\"cost\":"<<cost<<",\"defendedRaidsFood\":"<<Raided(owner)-baseline<<",\"ignoredRaidsFood\":"<<Raided(ignored)-baseline<<"}\n";
        }
    }});
    cases.push_back({"BR-064 frontier placement reachable raids bounded unattended growth", [] {
                         auto w = Fixture();
                         Accept(w.AdvanceDays(365));
                         Check(!w.State().Bandits.Camps.empty(), "No frontier camps");
                         bool grew = false;
                         for (const auto &row : w.State().Bandits.Camps)
                         {
                             const auto &c = row.second;
                             const auto &a = w.State().Areas.at(c.AreaId);
                             Check(!a.IsWater && (!a.RegionId || a.Wilderness || a.Claimants.size() > 1),
                                   "Camp not on frontier");
                             Check(c.Strength <= 24, "Unbounded strength");
                             grew = grew || c.Strength > 3;
                             const auto reach = BanditReach(w.State(), c.AreaId, 2);
                             bool near = false;
                             for (Id id : reach)
                                 near = near || w.State().Areas.at(id).RegionId != 0;
                             Check(near, "Camp cannot reach society");
                         }
                         Check(grew && Raided(w) > 0, "No escalation or economic raids");
                         Accept(Validate(w.State()));
                     }});
    cases.push_back({"BR-064 reach never crosses disconnected land or water", [] {
                         auto w = Fixture();
                         auto s = w.State();
                         const Id source = s.Areas.begin()->first;
                         const auto neighbors = s.Areas.at(source).Neighbors;
                         for (Id n : neighbors)
                         {
                             auto &links = s.Areas.at(n).Neighbors;
                             links.erase(std::remove(links.begin(), links.end(), source), links.end());
                         }
                         s.Areas.at(source).Neighbors.clear();
                         Check(BanditReach(s, source, 4) == std::vector<Id>{source},
                               "Disconnected raid link fabricated");
                         s.Areas.at(source).IsWater = true;
                         Check(BanditReach(s, source, 4).empty(), "Water used as raid origin");
                     }});
    cases.push_back(
        {"BR-064 sealed combat context replay suppression and clearing", [] {
             auto w = Fixture();
             Accept(w.AdvanceDays(90));
             const auto camp = w.State().Bandits.Camps.begin()->first;
             const auto leader = LeaderAtCamp(w, camp);
             const auto before = Saved(w);
             Check(!w.BeginCampAssault("bad", 999999, camp) && Saved(w) == before, "Invalid assault mutated authority");
             Accept(w.BeginCampAssault("fight", leader, camp));
             auto e = w.State().Bandits.Engagement;
             Check(!w.AdvanceDays(1) && !w.BeginCampAssault("duplicate", leader, camp), "Active encounter bypass");
             auto wrong = e;
             wrong.CampId = 999;
             Check(!w.ApplyCampCombat(CampCombatProbe::Report(wrong, e.Enemies)), "Wrong camp accepted");
             Check(!w.ApplyCampCombat(CampCombatProbe::Report(e, e.Enemies + 1)), "Invented extra casualties accepted");
             const auto raidDay = w.State().Bandits.Camps.at(camp).LastRaidDay;
             Accept(w.ApplyCampCombat(CampCombatProbe::Report(e, 0)));
             Check(w.State().Bandits.Camps.at(camp).LastRaidDay == raidDay, "Empty retreat delayed raids");
             Check(!w.ApplyCampCombat(CampCombatProbe::Report(e, e.Enemies)), "Combat replay accepted");
             Accept(w.BeginCampAssault("partial", leader, camp));
             e = w.State().Bandits.Engagement;
             Accept(w.ApplyCampCombat(CampCombatProbe::Report(e, 1, false)));
             Check(w.State().Bandits.Camps.at(camp).Active, "Defeat cleared camp");
             Accept(w.BeginCampAssault("victory", leader, camp));
             e = w.State().Bandits.Engagement;
             Accept(w.ApplyCampCombat(CampCombatProbe::Report(e, e.Enemies)));
             Check(!w.State().Bandits.Camps.at(camp).Active, "Certified victory did not clear");
             const auto food = Raided(w);
             Accept(w.AdvanceDays(30));
             Check(w.State().Bandits.Camps.at(camp).FoodRaided <= food, "Cleared camp raided");
             Check(!w.BeginCampAssault("replay", leader, camp), "Cleared camp reentered");
             Accept(Validate(w.State()));
         }});
    cases.push_back({"BR-064 save migration engagement recovery and partition independence", [] {
                         auto w = Fixture();
                         Accept(w.AdvanceDays(91));
                         auto other = w;
                         Accept(w.AdvanceDays(365));
                         Snapshot loaded;
                         std::istringstream in(Saved(other));
                         Accept(Load(in, loaded));
                         Accept(other.Restore(loaded));
                         for (unsigned d = 0; d < 365; ++d)
                             Accept(other.AdvanceDays(1));
                         Check(Saved(w) == Saved(other), "Camp state depends on step/report cadence");
                         auto legacyWorld=w;auto legacyState=w.State();
                         for(const auto& taken:legacyState.Bandits.FoodTakenByArea) legacyState.Evolution.Areas.at(taken.first).Consumed+=taken.second;
                         legacyState.Bandits={};Accept(legacyWorld.Restore(legacyState));
                         auto text = Saved(legacyWorld);
                         text.replace(text.find("EVOLUTION_METADATA 3"), 20, "EVOLUTION_METADATA 2");
                         const auto start = text.find("BANDITS_METADATA"), end = text.find("POLITICS_METADATA", start);
                         text.erase(start, end - start);
                         std::istringstream old(text);
                         Accept(Load(old, loaded));
                         Check(!loaded.Bandits.Policy.Enabled && loaded.Bandits.Camps.empty(), "Legacy camps invented");
                         const auto camp = w.State().Bandits.Camps.begin()->first;
                         const auto leader = LeaderAtCamp(w, camp);
                         Accept(w.BeginCampAssault("saved-assault", leader, camp));
                         std::istringstream active(Saved(w));
                         Accept(Load(active, loaded));
                         Accept(w.Restore(loaded));
                         Accept(w.AbandonCampAssault("recover", leader));
                         Check(w.State().Bandits.Engagement.EncounterId.empty() &&
                                   w.State().Bandits.Camps.at(camp).Active,
                               "Saved engagement recovery awards clear");
                     }});
    cases.push_back(
        {"BR-064 intervention reduces raids and long worlds retain viability", [] {
             std::ofstream out("Artifacts/World/br-064-impact.jsonl");
             for (unsigned seed : {7U, 42U, 91U})
             {
                 auto w = Fixture(seed);
                 Accept(w.AdvanceDays(365));
                 auto unattended = w;
                 for (const auto &row : unattended.State().Bandits.Camps)
                 {
                     const auto leader = LeaderAtCamp(w, row.first);
                     Accept(w.BeginCampAssault("clear/" + std::to_string(row.first), leader, row.first));
                     auto e = w.State().Bandits.Engagement;
                     Accept(w.ApplyCampCombat(CampCombatProbe::Report(e, e.Enemies)));
                 }
                 const auto initial = w.Inspect().Population;
                 const auto baseline = Raided(w);
                 Accept(w.AdvanceDays(90));
                 Accept(unattended.AdvanceDays(90));
                 Check(Raided(w) - baseline < Raided(unattended) - baseline, "Intervention does not reduce raids");
                 const auto treated = Raided(w) - baseline, untreated = Raided(unattended) - baseline;
                 Accept(unattended.AdvanceDays(7300));
                 Check(unattended.Inspect().Population > initial / 3 && unattended.Inspect().Population < initial * 3,
                       "Bandit world lost viability");
                 Accept(Validate(unattended.State()));
                 out << "{\"seed\":" << seed << ",\"day\":" << unattended.State().Day
                     << ",\"population\":" << unattended.Inspect().Population << ",\"treatedFood90Days\":" << treated
                     << ",\"untreatedFood90Days\":" << untreated << ",\"totalRaided\":" << Raided(unattended) << "}\n";
             }
         }});
    cases.push_back({"BR-064 rendered camp fixture and invalid ledgers", [] {
                         auto w = Fixture();
                         Accept(w.AdvanceDays(90));
                         auto s = w.State();
                         const auto camp = s.Bandits.Camps.begin()->second;
                         Character player;
                         player.Key = 10000;
                         player.Name = "Player";
                         player.AreaId = camp.AreaId;
                         player.CultureId = s.Areas.at(camp.AreaId).Cohorts.front().CultureId;
                         player.BornDay = -s.Policy.AdultAgeDays;
                         s.Characters.emplace(player.Key, player);
                         s.Evolution.ProtectedCharacters.push_back(player.Key);
                         Accept(w.Restore(s));
                         auto recruit = w.State();
                         for (const auto &a : recruit.Areas)
                             if (a.second.RegionId && !a.second.Cohorts.empty())
                             {
                                 recruit.Characters.at(10000).AreaId = a.first;
                                 recruit.Characters.at(10000).CultureId = a.second.Cohorts.front().CultureId;
                                 break;
                             }
                         Accept(w.Restore(recruit));
                         const auto offers = LocalRecruitment(w.State(), 10000);
                         auto offer =
                             std::find_if(offers.begin(), offers.end(), [](const auto &o) { return o.Available >= 3; });
                         Check(offer != offers.end(), "Playable fixture needs recruits");
                         Accept(w.RecruitPeasants("fixture-recruits", 10000, w.State().Characters.at(10000).AreaId,
                                                  offer->CultureId, 3));
                         recruit = w.State();
                         recruit.Characters.at(10000).AreaId = camp.AreaId;
                         Accept(w.Restore(recruit));
                         std::ofstream output("Artifacts/World/br-064-playable.world");
                         Accept(Save(w.State(), output));
                         const auto before = Saved(w);
                         for (int kind = 0; kind < 3; ++kind)
                         {
                             auto bad = w.State();
                             if (kind == 0)
                                 bad.Bandits.Policy.RaidHops = 100;
                             if (kind == 1)
                                 bad.Bandits.Camps.begin()->second.Strength = -1;
                             if (kind == 2)
                                 bad.Bandits.Camps.begin()->second.FoodRaided = 1e23;
                             Check(!w.Restore(bad) && Saved(w) == before, "Invalid bandit state changed authority");
                         }
                     }});
}
} // namespace Birthright::Campaign::Testing
