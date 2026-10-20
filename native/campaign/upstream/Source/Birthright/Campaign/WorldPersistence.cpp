#include "World.h"
#include "PlayerProgression.h"
#include "EmpireGeneration.h"
#include "WorldEvolution.h"
#include "WorldBandits.h"
#include "WorldPolitics.h"
#include "WorldMarket.h"
#include "WorldTraversal.h"
#include "MapGeography.h"

#include <iomanip>
#include <istream>
#include <locale>
#include <ostream>
#include <sstream>
#include <set>

namespace Birthright::Campaign
{
namespace
{
std::string JsonString(const std::string& value)
{
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : value)
    {
        if (c == '"' || c == '\\') out << '\\' << static_cast<char>(c);
        else if (c < 32) out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
        else out << static_cast<char>(c);
    }
    out << '"';
    return out.str();
}
bool Count(std::istream& input, std::size_t& count, std::size_t maximum)
{
    std::uint64_t value = 0;
    if (!(input >> value) || value > maximum) return false;
    count = static_cast<std::size_t>(value); return true;
}
template<class T> bool Insert(std::map<Id, T>& records, T&& record)
{
    const auto id = record.Key;
    return records.emplace(id, std::move(record)).second;
}
Result Corrupt() { return { false, "save_invalid", "Truncated, duplicate, malformed or oversized save record." }; }
}

