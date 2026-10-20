#include "WorldEvolution.h"
#include "WorldMarket.h"
#include "WorldBandits.h"
#include <algorithm>
#include <cmath>
#include <istream>
#include <limits>
#include <ostream>
#include <set>

namespace Birthright::Campaign
{
namespace
{
Result Ok() { return {true, "accepted", ""}; }
Result Fail(const std::string& detail) { return {false, "evolution_invalid", detail}; }
bool Range(double x, double lo, double hi) { return std::isfinite(x) && x >= lo && x <= hi; }
double Population(const Area& area)
{
    double n = 0; for (const auto& c : area.Cohorts) n += static_cast<double>(c.Population); return n;
}
Id DomainAt(const Snapshot& s, Id area)
{
    const auto region = s.Areas.at(area).RegionId;
    return region ? s.Regions.at(region).DomainId : 0;
}
bool Protected(const Snapshot& s, Id id)
{
    const auto& ids = s.Evolution.ProtectedCharacters;
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}
bool Adult(const Snapshot& s, const Character& c) { return c.Alive && s.Day - c.BornDay >= s.Policy.AdultAgeDays; }
// Counter-based draws: adding an unrelated actor or changing report cadence cannot consume another stream.
double Draw(const Snapshot& s, Id actor, std::uint64_t stream)
{
    std::uint64_t x = actor ^ (static_cast<std::uint64_t>(s.Seed) << 32) ^
        (static_cast<std::uint64_t>(s.Day) * 0x9e3779b97f4a7c15ULL) ^ stream;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return static_cast<double>((x ^ (x >> 31)) >> 11) / 9007199254740992.0;
}
std::string Operation(const Snapshot& s, const std::string& kind, Id id)
{
    return "evolution/" + std::to_string(s.Day) + "/" + kind + "/" + std::to_string(id);
}
bool Related(const Snapshot& s, Id a, Id b)
{
    // Shared ancestors, not just direct ancestry; retained genealogy stays authoritative.
    const auto ancestors = [&](Id id) {
        std::set<Id> found; std::vector<Id> pending{id};
        while (!pending.empty()) { const auto v = pending.back(); pending.pop_back();
            if (!found.insert(v).second) continue;
            for (Id p : s.Characters.at(v).Parents) pending.push_back(p); }
        return found;
    };
    const auto left = ancestors(a), right = ancestors(b);
    for (Id id : left) if (right.count(id)) return true;
    return false;
}
}

Result ValidateEvolution(const Snapshot& s)
{
    const auto& e = s.Evolution; const auto& p = e.Policy;
    if (p.Version != 3 || p.DecisionDays < 1 || p.DecisionDays > 365 || p.DemographyDays < 1 || p.DemographyDays > 365 ||
        !p.NamedPerRegion || p.NamedPerRegion > 32 || !p.MaxChildren || p.MaxChildren > 16 ||
        !Range(p.AnnualBirthChance, 0, 1) || (p.Enabled && p.FertilityEndDays < s.Policy.AdultAgeDays) ||
        p.FertilityEndDays > 36500 || p.MortalityStartDays < p.FertilityEndDays ||
        p.MaximumAgeDays <= p.MortalityStartDays || p.MaximumAgeDays > 73000 ||
        !Range(p.AnnualFoodSpoilage, 0, std::nextafter(1.0,0.0)) || !Range(p.FoodReserveDays, 0, 365) || !Range(p.SeasonalAmplitude, 0, 0.5) ||
        !Range(p.TaxPerPerson, 0, 1) || !Range(p.UpkeepPerPerson, 0, 1) || !Range(p.TreasuryPerPerson, 0.01, 100) ||
        !Range(p.ActivityFavor, 0.001, 1000) || !Range(p.InvestmentPerPerson, 0.0001, 1) || !Range(p.CultivationGain, 0.001, 0.1))
        return Fail("Policy v3 requires bounded cadence, ages, demographic rates and economic parameters.");
    if (e.SpoilageStartedDay < 0 || e.SpoilageStartedDay > s.Day || (!p.Enabled && e.SpoilageStartedDay)) return Fail("Invalid spoilage epoch.");
    if (!p.Enabled && (!e.Areas.empty() || !e.Domains.empty() || !e.LastActivity.empty())) return Fail("Disabled policy must have no economic runtime state.");
    if (p.Enabled)
    {
        std::size_t land = 0; for (const auto& a : s.Areas) if (!a.second.IsWater) ++land;
        if (e.Areas.size() != land || e.Domains.size() != s.Domains.size()) return Fail("Enabled policy requires complete area/domain ledgers.");
    }
    for (const auto& row : e.Areas)
    {
        const auto& a = row.second;
        const auto transfer=s.Market.Areas.find(row.first);
        const double imports=transfer==s.Market.Areas.end()?0:transfer->second.FoodImported;
        const auto stolen=s.Bandits.FoodTakenByArea.find(row.first);
        const double exports=(transfer==s.Market.Areas.end()?0:transfer->second.FoodExported)+(stolen==s.Bandits.FoodTakenByArea.end()?0:stolen->second);
        if (!s.Areas.count(row.first) || s.Areas.at(row.first).IsWater || !Range(a.ProductiveCapacity, 0, 1e12) ||
            !Range(a.FoodStored, 0, 365e12) || !Range(a.Produced, 0, 1e22) || !Range(a.Consumed, 0, 1e22) || !Range(a.Wasted, 0, 1e22) || !Range(a.Spoiled, 0, a.Wasted) ||
            std::abs(a.Produced + imports - exports - a.Consumed - a.Wasted - a.FoodStored) > 1e-7 * std::max(1.0, a.Produced+imports))
            return Fail("Area=" + std::to_string(row.first) + "; invalid food ledger or conservation.");
    }
    for (const auto& row : e.Domains)
    {
        const auto& d = row.second;
        if (!s.Domains.count(row.first) || !Range(d.Treasury, 0, 1e14) || !Range(d.Revenue, 0, 1e22) ||
            !Range(d.Spent, 0, 1e22) || !Range(d.Overflow, 0, 1e22) || !Range(d.Legitimacy, 0, 1) ||
            std::abs(d.Revenue - d.Spent - d.Overflow - d.Treasury) > 1e-7 * std::max(1.0, d.Revenue))
            return Fail("Domain=" + std::to_string(row.first) + "; invalid treasury ledger or conservation.");
    }
    for (const auto& row : e.LastActivity)
        if (!s.Characters.count(row.first) || row.second < 0 || row.second > s.Day) return Fail("Invalid activity clock.");
    std::set<Id> seen;
    for (Id id : e.ProtectedCharacters) if (!s.Characters.count(id) || !seen.insert(id).second) return Fail("Invalid protected character.");
    return Ok();
}

Result World::ConfigureEvolution(const std::string& operation, const EvolutionRules& policy)
{
    auto result = CheckOperation(operation); if (!result) return result;
    if (!policy.Enabled || StateData.Evolution.Policy.Enabled) return Fail("Enable once; policy changes require a validated snapshot migration.");
    auto next = BeginMutation(); next.Evolution.Policy = policy; next.Evolution.SpoilageStartedDay = next.Day;
    for (const auto& a : next.Areas) if (!a.second.IsWater)
        next.Evolution.Areas[a.first].ProductiveCapacity = std::min(static_cast<double>(a.second.FoodCapacity), std::max(1.0, Population(a.second) * 1.03));
    for (const auto& d : next.Domains) next.Evolution.Domains.emplace(d.first, DomainEconomy{});
    next.Events.push_back({operation, "evolution_enabled", next.Day, 0, 0, "Version=3; resource accounting and autonomous NPC policies enabled"});
    return Commit(std::move(next));
}

void DecayFoodReserves(Snapshot& s)
{
    if (!s.Evolution.Policy.Enabled || s.Evolution.Policy.AnnualFoodSpoilage == 0) return;
    const double fraction = -std::expm1(std::log1p(-s.Evolution.Policy.AnnualFoodSpoilage) / 365.0);
    for (auto& row : s.Evolution.Areas) {
        auto& a = row.second;
        const double retained = a.FoodStored * (1 - fraction);
        const double lost = a.FoodStored - retained;
        a.FoodStored = retained; a.Spoiled += lost; a.Wasted += lost;
    }
}

std::vector<double> AdvanceEconomy(Snapshot& s)
{
    DecayFoodReserves(s);
    if(s.Market.Policy.Enabled) return AdvanceMarket(s);
    std::vector<double> shortages; shortages.reserve(s.Evolution.Areas.size());
    const auto& p = s.Evolution.Policy;
    std::map<Id, double> population, unrest;
    const double season = 1 + p.SeasonalAmplitude * std::sin(static_cast<double>(s.Day % 365) * 6.283185307179586 / 365);
    for (auto& item : s.Evolution.Areas)
    {
        auto& a = s.Areas.at(item.first); auto& food = item.second;
        const auto n = Population(a);
        const double produced = food.ProductiveCapacity * season;
        const double available = food.FoodStored + produced;
        const double consumed = std::min(n, available);
        shortages.push_back(n > 0 ? 1 - consumed / n : 0);
        const double stored = std::min(std::max(0.0, available - consumed), n * p.FoodReserveDays);
        food.Produced += produced; food.Consumed += consumed;
        food.Wasted += std::max(0.0, available - consumed - stored); food.FoodStored = stored;
        a.FoodCapacity = static_cast<std::int64_t>(std::floor(std::min(1e12, available)));
        const auto d = DomainAt(s, a.Key);
        if (d) { population[d] += n; unrest[d] += n * a.Unrest; }
    }
    for (auto& item : s.Evolution.Domains)
    {
        auto& d = item.second; const auto n = population[item.first];
        const auto disorder = n ? unrest[item.first] / n : 1;
        const double revenue = n * p.TaxPerPerson * (1 - disorder);
        d.Revenue += revenue; d.Treasury += revenue;
        const double due = n * p.UpkeepPerPerson;
        const double paid = std::min(d.Treasury, due); d.Treasury -= paid; d.Spent += paid;
        const double overflow = std::max(0.0, d.Treasury - n * p.TreasuryPerPerson);
        d.Treasury -= overflow; d.Overflow += overflow;
        const double target = std::clamp(0.75 - disorder * 0.5 - (due > 0 ? (1 - paid / due) * 0.3 : 0), 0.0, 1.0);
        d.Legitimacy += (target - d.Legitimacy) * 0.002;
    }
    return shortages;
}

ActivityQuote QuoteActivity(const Snapshot& s, const ActivityIntent& intent)
{
    ActivityQuote q; q.Eligibility = Fail("Activity requires policy v1, a living adult, current area, defined activity and ready cooldown.");
    const auto& p = s.Evolution.Policy;
    const auto found = s.Characters.find(intent.Actor);
    if (!p.Enabled || intent.Version != 1 || found == s.Characters.end() || !Adult(s, found->second) ||
        intent.Area != found->second.AreaId || static_cast<unsigned>(intent.Kind) > 2) return q;
    const auto last = s.Evolution.LastActivity.find(intent.Actor);
    if (last != s.Evolution.LastActivity.end() && s.Day - last->second < p.DecisionDays) return q;
    q.Favor = p.ActivityFavor; q.Path = s.Cultures.at(found->second.CultureId).Patron;
    if (intent.Kind != CampaignActivity::Practice)
    {
        q.PayingDomain = DomainAt(s, intent.Area);
        if (!q.PayingDomain) return q;
        const auto region = s.Areas.at(intent.Area).RegionId;
        const auto entity = s.Regions.at(region).OwnerEntityId;
        if (s.Domains.at(q.PayingDomain).HeadCharacterId != intent.Actor && s.Entities.at(entity).HeadCharacterId != intent.Actor) return q;
        q.Cost = std::max(1.0, Population(s.Areas.at(intent.Area)) * p.InvestmentPerPerson);
        if (s.Evolution.Domains.at(q.PayingDomain).Treasury < q.Cost) return q;
        if (intent.Kind == CampaignActivity::Cultivate && s.Evolution.Areas.at(intent.Area).ProductiveCapacity >= std::max(1.0, Population(s.Areas.at(intent.Area))) * 1.2) return q;
        if (intent.Kind == CampaignActivity::Govern && s.Areas.at(intent.Area).Unrest <= 0.02 && s.Evolution.Domains.at(q.PayingDomain).Legitimacy >= 0.8) return q;
    }
    q.Eligibility = Ok(); return q;
}

std::vector<ActivityIntent> PlanActivities(const Snapshot& s)
{
    std::vector<ActivityIntent> intents;
    if (!s.Evolution.Policy.Enabled) return intents;
    for (const auto& row : s.Characters)
    {
        if (!Adult(s, row.second) || Protected(s, row.first) || s.Players.count(row.first)) continue;
        ActivityIntent intent{1, row.first, row.second.AreaId, CampaignActivity::Practice};
        const auto& a = s.Areas.at(intent.Area);
        // Prioritize deficits, then govern; all proposals still require a fresh authority quote.
        intent.Kind = s.Evolution.Areas.at(a.Key).FoodStored < Population(a) * 10 ? CampaignActivity::Cultivate : CampaignActivity::Govern;
        if (!QuoteActivity(s, intent).Eligibility) intent.Kind = CampaignActivity::Practice;
        if (QuoteActivity(s, intent).Eligibility) intents.push_back(intent);
    }
    return intents;
}

Result World::PerformActivity(const std::string& operation, const ActivityIntent& intent)
{
    auto result = CheckOperation(operation); if (!result) return result;
    const auto quote = QuoteActivity(StateData, intent); if (!quote.Eligibility) return quote.Eligibility;
    auto next = BeginMutation(); const auto& p = next.Evolution.Policy;
    const char* kind = "practice";
    if (quote.PayingDomain)
    {
        auto& d = next.Evolution.Domains.at(quote.PayingDomain); d.Treasury -= quote.Cost; d.Spent += quote.Cost;
        CreditMarketServices(next,quote.PayingDomain,quote.Cost);
        if (intent.Kind == CampaignActivity::Cultivate)
        {
            kind = "cultivate"; auto& food = next.Evolution.Areas.at(intent.Area);
            food.ProductiveCapacity = std::min(1e12, food.ProductiveCapacity + std::max(1.0, Population(next.Areas.at(intent.Area))) * p.CultivationGain);
        }
        else { kind = "govern"; d.Legitimacy = std::min(1.0, d.Legitimacy + 0.005); next.Areas.at(intent.Area).Unrest *= 0.9; }
    }
    auto& affinity = next.Characters.at(intent.Actor).Affinities[static_cast<std::size_t>(quote.Path)];
    const auto room = FavorCost(1000000, next.Policy) - FavorCost(affinity.BirthrightPower, next.Policy) - affinity.EarnedFavor - affinity.TransferredFavor;
    const auto reward = std::max(0.0, std::min(room, quote.Favor)); affinity.EarnedFavor += reward;
    next.Evolution.LastActivity[intent.Actor] = next.Day;
    next.Events.push_back({operation, kind, next.Day, intent.Actor, 0,
        "area=" + std::to_string(intent.Area) + "; domain=" + std::to_string(quote.PayingDomain) + "; cost=" + std::to_string(quote.Cost) + "; favor=" + std::to_string(reward)});
    return Commit(std::move(next));
}

Result World::PromoteCandidate(const std::string& operation, Id character, Id area, Id culture)
{
    auto result = CheckOperation(operation); if (!result) return result;
    const auto found = StateData.Areas.find(area);
    if (!StateData.Evolution.Policy.Enabled || !character || StateData.Characters.count(character) || found == StateData.Areas.end() || found->second.IsWater)
        return Fail("Promotion requires a new character ID and an inhabited land area.");
    auto cohort = std::find_if(found->second.Cohorts.begin(), found->second.Cohorts.end(), [culture](const Cohort& c) { return c.CultureId == culture && c.Population > 0; });
    if (cohort == found->second.Cohorts.end()) return Fail("Promotion requires an available member of the specified cohort.");
    auto next = BeginMutation();
    Character c; c.Key = character; c.Name = "Appointee " + std::to_string(character);
    c.AreaId = area; c.CultureId = culture; c.BornDay = next.Day - next.Policy.AdultAgeDays;
    next.Characters.emplace(character, c);
    for (auto& row : next.Areas.at(area).Cohorts) if (row.CultureId == culture) { --row.Population; break; }
    next.Events.push_back({operation, "promotion", next.Day, character, 0, "area=" + std::to_string(area) + "; one cohort member became a named candidate"});
    return Commit(std::move(next));
}

Result World::AppointOffice(const std::string& operation, OfficeKind kind, Id office, Id candidate)
{
    auto result = CheckOperation(operation); if (!result) return result;
    const auto& s = StateData;
    if ((kind != OfficeKind::Domain && kind != OfficeKind::Entity) || !s.Characters.count(candidate) || !Adult(s, s.Characters.at(candidate))) return Fail("Appointment requires a defined office kind and living adult candidate.");
    const bool domain = kind == OfficeKind::Domain;
    if (domain ? (!s.Domains.count(office) || s.Domains.at(office).HeadCharacterId) : (!s.Entities.count(office) || s.Entities.at(office).HeadCharacterId)) return Fail("Appointment requires an existing vacant office.");
    if (domain) { for (const auto& d : s.Domains) if (d.second.HeadCharacterId == candidate) return Fail("Candidate already holds a domain office."); }
    else { for (const auto& e : s.Entities) if (e.second.HeadCharacterId == candidate) return Fail("Candidate already holds a regional office."); }
    Id area = 0;
    for (const auto& a : s.Areas) if (a.second.RegionId) {
        const auto& r = s.Regions.at(a.second.RegionId);
        if ((domain ? r.DomainId : r.OwnerEntityId) == office && Population(a.second) > 0) { area = a.first; break; }
    }
    if (!area || DomainAt(s, s.Characters.at(candidate).AreaId) != DomainAt(s, area)) return Fail("Candidate must belong to the office's domain and its territory must be inhabited.");
    auto next = BeginMutation();
    if (domain) next.Domains.at(office).HeadCharacterId = candidate; else next.Entities.at(office).HeadCharacterId = candidate;
    next.Characters.at(candidate).AreaId = area;
    next.Events.push_back({operation, "succession", next.Day, candidate, 0, std::string(domain ? "Domain" : "Regional") + "; office=" + std::to_string(office) + "; area=" + std::to_string(area)});
    return Commit(std::move(next));
}

Result AdvanceEvolution(World& world)
{
    auto& s = world.StateData;
    const auto p = s.Evolution.Policy;
    if (s.Day % p.DemographyDays == 0)
    {
        std::map<Id, Id> formerDomains, formerEntities;
        for (const auto& d : s.Domains) formerDomains[d.first] = d.second.HeadCharacterId;
        for (const auto& e : s.Entities) formerEntities[e.first] = e.second.HeadCharacterId;
        std::vector<Id> living;
        for (const auto& c : s.Characters) if (c.second.Alive && !Protected(s, c.first)) living.push_back(c.first);
        for (Id id : living)
        {
            const auto age = s.Day - s.Characters.at(id).BornDay;
            const double hazard = age < p.MortalityStartDays ? 0 :
                1 - std::exp(-0.04 * static_cast<double>(age - p.MortalityStartDays) / 365 * p.DemographyDays / 365);
            if (age >= p.MaximumAgeDays || Draw(s, id, 1) < hazard)
            {
                auto result = world.RecordDeath(Operation(s, "death", id), id, "Age mortality"); if (!result) return result;
            }
        }
        std::size_t count = 0; for (const auto& c : s.Characters) if (c.second.Alive) ++count;
        const std::size_t capacity = std::max<std::size_t>(8, s.Regions.size() * p.NamedPerRegion);
        for (Id id : living)
        {
            const auto c = s.Characters.at(id);
            if (!Adult(s, c) || c.SpouseId || s.Day - c.BornDay > p.FertilityEndDays) continue;
            for (Id partner : living)
            {
                const auto& other = s.Characters.at(partner);
                if (partner <= id || !Adult(s, other) || other.SpouseId || other.CultureId != c.CultureId ||
                    s.Day - other.BornDay > p.FertilityEndDays || Related(s, id, partner)) continue;
                auto result = world.Marry(Operation(s, "marry", id), id, partner); if (!result) return result;
                break;
            }
        }
        for (Id id : living)
        {
            const auto c = s.Characters.at(id);
            if (!Adult(s, c) || c.SpouseId <= id || Protected(s, c.SpouseId) || count >= capacity ||
                s.Day - c.BornDay > p.FertilityEndDays || s.Day - s.Characters.at(c.SpouseId).BornDay > p.FertilityEndDays) continue;
            const auto children = world.ChildrenOf(id);
            if (children.size() >= p.MaxChildren || world.ChildrenOf(c.SpouseId).size() >= p.MaxChildren) continue;
            bool recent = false; for (Id child : children) if (s.Day - s.Characters.at(child).BornDay < 730) recent = true;
            const double chance = 1 - std::pow(1 - p.AnnualBirthChance, static_cast<double>(p.DemographyDays) / 365);
            if (recent || Draw(s, id, 2) >= chance || s.Characters.size() >= 999999) continue;
            const auto next = s.Characters.rbegin()->first + 1;
            const auto& culture = s.Cultures.at(c.CultureId);
            const auto primary = culture.Patron;
            const auto secondary = culture.SecondaryPatron == Deity::Count ? static_cast<Deity>((static_cast<unsigned>(primary) + 1) % DeityCount) : culture.SecondaryPatron;
            auto result = world.Birth(Operation(s, "birth", id), next, "Descendant " + std::to_string(next), id, c.SpouseId, primary, secondary);
            if (!result) return result; ++count;
        }
        // Offices are stable institutions. Vacancies prefer an adult child of the last incumbent,
        // then a local adult; the cohort fallback prevents an extinct named pool from deleting a society.
        const auto fill = [&](Id office, bool domain) -> Result {
            const Id head = domain ? s.Domains.at(office).HeadCharacterId : s.Entities.at(office).HeadCharacterId;
            if (head) return Ok();
            Id area = 0;
            for (const auto& a : s.Areas) if (a.second.RegionId)
            {
                const auto& r = s.Regions.at(a.second.RegionId);
                if ((domain ? r.DomainId : r.OwnerEntityId) == office && Population(a.second) > 0) { area = a.first; break; }
            }
            if (!area) return Ok();
            Id chosen = 0;
            const auto former = domain ? formerDomains.at(office) : formerEntities.at(office);
            bool inherited = false;
            for (const auto& c : s.Characters)
            {
                if (!Adult(s, c.second) || Protected(s, c.first) || DomainAt(s, c.second.AreaId) != DomainAt(s, area)) continue;
                bool occupied = false;
                if (domain) { for (const auto& d : s.Domains) if (d.second.HeadCharacterId == c.first) occupied = true; }
                else { for (const auto& e : s.Entities) if (e.second.HeadCharacterId == c.first) occupied = true; }
                const bool heir = former && std::find(c.second.Parents.begin(), c.second.Parents.end(), former) != c.second.Parents.end();
                if (!occupied && (!chosen || (heir && !inherited))) { chosen = c.first; inherited = heir; }
            }
            if (!chosen && count < capacity && s.Characters.size() < 999999)
            {
                for (const auto cohort : s.Areas.at(area).Cohorts) if (cohort.Population > 0)
                {
                    chosen = s.Characters.rbegin()->first + 1;
                    auto result = world.PromoteCandidate(Operation(s, "promote", chosen), chosen, area, cohort.CultureId);
                    if (!result) return result; ++count; break;
                }
            }
            if (chosen)
            {
                auto result = world.AppointOffice(Operation(s, domain ? "domain-succession" : "entity-succession", office), domain ? OfficeKind::Domain : OfficeKind::Entity, office, chosen);
                if (!result) return result;
                s.Events.back().OtherId = former;
                s.Events.back().Detail += std::string("; method=") + (inherited ? "adult-heir" : "local-appointment");
            }
            return Ok();
        };
        std::vector<Id> domains, entities;
        for (const auto& d : s.Domains) domains.push_back(d.first);
        for (const auto& e : s.Entities) entities.push_back(e.first);
        for (Id id : domains) { auto result = fill(id, true); if (!result) return result; }
        for (Id id : entities) { auto result = fill(id, false); if (!result) return result; }
    }
    if (s.Day % p.DecisionDays == 0)
    {
        const auto intents = PlanActivities(s);
        for (auto intent : intents)
        {
            // Earlier actors may spend a shared treasury; fallback is replanned against current authority.
            if (!QuoteActivity(s, intent).Eligibility) intent.Kind = CampaignActivity::Practice;
            auto result = world.PerformActivity(Operation(s, "activity", intent.Actor), intent);
            if (!result) return result;
        }
    }
    return Ok();
}

void WriteEvolution(const Snapshot& s, std::ostream& out)
{
    const auto& e = s.Evolution; const auto& p = e.Policy;
    out << "EVOLUTION_METADATA " << p.Version << ' ' << p.Enabled << ' ' << p.DecisionDays << ' ' << p.DemographyDays << ' '
        << p.NamedPerRegion << ' ' << p.MaxChildren << ' ' << p.AnnualBirthChance << ' ' << p.FertilityEndDays << ' '
        << p.MortalityStartDays << ' ' << p.MaximumAgeDays << ' ' << p.FoodReserveDays << ' ' << p.SeasonalAmplitude << ' '
        << p.TaxPerPerson << ' ' << p.UpkeepPerPerson << ' ' << p.TreasuryPerPerson << ' ' << p.ActivityFavor << ' '
        << p.InvestmentPerPerson << ' ' << p.CultivationGain << '\n' << e.Areas.size() << '\n';
    for (const auto& a : e.Areas) out << a.first << ' ' << a.second.ProductiveCapacity << ' ' << a.second.FoodStored << ' ' << a.second.Produced << ' ' << a.second.Consumed << ' ' << a.second.Wasted << '\n';
    out << e.Domains.size() << '\n';
    for (const auto& d : e.Domains) out << d.first << ' ' << d.second.Treasury << ' ' << d.second.Revenue << ' ' << d.second.Spent << ' ' << d.second.Overflow << ' ' << d.second.Legitimacy << '\n';
    out << e.LastActivity.size() << '\n'; for (const auto& a : e.LastActivity) out << a.first << ' ' << a.second << '\n';
    out << e.ProtectedCharacters.size() << '\n'; for (Id id : e.ProtectedCharacters) out << id << '\n';
    out << "FOOD_DECAY " << p.AnnualFoodSpoilage << ' ' << e.SpoilageStartedDay << ' ' << e.Areas.size() << '\n';
    for (const auto& a : e.Areas) out << a.first << ' ' << a.second.Spoiled << '\n';
    WriteBandits(s,out);
}
Result ReadEvolution(std::istream& in, Snapshot& s)
{
    auto& e = s.Evolution; auto& p = e.Policy; std::string magic; unsigned enabled = 0;
    if (!(in >> magic >> p.Version >> enabled >> p.DecisionDays >> p.DemographyDays >> p.NamedPerRegion >> p.MaxChildren >>
        p.AnnualBirthChance >> p.FertilityEndDays >> p.MortalityStartDays >> p.MaximumAgeDays >> p.FoodReserveDays >> p.SeasonalAmplitude >>
        p.TaxPerPerson >> p.UpkeepPerPerson >> p.TreasuryPerPerson >> p.ActivityFavor >> p.InvestmentPerPerson >> p.CultivationGain) || magic != "EVOLUTION_METADATA" || enabled > 1) return Fail("Malformed evolution header.");
    if (p.Version < 1 || p.Version > 3) return Fail("Unsupported evolution version.");
    const auto version = p.Version; p.Version = 3;
    p.Enabled = enabled != 0;
    e.SpoilageStartedDay = p.Enabled ? s.Day : 0;
    std::size_t n = 0;
    if (!(in >> n) || n > s.Areas.size()) return Fail("Invalid food ledger count.");
    for (std::size_t i = 0; i < n; ++i) { Id id; AreaEconomy a; if (!(in >> id >> a.ProductiveCapacity >> a.FoodStored >> a.Produced >> a.Consumed >> a.Wasted) || !e.Areas.emplace(id, a).second) return Fail("Invalid food ledger."); }
    if (!(in >> n) || n > s.Domains.size()) return Fail("Invalid treasury ledger count.");
    for (std::size_t i = 0; i < n; ++i) { Id id; DomainEconomy d; if (!(in >> id >> d.Treasury >> d.Revenue >> d.Spent >> d.Overflow >> d.Legitimacy) || !e.Domains.emplace(id, d).second) return Fail("Invalid treasury ledger."); }
    if (!(in >> n) || n > s.Characters.size()) return Fail("Invalid activity count.");
    for (std::size_t i = 0; i < n; ++i) { Id id; std::int64_t day; if (!(in >> id >> day) || !e.LastActivity.emplace(id, day).second) return Fail("Invalid activity ledger."); }
    if (!(in >> n) || n > s.Characters.size()) return Fail("Invalid protected count.");
    for (std::size_t i = 0; i < n; ++i) { Id id; if (!(in >> id)) return Fail("Invalid protected ID."); e.ProtectedCharacters.push_back(id); }
    if (version >= 2) {
        if (!(in >> magic >> p.AnnualFoodSpoilage >> e.SpoilageStartedDay >> n) || magic != "FOOD_DECAY" || n != e.Areas.size()) return Fail("Invalid food decay header.");
        std::set<Id> ids;
        for (std::size_t i=0; i<n; ++i) { Id id; double spoiled;
            if (!(in >> id >> spoiled) || !e.Areas.count(id) || !ids.insert(id).second) return Fail("Invalid food decay ledger.");
            e.Areas.at(id).Spoiled = spoiled;
        }
    }
    if (version >= 3) return ReadBandits(in,s);
    return Ok();
}
void ExportEvolution(const Snapshot& s, std::ostream& out)
{
    const auto& e = s.Evolution;
    const auto& p = e.Policy;
    out << ",\n\"evolution\":{\"version\":" << p.Version << ",\"enabled\":" << (p.Enabled ? "true" : "false")
        << ",\"policy\":{\"decisionDays\":" << p.DecisionDays << ",\"demographyDays\":" << p.DemographyDays
        << ",\"namedPerRegion\":" << p.NamedPerRegion << ",\"maxChildren\":" << p.MaxChildren << ",\"annualBirthChance\":" << p.AnnualBirthChance
        << ",\"fertilityEndDays\":" << p.FertilityEndDays << ",\"mortalityStartDays\":" << p.MortalityStartDays << ",\"maximumAgeDays\":" << p.MaximumAgeDays
        << ",\"annualFoodSpoilage\":" << p.AnnualFoodSpoilage << ",\"spoilageStartedDay\":" << e.SpoilageStartedDay
        << ",\"foodReserveDays\":" << p.FoodReserveDays << ",\"seasonalAmplitude\":" << p.SeasonalAmplitude
        << ",\"taxPerPerson\":" << p.TaxPerPerson << ",\"upkeepPerPerson\":" << p.UpkeepPerPerson << ",\"treasuryPerPerson\":" << p.TreasuryPerPerson
        << ",\"activityFavor\":" << p.ActivityFavor << ",\"investmentPerPerson\":" << p.InvestmentPerPerson << ",\"cultivationGain\":" << p.CultivationGain << "},\"areas\":[";
    bool comma = false;
    for (const auto& a : e.Areas) { if (comma) out << ','; comma = true;
        out << "{\"id\":" << a.first << ",\"productiveCapacity\":" << a.second.ProductiveCapacity << ",\"foodStored\":" << a.second.FoodStored << ",\"produced\":" << a.second.Produced << ",\"consumed\":" << a.second.Consumed << ",\"wasted\":" << a.second.Wasted << ",\"spoiled\":" << a.second.Spoiled << ",\"capacityOverflow\":" << a.second.Wasted-a.second.Spoiled << '}'; }
    out << "],\"domains\":["; comma = false;
    for (const auto& d : e.Domains) { if (comma) out << ','; comma = true;
        out << "{\"id\":" << d.first << ",\"treasury\":" << d.second.Treasury << ",\"revenue\":" << d.second.Revenue << ",\"spent\":" << d.second.Spent << ",\"overflow\":" << d.second.Overflow << ",\"legitimacy\":" << d.second.Legitimacy << '}'; }
    out << "],\"lastActivity\":["; comma = false;
    for (const auto& a : e.LastActivity) { if (comma) out << ','; comma = true; out << "{\"characterId\":" << a.first << ",\"day\":" << a.second << '}'; }
    out << "],\"protectedCharacters\":["; comma = false;
    for (Id id : e.ProtectedCharacters) { if (comma) out << ','; comma = true; out << id; }
    out << "]}";
}
}
