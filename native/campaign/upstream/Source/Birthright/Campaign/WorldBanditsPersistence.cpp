#include "WorldBandits.h"
#include <iomanip>
#include <istream>
#include <ostream>
namespace Birthright::Campaign
{
void WriteBandits(const Snapshot& s,std::ostream& o)
{
    const auto& b=s.Bandits; const auto& p=b.Policy;
    o<<"BANDITS_METADATA "<<p.Version<<' '<<p.Enabled<<' '<<p.SpawnDays<<' '<<p.RaidDays<<' '<<p.RecoveryDays<<' '<<p.RaidHops<<' '<<p.SpacingHops<<' '<<p.MaxCamps<<' '<<p.InitialStrength<<' '<<p.MaximumStrength<<' '<<p.DailyGrowth<<' '<<p.RaidGrowth<<' '<<p.RaidStockFraction<<' '<<p.FoodPerStrength<<' '<<p.MaximumDevelopment<<' '<<b.NextId<<' '<<b.Camps.size()<<'\n';
    o<<p.FoodUpkeep<<' '<<p.GrowthFood<<' '<<p.StarvationLoss<<' '<<p.StoragePerStrength<<' '<<p.OwnerDefense<<' '<<p.ResponseDays<<' '<<p.DefenseBattleDays<<' '<<p.DefenseMaxRounds<<' '<<p.DefenseCooldownDays<<' '<<p.DefenseStrengthRatio<<' '<<p.DefenseWealthReserve<<' '<<p.DefenseFoodReserveDays<<'\n';
    for(const auto& row:b.Camps) { const auto& c=row.second;
        o<<c.Key<<' '<<c.AreaId<<' '<<c.Active<<' '<<c.BornDay<<' '<<c.LastRaidDay<<' '<<c.SuppressedUntil<<' '<<c.ClearedDay<<' '<<c.Strength<<' '<<c.FoodRaided<<' '<<c.Raids<<' '<<c.FoodStored<<' '<<c.FoodConsumed<<' '<<c.FoodSpoiled<<' '<<c.FoodWasted<<' '<<c.FoodAbandoned<<' '<<c.LegacyFoodConsumed<<' '<<c.LootGrowth<<' '<<c.StarvationLosses<<' '<<c.LastTargetArea<<' '<<c.Outcome<<'\n';
    }
    const auto& e=b.Engagement;
    o<<std::quoted(e.EncounterId)<<' '<<e.CampId<<' '<<e.LeaderId<<' '<<e.AreaId<<' '<<e.Day<<' '<<e.Enemies<<'\n';
    o<<b.FoodTakenByArea.size()<<'\n'; for(const auto& row:b.FoodTakenByArea) o<<row.first<<' '<<row.second<<'\n';
    o<<b.NextExpedition<<' '<<b.Expeditions.size()<<'\n';
    for(const auto& row:b.Expeditions) { const auto& x=row.second;
        o<<x.Key<<' '<<x.Camp<<' '<<x.Domain<<' '<<x.Region<<' '<<x.Lord<<' '<<x.Family<<' '<<x.HomeArea<<' '<<static_cast<unsigned>(x.Phase)<<' '<<x.Position<<' '<<x.Rounds<<' '<<x.Mustered<<' '<<x.Casualties<<' '<<x.StartedDay<<' '<<x.NextActionDay<<' '<<x.FinishedDay<<' '<<x.Readiness<<' '<<x.Cost<<' '<<x.Food<<' '<<x.Route.size();
        for(Id a:x.Route) o<<' '<<a; o<<'\n';
    }
}
Result ReadBandits(std::istream& in,Snapshot& s)
{
    auto& b=s.Bandits; auto& p=b.Policy; std::string tag; unsigned enabled; std::size_t n;
    const auto bad=[](){return Result{false,"bandit_save_invalid","Malformed camp resources or expedition record"};};
    if(!(in>>tag>>p.Version>>enabled>>p.SpawnDays>>p.RaidDays>>p.RecoveryDays>>p.RaidHops>>p.SpacingHops>>p.MaxCamps>>p.InitialStrength>>p.MaximumStrength>>p.DailyGrowth>>p.RaidGrowth>>p.RaidStockFraction>>p.FoodPerStrength>>p.MaximumDevelopment>>b.NextId>>n) || tag!="BANDITS_METADATA" || enabled>1 || n>100000 || (p.Version!=1 && p.Version!=2)) return bad();
    const auto version=p.Version; p.Enabled=enabled!=0;
    if(version==2) { unsigned defend;
        if(!(in>>p.FoodUpkeep>>p.GrowthFood>>p.StarvationLoss>>p.StoragePerStrength>>defend>>p.ResponseDays>>p.DefenseBattleDays>>p.DefenseMaxRounds>>p.DefenseCooldownDays>>p.DefenseStrengthRatio>>p.DefenseWealthReserve>>p.DefenseFoodReserveDays) || defend>1) return bad();
        p.OwnerDefense=defend!=0;
    } else { p.Version=2; p.DailyGrowth=0; }
    for(std::size_t i=0;i<n;++i) { BanditCamp c; unsigned active;
        if(!(in>>c.Key>>c.AreaId>>active>>c.BornDay>>c.LastRaidDay>>c.SuppressedUntil>>c.ClearedDay>>c.Strength>>c.FoodRaided>>c.Raids) || active>1) return bad();
        c.Active=active!=0;
        if(version==2) { if(!(in>>c.FoodStored>>c.FoodConsumed>>c.FoodSpoiled>>c.FoodWasted>>c.FoodAbandoned>>c.LegacyFoodConsumed>>c.LootGrowth>>c.StarvationLosses>>c.LastTargetArea>>c.Outcome)) return bad(); }
        else { c.LegacyFoodConsumed=c.FoodRaided; c.Outcome=c.Active?0:1; }
        if(!b.Camps.emplace(c.Key,c).second) return bad();
    }
    auto& e=b.Engagement;
    if(!(in>>std::quoted(e.EncounterId)>>e.CampId>>e.LeaderId>>e.AreaId>>e.Day>>e.Enemies)) return bad();
    if(version==1) return {true,"accepted",""};
    if(!(in>>n) || n>s.Areas.size()) return bad();
    for(std::size_t i=0;i<n;++i) { Id a; double food; if(!(in>>a>>food) || !b.FoodTakenByArea.emplace(a,food).second) return bad(); }
    if(!(in>>b.NextExpedition>>n) || n>100000) return bad();
    for(std::size_t i=0;i<n;++i) { CampExpedition x; unsigned phase; std::size_t length;
        if(!(in>>x.Key>>x.Camp>>x.Domain>>x.Region>>x.Lord>>x.Family>>x.HomeArea>>phase>>x.Position>>x.Rounds>>x.Mustered>>x.Casualties>>x.StartedDay>>x.NextActionDay>>x.FinishedDay>>x.Readiness>>x.Cost>>x.Food>>length) || phase>3 || length>s.Areas.size()) return bad();
        x.Phase=static_cast<ExpeditionPhase>(phase);
        for(std::size_t j=0;j<length;++j) { Id a; if(!(in>>a)) return bad(); x.Route.push_back(a); }
        if(!b.Expeditions.emplace(x.Key,x).second) return bad();
    }
    return {true,"accepted",""};
}
void ExportBandits(const Snapshot& s,std::ostream& o)
{
    const auto& b=s.Bandits; const auto& p=b.Policy;
    const auto field=[&](const char* name,auto value){o<<",\""<<name<<"\":"<<value;};
    o<<",\"bandits\":{\"version\":"<<p.Version;field("enabled",p.Enabled?"true":"false");field("nextId",b.NextId);
    o<<",\"policy\":{\"spawnDays\":"<<p.SpawnDays;
    field("raidDays",p.RaidDays);field("recoveryDays",p.RecoveryDays);field("raidHops",p.RaidHops);field("spacingHops",p.SpacingHops);field("maxCamps",p.MaxCamps);
    field("initialStrength",p.InitialStrength);field("maximumStrength",p.MaximumStrength);field("dailyGrowth",p.DailyGrowth);field("raidGrowth",p.RaidGrowth);field("raidStockFraction",p.RaidStockFraction);field("foodPerStrength",p.FoodPerStrength);field("maximumDevelopment",p.MaximumDevelopment);
    field("foodUpkeep",p.FoodUpkeep);field("growthFood",p.GrowthFood);field("starvationLoss",p.StarvationLoss);field("storagePerStrength",p.StoragePerStrength);field("ownerDefense",p.OwnerDefense?"true":"false");field("responseDays",p.ResponseDays);field("defenseBattleDays",p.DefenseBattleDays);field("defenseMaxRounds",p.DefenseMaxRounds);field("defenseCooldownDays",p.DefenseCooldownDays);field("defenseStrengthRatio",p.DefenseStrengthRatio);field("defenseWealthReserve",p.DefenseWealthReserve);field("defenseFoodReserveDays",p.DefenseFoodReserveDays);
    o<<'}';field("engagedCamp",b.Engagement.CampId);o<<",\"camps\":[";bool comma=false;
    for(const auto& row:b.Camps) { const auto& c=row.second;if(comma)o<<',';comma=true;o<<"{\"id\":"<<c.Key;
        field("areaId",c.AreaId);field("active",c.Active?"true":"false");field("bornDay",c.BornDay);field("lastRaidDay",c.LastRaidDay);field("suppressedUntil",c.SuppressedUntil);field("clearedDay",c.ClearedDay);field("strength",c.Strength);field("foodRaided",c.FoodRaided);field("raids",c.Raids);
        field("foodStored",c.FoodStored);field("foodConsumed",c.FoodConsumed);field("foodSpoiled",c.FoodSpoiled);field("foodWasted",c.FoodWasted);field("foodAbandoned",c.FoodAbandoned);field("legacyFoodConsumed",c.LegacyFoodConsumed);field("lootGrowth",c.LootGrowth);field("starvationLosses",c.StarvationLosses);field("lastTargetArea",c.LastTargetArea);field("outcome",c.Outcome);o<<'}';
    }
    o<<"],\"foodTakenByArea\":[";comma=false;
    for(const auto& row:b.FoodTakenByArea){if(comma)o<<',';comma=true;o<<"{\"area\":"<<row.first;field("food",row.second);o<<'}';}
    o<<"],\"expeditions\":[";comma=false;
    for(const auto& row:b.Expeditions){const auto& x=row.second;if(comma)o<<',';comma=true;o<<"{\"id\":"<<x.Key;
        field("camp",x.Camp);field("domain",x.Domain);field("region",x.Region);field("lord",x.Lord);field("family",x.Family);field("homeArea",x.HomeArea);field("phase",static_cast<unsigned>(x.Phase));field("position",x.Position);field("rounds",x.Rounds);field("mustered",x.Mustered);field("casualties",x.Casualties);field("startedDay",x.StartedDay);field("nextActionDay",x.NextActionDay);field("finishedDay",x.FinishedDay);field("readiness",x.Readiness);field("cost",x.Cost);field("food",x.Food);
        o<<",\"route\":[";for(std::size_t i=0;i<x.Route.size();++i){if(i)o<<',';o<<x.Route[i];}o<<"]}";
    }o<<"]}";
}
}