Result Save(const Snapshot& state, std::ostream& output)
{
    const auto valid = Validate(state); if (!valid) return valid;
    output.imbue(std::locale::classic()); output << std::setprecision(17);
    output << "BIRTHRIGHT_WORLD " << state.Version << ' ' << std::quoted(state.WorldId) << ' ' << state.Seed << ' ' << state.Day << '\n';
    const auto& r = state.Policy;
    output << r.Version << ' ' << r.BasePointCost << ' ' << r.PointCostGrowth << ' ' << r.PrimaryInheritance << ' '
        << r.SecondaryInheritance << ' ' << r.OtherInheritance << ' ' << r.ImmediateBirthrightFraction << ' '
        << r.StrifeTransferFraction << ' ' << r.AdultAgeDays << ' ' << r.DailyBirthRate << ' ' << r.DailyDeathRate << ' ' << r.DailyStewardFavor << '\n';
    output << state.Cultures.size() << '\n';
    for (const auto& item : state.Cultures)
        output << item.first << ' ' << std::quoted(item.second.Name) << ' ' << static_cast<unsigned>(item.second.Patron) << '\n';
    output << state.Domains.size() << '\n';
    for (const auto& item : state.Domains) output << item.first << ' ' << std::quoted(item.second.Name) << ' ' << item.second.HeadCharacterId << '\n';
    output << state.Entities.size() << '\n';
    for (const auto& item : state.Entities) output << item.first << ' ' << std::quoted(item.second.Name) << ' ' << item.second.HeadCharacterId << '\n';
    output << state.Regions.size() << '\n';
    for (const auto& item : state.Regions) output << item.first << ' ' << item.second.DomainId << ' ' << item.second.OwnerEntityId << '\n';
    output << state.Areas.size() << '\n';
    for (const auto& item : state.Areas)
    {
        const auto& a = item.second;
        output << a.Key << ' ' << a.RegionId << ' ' << a.X << ' ' << a.Y << ' ' << a.FoodCapacity << ' ' << a.Prosperity << ' ' << a.Unrest << '\n';
        output << a.Neighbors.size(); for (const auto id : a.Neighbors) output << ' ' << id; output << '\n';
        output << a.Cohorts.size() << '\n';
        for (const auto& c : a.Cohorts) output << c.CultureId << ' ' << c.Population << ' ' << c.BirthRemainder << ' ' << c.DeathRemainder << '\n';
    }
    output << state.Characters.size() << '\n';
    for (const auto& item : state.Characters)
    {
        const auto& c = item.second;
        output << c.Key << ' ' << std::quoted(c.Name) << ' ' << c.AreaId << ' ' << c.CultureId << ' ' << c.BornDay << ' ' << c.Alive << ' ' << c.DiedDay << ' ' << c.SpouseId << '\n';
        output << c.Parents.size(); for (const auto parent : c.Parents) output << ' ' << parent; output << '\n';
        for (std::size_t d = 0; d < DeityCount; ++d)
        {
            const auto& a = c.Affinities[d]; const auto& grant = c.Birthright[d];
            output << a.BirthrightPower << ' ' << a.EarnedFavor << ' ' << a.TransferredFavor << ' '
                << grant.SourceParentId << ' ' << grant.FullPower << ' ' << grant.Released << ' ' << grant.GrantedFavor << '\n';
        }
    }
    output << state.Allocations.size() << '\n';
    for (const auto& a : state.Allocations)
        output << a.ParentId << ' ' << static_cast<unsigned>(a.Path) << ' ' << a.FavorBudget << ' ' << a.SplitAuthorized << '\n';
    output << state.Events.size() << '\n';
    for (const auto& e : state.Events)
        output << std::quoted(e.OperationId) << ' ' << std::quoted(e.Kind) << ' ' << e.Day << ' ' << e.ActorId << ' ' << e.OtherId << ' ' << std::quoted(e.Detail) << '\n';
    output << "EMPIRE_METADATA 1\n" << state.Cultures.size() << '\n';
    for (const auto& item : state.Cultures)
    {
        const auto& c = item.second;
        output << item.first << ' ' << static_cast<unsigned>(c.SecondaryPatron) << ' ' << c.PrimaryPatronWeight << ' ' << c.OriginDomainId;
        for (const auto share : c.Occupations.Roles) output << ' ' << share;
        output << '\n';
    }
    output << state.Domains.size() << '\n';
    for (const auto& item : state.Domains)
    {
        const auto& d = item.second;
        output << item.first << ' ' << d.CapitalAreaId << ' ' << static_cast<unsigned>(d.PrimaryPatron) << ' '
            << static_cast<unsigned>(d.SecondaryPatron) << ' ' << d.PrimaryPatronWeight << '\n';
    }
    output << state.Areas.size() << '\n';
    for (const auto& item : state.Areas)
    {
        const auto& a = item.second;
        output << item.first << ' ' << a.Wilderness << ' ' << a.SettlementSuitability << ' ' << a.DiffusionCost << ' ' << a.Claimants.size();
        for (const auto id : a.Claimants) output << ' ' << id;
        output << ' ' << a.Influences.size();
        for (const auto& influence : a.Influences) output << ' ' << influence.DomainId << ' ' << influence.Strength;
        output << '\n';
    }
    const auto& policy = state.Connectivity;
    output << "TRAVERSAL_METADATA 1\n" << policy.Version << ' ' << policy.InfluenceDailyResponse << ' '
        << policy.MigrationDailyFraction << ' ' << policy.SettlementFraction << ' ' << policy.LocalCultureWeight << '\n';
    output << state.Domains.size() << '\n';
    for (const auto& item : state.Domains) output << item.first << ' ' << item.second.InfluenceBudget << '\n';
    output << state.Connections.size() << '\n';
    for (const auto& item : state.Connections)
    {
        const auto& link = item.second;
        output << link.FirstAreaId << ' ' << link.SecondAreaId << ' ' << link.RoadQuality << ' ' << link.DistanceScale << '\n';
    }
    output << state.Areas.size() << '\n';
    for (const auto& item : state.Areas)
    {
        const auto& area = item.second;
        output << item.first << ' ' << area.Development << ' ' << area.PopulationCapacity << ' ' << area.TargetInfluences.size();
        for (const auto& target : area.TargetInfluences) output << ' ' << target.DomainId << ' ' << target.Strength;
        output << '\n' << area.Cohorts.size();
        for (const auto& cohort : area.Cohorts) output << ' ' << cohort.CultureId << ' ' << cohort.MigrationRemainder;
        output << '\n';
    }
    const auto geometryResult = WriteGeometry(state.Geometry, output); if (!geometryResult) return geometryResult;
    output << "AREA_TYPES 1 " << state.Areas.size() << '\n';
    for (const auto& item : state.Areas) output << item.first << ' ' << item.second.IsWater << '\n';
    output << "TRAVEL_METADATA " << state.TravelPolicy.Version << ' ' << state.TravelPolicy.DaysPerEffort << ' ' << state.DayFraction << '\n';
    output << "WARBAND_METADATA 1 " << state.NextTroopId << ' ' << state.Troops.size() << '\n';
    for (const auto& t:state.Troops) output << t.first << ' ' << t.second.LeaderId << ' ' << t.second.CultureId << ' ' << t.second.HomeAreaId << ' ' << static_cast<unsigned>(t.second.Kind) << '\n';
    output << state.RecruitmentUses.size() << '\n';
    for (const auto& u:state.RecruitmentUses) output << u.AreaId << ' ' << u.CultureId << ' ' << u.Count << '\n';
    WriteEvolution(state, output);
    WritePolitics(state, output);
    WriteMarket(state, output);
    WritePlayerProgression(state, output);
    return output ? Result{ true, "accepted", "" } : Result{ false, "save_write_failed", "Output stream rejected the save." };
}

Result Load(std::istream& input, Snapshot& output)
{
    input.imbue(std::locale::classic());
    Snapshot next;
    std::string magic;
    if (!(input >> magic >> next.Version >> std::quoted(next.WorldId) >> next.Seed >> next.Day) || magic != "BIRTHRIGHT_WORLD" || (next.Version < 1 || next.Version > 11)) return Corrupt();
    const auto fileVersion = next.Version;
    auto& r = next.Policy;
    if (!(input >> r.Version >> r.BasePointCost >> r.PointCostGrowth >> r.PrimaryInheritance >> r.SecondaryInheritance >> r.OtherInheritance
        >> r.ImmediateBirthrightFraction >> r.StrifeTransferFraction >> r.AdultAgeDays >> r.DailyBirthRate >> r.DailyDeathRate >> r.DailyStewardFavor)) return Corrupt();
    std::size_t count = 0;
    if (!Count(input, count, 10000)) return Corrupt();
    for (std::size_t i = 0; i < count; ++i)
    {
        Culture c; unsigned deity = 0;
        if (!(input >> c.Key >> std::quoted(c.Name) >> deity)) return Corrupt();
        c.Patron = static_cast<Deity>(deity); if (!Insert(next.Cultures, std::move(c))) return Corrupt();
    }
    if (!Count(input, count, 100000)) return Corrupt();
    for (std::size_t i = 0; i < count; ++i)
    {
        PoliticalDomain d;
        if (!(input >> d.Key >> std::quoted(d.Name) >> d.HeadCharacterId) || !Insert(next.Domains, std::move(d))) return Corrupt();
    }
    if (!Count(input, count, 100000)) return Corrupt();
    for (std::size_t i = 0; i < count; ++i)
    {
        PoliticalEntity e;
        if (!(input >> e.Key >> std::quoted(e.Name) >> e.HeadCharacterId) || !Insert(next.Entities, std::move(e))) return Corrupt();
    }
    if (!Count(input, count, 100000)) return Corrupt();
    for (std::size_t i = 0; i < count; ++i)
    {
        Region region;
        if (!(input >> region.Key >> region.DomainId >> region.OwnerEntityId) || !Insert(next.Regions, std::move(region))) return Corrupt();
    }
    if (!Count(input, count, 100000)) return Corrupt();
    for (std::size_t i = 0; i < count; ++i)
    {
        Area a; std::size_t nested = 0;
        if (!(input >> a.Key >> a.RegionId >> a.X >> a.Y >> a.FoodCapacity >> a.Prosperity >> a.Unrest) || !Count(input, nested, 100000)) return Corrupt();
        for (std::size_t n = 0; n < nested; ++n) { Id id = 0; if (!(input >> id)) return Corrupt(); a.Neighbors.push_back(id); }
        if (!Count(input, nested, 10000)) return Corrupt();
        for (std::size_t n = 0; n < nested; ++n)
        {
            Cohort c;
            if (!(input >> c.CultureId >> c.Population >> c.BirthRemainder >> c.DeathRemainder)) return Corrupt(); a.Cohorts.push_back(c);
        }
        if (!Insert(next.Areas, std::move(a))) return Corrupt();
    }
    if (!Count(input, count, 1000000)) return Corrupt();
    for (std::size_t i = 0; i < count; ++i)
    {
        Character c; std::size_t nested = 0;
        if (!(input >> c.Key >> std::quoted(c.Name) >> c.AreaId >> c.CultureId >> c.BornDay >> c.Alive >> c.DiedDay >> c.SpouseId) || !Count(input, nested, 2)) return Corrupt();
        for (std::size_t n = 0; n < nested; ++n) { Id id = 0; if (!(input >> id)) return Corrupt(); c.Parents.push_back(id); }
        for (std::size_t d = 0; d < DeityCount; ++d)
        {
            auto& a = c.Affinities[d]; auto& grant = c.Birthright[d];
            if (!(input >> a.BirthrightPower >> a.EarnedFavor >> a.TransferredFavor >> grant.SourceParentId >> grant.FullPower >> grant.Released >> grant.GrantedFavor)) return Corrupt();
        }
        if (!Insert(next.Characters, std::move(c))) return Corrupt();
    }
    if (!Count(input, count, 7000000)) return Corrupt();
    for (std::size_t i = 0; i < count; ++i)
    {
        BirthrightAllocation a; unsigned deity = 0;
        if (!(input >> a.ParentId >> deity >> a.FavorBudget >> a.SplitAuthorized)) return Corrupt();
        a.Path = static_cast<Deity>(deity); next.Allocations.push_back(a);
    }
    if (!Count(input, count, 1000000)) return Corrupt();
    for (std::size_t i = 0; i < count; ++i)
    {
        Event e;
        if (!(input >> std::quoted(e.OperationId) >> std::quoted(e.Kind) >> e.Day >> e.ActorId >> e.OtherId >> std::quoted(e.Detail))) return Corrupt();
        next.Events.push_back(e);
    }
    if (fileVersion >= 2)
    {
        unsigned metadataVersion = 0;
        if (!(input >> magic >> metadataVersion) || magic != "EMPIRE_METADATA" || metadataVersion != 1 ||
            !Count(input, count, 10000) || count != next.Cultures.size()) return Corrupt();
        std::set<Id> seen;
        for (std::size_t i = 0; i < count; ++i)
        {
            Id id = 0; unsigned secondary = 0;
            if (!(input >> id >> secondary) || !next.Cultures.count(id) || !seen.insert(id).second) return Corrupt();
            auto& c = next.Cultures.at(id); c.SecondaryPatron = static_cast<Deity>(secondary);
            if (!(input >> c.PrimaryPatronWeight >> c.OriginDomainId)) return Corrupt();
            for (auto& share : c.Occupations.Roles) if (!(input >> share)) return Corrupt();
        }
        if (!Count(input, count, 100000) || count != next.Domains.size()) return Corrupt();
        seen.clear();
        for (std::size_t i = 0; i < count; ++i)
        {
            Id id = 0; unsigned primary = 0, secondary = 0;
            if (!(input >> id) || !next.Domains.count(id) || !seen.insert(id).second) return Corrupt();
            auto& d = next.Domains.at(id);
            if (!(input >> d.CapitalAreaId >> primary >> secondary >> d.PrimaryPatronWeight)) return Corrupt();
            d.PrimaryPatron = static_cast<Deity>(primary); d.SecondaryPatron = static_cast<Deity>(secondary);
        }
        if (!Count(input, count, 100000) || count != next.Areas.size()) return Corrupt();
        seen.clear();
        for (std::size_t i = 0; i < count; ++i)
        {
            Id id = 0; std::size_t nested = 0;
            if (!(input >> id) || !next.Areas.count(id) || !seen.insert(id).second) return Corrupt();
            auto& a = next.Areas.at(id);
            if (!(input >> a.Wilderness >> a.SettlementSuitability >> a.DiffusionCost) || !Count(input, nested, next.Domains.size())) return Corrupt();
            for (std::size_t n = 0; n < nested; ++n) { Id claimant = 0; if (!(input >> claimant)) return Corrupt(); a.Claimants.push_back(claimant); }
            if (!Count(input, nested, next.Domains.size())) return Corrupt();
            for (std::size_t n = 0; n < nested; ++n)
            {
                Area::Influence influence;
                if (!(input >> influence.DomainId >> influence.Strength)) return Corrupt();
                a.Influences.push_back(influence);
            }
        }
    }
    if (fileVersion >= 3)
    {
        unsigned metadataVersion = 0;
        auto& policy = next.Connectivity;
        if (!(input >> magic >> metadataVersion) || magic != "TRAVERSAL_METADATA" || metadataVersion != 1 ||
            !(input >> policy.Version >> policy.InfluenceDailyResponse >> policy.MigrationDailyFraction >>
                policy.SettlementFraction >> policy.LocalCultureWeight) ||
            !Count(input, count, 100000) || count != next.Domains.size()) return Corrupt();
        std::set<Id> seen;
        for (std::size_t n = 0; n < count; ++n)
        {
            Id id = 0;
            if (!(input >> id) || !next.Domains.count(id) || !seen.insert(id).second ||
                !(input >> next.Domains.at(id).InfluenceBudget)) return Corrupt();
        }
        if (!Count(input, count, 1000000)) return Corrupt();
        for (std::size_t n = 0; n < count; ++n)
        {
            TraversalConnection link;
            if (!(input >> link.FirstAreaId >> link.SecondAreaId >> link.RoadQuality >> link.DistanceScale) ||
                !next.Connections.emplace(std::make_pair(link.FirstAreaId, link.SecondAreaId), link).second) return Corrupt();
        }
        if (!Count(input, count, 100000) || count != next.Areas.size()) return Corrupt();
        seen.clear();
        for (std::size_t n = 0; n < count; ++n)
        {
            Id id = 0; std::size_t nested = 0;
            if (!(input >> id) || !next.Areas.count(id) || !seen.insert(id).second) return Corrupt();
            auto& area = next.Areas.at(id);
            if (!(input >> area.Development >> area.PopulationCapacity) || !Count(input, nested, next.Domains.size())) return Corrupt();
            for (std::size_t j = 0; j < nested; ++j)
            {
                Area::Influence target;
                if (!(input >> target.DomainId >> target.Strength)) return Corrupt();
                area.TargetInfluences.push_back(target);
            }
            if (!Count(input, nested, next.Cultures.size()) || nested != area.Cohorts.size()) return Corrupt();
            std::set<Id> cultures;
            for (std::size_t j = 0; j < nested; ++j)
            {
                Id culture = 0; double remainder = 0;
                if (!(input >> culture >> remainder) || !cultures.insert(culture).second) return Corrupt();
                bool found = false;
                for (auto& cohort : area.Cohorts)
                    if (cohort.CultureId == culture) { cohort.MigrationRemainder = remainder; found = true; break; }
                if (!found) return Corrupt();
            }
        }
    }
    else
    {
        // Old checkpoints have no persisted reach recipe. Preserve cached influence until explicitly configured.
        for (auto& item : next.Areas) item.second.TargetInfluences = item.second.Influences;
    }
    if (fileVersion >= 4) { const auto geometryResult = ReadGeometry(input, next.Geometry); if (!geometryResult) return geometryResult; }
    if (fileVersion >= 5)
    {
        unsigned typeVersion = 0;
        if (!(input >> magic >> typeVersion) || magic != "AREA_TYPES" || typeVersion != 1 ||
            !Count(input,count,100000) || count != next.Areas.size()) return Corrupt();
        std::set<Id> seen;
        for (std::size_t n = 0; n < count; ++n)
        {
            Id id = 0; unsigned water = 0;
            if (!(input >> id >> water) || water > 1 || !next.Areas.count(id) || !seen.insert(id).second) return Corrupt();
            next.Areas.at(id).IsWater = water != 0;
        }
    }
    else
    {
        // Earlier polygon saves had land areas only. Do not repair malformed land or existing water records.
        for (const auto& item : next.Geometry.Cells)
            if (item.second.Height < 20 && next.Areas.count(item.first)) return Corrupt();
        MaterializeWaterAreas(next);
    }
    if (fileVersion >= 6)
    {
        if (!(input >> magic) || magic != "TRAVEL_METADATA" ||
            !(input >> next.TravelPolicy.Version >> next.TravelPolicy.DaysPerEffort >> next.DayFraction)) return Corrupt();
    }
    if (fileVersion >= 7) {
        unsigned version=0;
        if (!(input >> magic >> version >> next.NextTroopId) || magic!="WARBAND_METADATA" || version!=1 || !Count(input,count,1000000)) return Corrupt();
        for (std::size_t n=0;n<count;++n) {
            Troop t; unsigned kind=0;
            if (!(input >> t.Key >> t.LeaderId >> t.CultureId >> t.HomeAreaId >> kind)) return Corrupt();
            t.Kind=static_cast<TroopKind>(kind); if (!Insert(next.Troops,std::move(t))) return Corrupt();
        }
        if (!Count(input,count,1000000)) return Corrupt();
        for (std::size_t n=0;n<count;++n) { RecruitmentUse u; if (!(input >> u.AreaId >> u.CultureId >> u.Count)) return Corrupt(); next.RecruitmentUses.push_back(u); }
    }
    if (fileVersion >= 8) { const auto evolution = ReadEvolution(input, next); if (!evolution) return evolution; }
    if (fileVersion >= 9) { const auto politics = ReadPolitics(input, next); if (!politics) return politics; }
    if(fileVersion >= 10) { const auto market=ReadMarket(input,next); if(!market) return market; }
    if (fileVersion >= 11) { const auto players = ReadPlayerProgression(input, next); if (!players) return players; }
    next.Version = 11;
    input >> std::ws; if (!input.eof()) return Corrupt();
    const auto result = Validate(next);
    if (result) output = std::move(next);
    return result;
}

Result ExportJson(const Snapshot& state, std::ostream& output)
{
    const auto valid = Validate(state); if (!valid) return valid;
    output.imbue(std::locale::classic()); output << std::setprecision(17);
    output << "{\n\"schemaVersion\":" << state.Version << ",\"worldId\":" << JsonString(state.WorldId)
        << ",\"seed\":" << state.Seed << ",\"day\":" << state.Day << ",\"dayFraction\":" << state.DayFraction
        << ",\"travelPolicy\":{\"version\":" << state.TravelPolicy.Version << ",\"daysPerEffort\":" << state.TravelPolicy.DaysPerEffort << '}';
    const auto& r = state.Policy;
    output << ",\n\"rules\":{\"version\":" << r.Version << ",\"basePointCost\":" << r.BasePointCost << ",\"pointCostGrowth\":" << r.PointCostGrowth
        << ",\"primaryInheritance\":" << r.PrimaryInheritance << ",\"secondaryInheritance\":" << r.SecondaryInheritance << ",\"otherInheritance\":" << r.OtherInheritance
        << ",\"immediateBirthrightFraction\":" << r.ImmediateBirthrightFraction << ",\"strifeTransferFraction\":" << r.StrifeTransferFraction
        << ",\"adultAgeDays\":" << r.AdultAgeDays << ",\"dailyBirthRate\":" << r.DailyBirthRate << ",\"dailyDeathRate\":" << r.DailyDeathRate << ",\"dailyStewardFavor\":" << r.DailyStewardFavor << '}';
    ExportEvolution(state, output);
    ExportBandits(state, output);
    ExportPolitics(state, output);
    ExportMarket(state, output);
    ExportPlayerProgression(state, output);
    const auto& connectivity = state.Connectivity;
    output << ",\n\"connectivity\":{\"version\":" << connectivity.Version
        << ",\"influenceDailyResponse\":" << connectivity.InfluenceDailyResponse
        << ",\"migrationDailyFraction\":" << connectivity.MigrationDailyFraction
        << ",\"settlementFraction\":" << connectivity.SettlementFraction
        << ",\"localCultureWeight\":" << connectivity.LocalCultureWeight << "},\n\"connections\":[";
    bool linkComma = false;
    for (const auto& item : state.Connections)
    {
        if (linkComma) output << ','; linkComma = true;
        const auto& link = item.second;
        output << "{\"firstAreaId\":" << link.FirstAreaId << ",\"secondAreaId\":" << link.SecondAreaId
            << ",\"roadQuality\":" << link.RoadQuality << ",\"distanceScale\":" << link.DistanceScale
            << ",\"effectiveDifficulty\":" << TraversalCost(state, link.FirstAreaId, link.SecondAreaId)
            << ",\"travelDays\":" << state.TravelPolicy.DaysPerEffort * TraversalCost(state,link.FirstAreaId,link.SecondAreaId) << '}';
    }
    output << "],\n\"geometry\":{\"version\":" << state.Geometry.Version << ",\"name\":" << JsonString(state.Geometry.Name)
        << ",\"sourceVersion\":" << JsonString(state.Geometry.SourceVersion) << ",\"sourceSeed\":" << JsonString(state.Geometry.SourceSeed)
        << ",\"distanceUnit\":" << JsonString(state.Geometry.DistanceUnit) << ",\"distanceScale\":" << state.Geometry.DistanceScale
        << ",\"width\":" << state.Geometry.Width << ",\"height\":" << state.Geometry.Height << ",\"cells\":[";
    bool shapeComma = false;
    for (const auto& item : state.Geometry.Cells)
    {
        if (shapeComma) output << ','; shapeComma = true;
        const auto& cell = item.second;
        output << "{\"id\":" << cell.Key << ",\"center\":[" << cell.Center.X << ',' << cell.Center.Y << "],\"elevation\":" << cell.Height
            << ",\"biome\":" << cell.Biome << ",\"polygon\":[";
        for (std::size_t n = 0; n < cell.Polygon.size(); ++n)
        {
            if (n) output << ',';
            output << '[' << cell.Polygon[n].X << ',' << cell.Polygon[n].Y << ']';
        }
        output << "]}";
    }
    output << "]}";
    const auto hierarchy = InspectHierarchy(state);
    const auto territory = [&](const TerritorySummary& summary) {
        output << ",\"areaIds\":[";
        for (std::size_t n = 0; n < summary.AreaIds.size(); ++n) { if (n) output << ','; output << summary.AreaIds[n]; }
        output << "],\"population\":" << summary.Population << ",\"estimatedRoles\":[";
        for (std::size_t n = 0; n < PopulationRoleCount; ++n) { if (n) output << ','; output << summary.Roles[n]; }
        output << ']';
    };
    bool comma = false;
    const auto separate = [&]() { if (comma) output << ','; comma = true; };
    output << ",\n\"cultures\":[";
    for (const auto& item : state.Cultures)
    {
        separate(); const auto& c = item.second;
        output << "{\"id\":" << item.first << ",\"name\":" << JsonString(c.Name) << ",\"patron\":" << JsonString(DeityName(c.Patron))
            << ",\"secondaryPatron\":" << (c.SecondaryPatron == Deity::Count ? "null" : JsonString(DeityName(c.SecondaryPatron)))
            << ",\"primaryPatronWeight\":" << c.PrimaryPatronWeight << ",\"originDomainId\":" << c.OriginDomainId << ",\"roleShares\":[";
        for (std::size_t n = 0; n < PopulationRoleCount; ++n) { if (n) output << ','; output << c.Occupations.Roles[n]; }
        output << "]}";
    }
    output << "],\n\"domains\":["; comma = false;
    for (const auto& item : state.Domains)
    {
        separate(); const auto& d = item.second;
        output << "{\"id\":" << item.first << ",\"name\":" << JsonString(d.Name) << ",\"headCharacterId\":" << d.HeadCharacterId
            << ",\"capitalAreaId\":" << d.CapitalAreaId << ",\"primaryPatron\":" << (d.PrimaryPatron == Deity::Count ? "null" : JsonString(DeityName(d.PrimaryPatron)))
            << ",\"secondaryPatron\":" << (d.SecondaryPatron == Deity::Count ? "null" : JsonString(DeityName(d.SecondaryPatron)))
            << ",\"primaryPatronWeight\":" << d.PrimaryPatronWeight << ",\"influenceBudget\":" << d.InfluenceBudget;
        const auto& summary = hierarchy.Domains.at(item.first);
        output << ",\"regionIds\":[";
        for (std::size_t n = 0; n < summary.RegionIds.size(); ++n) { if (n) output << ','; output << summary.RegionIds[n]; }
        output << ']'; territory(summary); output << '}';
    }
    output << "],\n\"entities\":["; comma = false;
    for (const auto& item : state.Entities) { separate(); output << "{\"id\":" << item.first << ",\"name\":" << JsonString(item.second.Name) << ",\"headCharacterId\":" << item.second.HeadCharacterId << '}'; }
    output << "],\n\"regions\":["; comma = false;
    for (const auto& item : state.Regions)
    {
        separate(); const auto traversal = InspectRegionTraversal(state, item.first);
        output << "{\"id\":" << item.first << ",\"domainId\":" << item.second.DomainId << ",\"ownerEntityId\":" << item.second.OwnerEntityId
            << ",\"traversal\":{\"areas\":" << traversal.Areas << ",\"internalLinks\":" << traversal.InternalLinks
            << ",\"roadLinks\":" << traversal.RoadLinks << ",\"meanAreaDifficulty\":" << traversal.MeanAreaDifficulty
            << ",\"meanLinkDifficulty\":" << traversal.MeanLinkDifficulty << ",\"maximumLinkDifficulty\":" << traversal.MaximumLinkDifficulty << '}';
        territory(hierarchy.Regions.at(item.first)); output << '}';
    }
    output << "],\n\"areas\":["; comma = false;
    for (const auto& item : state.Areas)
    {
        separate(); const auto& a = item.second;
        output << "{\"id\":" << a.Key << ",\"regionId\":" << a.RegionId << ",\"isWater\":" << (a.IsWater ? "true" : "false") << ",\"geometryCellId\":"
            << (state.Geometry.Cells.empty() ? 0 : a.Key) << ",\"x\":" << a.X << ",\"y\":" << a.Y << ",\"foodCapacity\":" << a.FoodCapacity << ",\"prosperity\":" << a.Prosperity << ",\"unrest\":" << a.Unrest << ",\"neighbors\":[";
        for (std::size_t n = 0; n < a.Neighbors.size(); ++n) { if (n) output << ','; output << a.Neighbors[n]; }
        output << "],\"wilderness\":" << (a.Wilderness ? "true" : "false") << ",\"settlementSuitability\":" << a.SettlementSuitability
            << ",\"diffusionCost\":" << a.DiffusionCost << ",\"development\":" << a.Development
            << ",\"populationCapacity\":" << a.PopulationCapacity << ",\"effectiveAreaDifficulty\":" << EffectiveAreaDifficulty(a.DiffusionCost, a.Development)
            << ",\"claimants\":[";
        for (std::size_t n = 0; n < a.Claimants.size(); ++n) { if (n) output << ','; output << a.Claimants[n]; }
        output << "],\"influences\":[";
        for (std::size_t n = 0; n < a.Influences.size(); ++n)
        {
            if (n) output << ',';
            output << "{\"domainId\":" << a.Influences[n].DomainId << ",\"strength\":" << a.Influences[n].Strength << '}';
        }
        output << "],\"targetInfluences\":[";
        for (std::size_t n = 0; n < a.TargetInfluences.size(); ++n)
        {
            if (n) output << ',';
            output << "{\"domainId\":" << a.TargetInfluences[n].DomainId << ",\"strength\":" << a.TargetInfluences[n].Strength << '}';
        }
        output << "],\"estimatedRoles\":[";
        const auto roles = EstimatePopulationRoles(state, a.Key);
        for (std::size_t n = 0; n < PopulationRoleCount; ++n) { if (n) output << ','; output << roles[n]; }
        output << "],\"cohorts\":[";
        for (std::size_t n = 0; n < a.Cohorts.size(); ++n)
        {
            if (n) output << ','; const auto& c = a.Cohorts[n];
            output << "{\"cultureId\":" << c.CultureId << ",\"population\":" << c.Population << ",\"birthRemainder\":" << c.BirthRemainder << ",\"deathRemainder\":" << c.DeathRemainder << ",\"migrationRemainder\":" << c.MigrationRemainder << '}';
        }
        output << "]}";
    }
    output << "],\n\"warband\":{\"version\":1,\"nextTroopId\":" << state.NextTroopId << ",\"troops\":["; comma=false;
    for (const auto& t:state.Troops) { separate(); output << "{\"id\":" << t.first << ",\"leaderId\":" << t.second.LeaderId << ",\"cultureId\":" << t.second.CultureId << ",\"homeAreaId\":" << t.second.HomeAreaId << ",\"kind\":\"Peasant\"}"; }
    output << "],\"recruitmentUses\":["; comma=false;
    for (const auto& u:state.RecruitmentUses) { separate(); output << "{\"areaId\":" << u.AreaId << ",\"cultureId\":" << u.CultureId << ",\"count\":" << u.Count << '}'; }
    output << "]},\n\"characters\":["; comma=false;
    for (const auto& item : state.Characters)
    {
        separate(); const auto& c = item.second;
        output << "{\"id\":" << c.Key << ",\"name\":" << JsonString(c.Name) << ",\"areaId\":" << c.AreaId << ",\"cultureId\":" << c.CultureId << ",\"bornDay\":" << c.BornDay << ",\"alive\":" << (c.Alive ? "true" : "false") << ",\"diedDay\":" << c.DiedDay << ",\"spouseId\":" << c.SpouseId << ",\"parents\":[";
        for (std::size_t n = 0; n < c.Parents.size(); ++n) { if (n) output << ','; output << c.Parents[n]; }
        output << "],\"affinities\":[";
        for (std::size_t d = 0; d < DeityCount; ++d)
        {
            if (d) output << ','; const auto& a = c.Affinities[d]; const auto& grant = c.Birthright[d];
            output << "{\"deity\":" << JsonString(DeityName(static_cast<Deity>(d))) << ",\"power\":" << Power(a, state.Policy)
                << ",\"birthrightPower\":" << a.BirthrightPower << ",\"earnedFavor\":" << a.EarnedFavor << ",\"transferredFavor\":" << a.TransferredFavor
                << ",\"sourceParentId\":" << grant.SourceParentId << ",\"fullBirthrightPower\":" << grant.FullPower << ",\"grantedBirthrightFavor\":" << grant.GrantedFavor << ",\"released\":" << (grant.Released ? "true" : "false") << '}';
        }
        output << "]}";
    }
    output << "],\n\"birthrightAllocations\":["; comma = false;
    for (const auto& a : state.Allocations)
    {
        separate(); output << "{\"parentId\":" << a.ParentId << ",\"deity\":" << JsonString(DeityName(a.Path))
            << ",\"favorBudget\":" << a.FavorBudget << ",\"splitAuthorized\":" << (a.SplitAuthorized ? "true" : "false") << '}';
    }
    output << "],\n\"events\":["; comma = false;
    for (const auto& e : state.Events)
    {
        separate(); output << "{\"operationId\":" << JsonString(e.OperationId) << ",\"kind\":" << JsonString(e.Kind) << ",\"day\":" << e.Day
            << ",\"actorId\":" << e.ActorId << ",\"otherId\":" << e.OtherId << ",\"detail\":" << JsonString(e.Detail) << '}';
    }
    output << "]\n}\n";
    return output ? Result{ true, "accepted", "" } : Result{ false, "json_write_failed", "Output stream rejected JSON." };
}
}
