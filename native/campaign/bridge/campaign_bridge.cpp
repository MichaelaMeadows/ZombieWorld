#include "campaign_bridge.h"
#include "MapGeography.h"
#include "PlayerProgression.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <algorithm>
#include <charconv>
#include <limits>
#include <sstream>

using namespace godot;
namespace C = Birthright::Campaign;
namespace {
String str(const std::string &s) { return String::utf8(s.c_str()); }
String id(C::Id value) { return str(std::to_string(value)); }
bool parse_id(const String &input, C::Id &out) {
    const auto chars = input.utf8();
    const std::string text(chars.get_data());
    if (text.empty() || text.size() > 20 || (text.size() > 1 && text[0] == '0')) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}
Dictionary result(const C::Result &r) {
    Dictionary d; d["accepted"] = r.Accepted; d["code"] = str(r.Code); d["detail"] = str(r.Detail); return d;
}
Dictionary bad(const char *code) { return result({false, code, ""}); }
Array ids(const std::vector<C::Id> &values) { Array a; for (auto v : values) a.append(id(v)); return a; }
int64_t population(const C::Area &area) { int64_t total = 0; for (const auto &c : area.Cohorts) total += c.Population; return total; }
}

void CampaignBridge::_bind_methods() {
    ClassDB::bind_method(D_METHOD("create_world","geography","seed","origin"), &CampaignBridge::create_world);
    ClassDB::bind_method(D_METHOD("advance_days","days"), &CampaignBridge::advance_days);
    ClassDB::bind_method(D_METHOD("summary"), &CampaignBridge::summary);
    ClassDB::bind_method(D_METHOD("geometry"), &CampaignBridge::geometry);
    ClassDB::bind_method(D_METHOD("areas"), &CampaignBridge::areas);
    ClassDB::bind_method(D_METHOD("inspect_area","id"), &CampaignBridge::inspect_area);
    ClassDB::bind_method(D_METHOD("people"), &CampaignBridge::people);
    ClassDB::bind_method(D_METHOD("inspect_person","id"), &CampaignBridge::inspect_person);
    ClassDB::bind_method(D_METHOD("realms"), &CampaignBridge::realms);
    ClassDB::bind_method(D_METHOD("item_catalog"), &CampaignBridge::item_catalog);
    ClassDB::bind_method(D_METHOD("inventory"), &CampaignBridge::inventory);
    ClassDB::bind_method(D_METHOD("warband"), &CampaignBridge::warband);
    ClassDB::bind_method(D_METHOD("recruit","culture_id","count"), &CampaignBridge::recruit);
    ClassDB::bind_method(D_METHOD("dismiss_troop","troop_id"), &CampaignBridge::dismiss_troop);
    ClassDB::bind_method(D_METHOD("equip_item","slot","id"), &CampaignBridge::equip_item);
    ClassDB::bind_method(D_METHOD("recent_events","limit"), &CampaignBridge::recent_events);
    ClassDB::bind_method(D_METHOD("snapshot_text"), &CampaignBridge::snapshot_text);
    ClassDB::bind_method(D_METHOD("restore_snapshot","text","player_id"), &CampaignBridge::restore_snapshot);
}
std::string CampaignBridge::operation(const char *kind) const {
    return "godot/" + std::string(kind) + "/" + std::to_string(world.State().Events.size());
}

Dictionary CampaignBridge::create_world(const String &geography, int64_t seed, const String &origin) {
    if (seed < 0 || seed > std::numeric_limits<uint32_t>::max()) return bad("seed_invalid");
    const std::string origin_key(origin.utf8().get_data());
    if (!C::FindOrigin(origin_key)) return bad("origin_invalid");
    const auto file = FileAccess::open(geography, FileAccess::READ);
    if (file.is_null()) return bad("geography_missing");
    if (file->get_length() > 64 * 1024 * 1024) return bad("geography_too_large");
    std::istringstream input(file->get_as_text().utf8().get_data());
    C::GeographyDocument document;
    auto checked = C::LoadGeography(input, document); if (!checked) return result(checked);
    C::EmpireGeneration options;
    options.Seed = static_cast<uint32_t>(seed); options.ClaimRadius = 20; options.InfluenceRadius = 30;
    options.WorldId = "duvaleia-" + std::to_string(seed);
    C::Snapshot state;
    checked = C::GenerateImportedWorld(document, options, {}, state); if (!checked) return result(checked);
    if (state.Domains.empty() || state.Characters.empty()) return bad("world_empty");
    C::Id player_id = state.Characters.rbegin()->first + 1;
    if (!player_id) return bad("character_ids_exhausted");
    const auto area_id = state.Domains.begin()->second.CapitalAreaId;
    const auto &area = state.Areas.at(area_id);
    if (area.Cohorts.empty()) return bad("capital_unpopulated");
    C::Character hero;
    hero.Key = player_id; hero.Name = "The Wanderer"; hero.AreaId = area_id;
    hero.CultureId = area.Cohorts.front().CultureId; hero.BornDay = -state.Policy.AdultAgeDays;
    state.Characters.emplace(player_id, hero);
    state.Evolution.ProtectedCharacters.push_back(player_id);
    C::World candidate;
    checked = candidate.Restore(state); if (!checked) return result(checked);
    C::EvolutionRules evolution; evolution.Enabled = true;
    checked = candidate.ConfigureEvolution("godot/setup/evolution", evolution); if (!checked) return result(checked);
    C::PoliticsRules politics; politics.Enabled = true;
    checked = candidate.ConfigurePolitics("godot/setup/politics", politics); if (!checked) return result(checked);
    C::MarketRules market; market.Enabled = true;
    checked = candidate.ConfigureMarket("godot/setup/market", market); if (!checked) return result(checked);
    C::BanditRules bandits; bandits.Enabled = true;
    checked = candidate.ConfigureBandits("godot/setup/bandits", bandits); if (!checked) return result(checked);
    checked = candidate.ChooseOrigin("godot/setup/origin", player_id, origin_key); if (!checked) return result(checked);
    world = std::move(candidate); player = player_id; initialized = true;
    return result({true,"world_created",""});
}
Dictionary CampaignBridge::advance_days(int64_t days) {
    if (!initialized) return bad("world_not_initialized");
    // Bound interactive commands. Long runs belong in the portable observation harness.
    if (days < 1 || days > 365) return bad("days_out_of_range");
    return result(world.AdvanceDays(static_cast<uint32_t>(days)));
}
Dictionary CampaignBridge::summary() const {
    Dictionary d; d["initialized"] = initialized;
    if (!initialized) return d;
    const auto &s = world.State(); const auto m = world.Inspect();
    d["world_id"] = str(s.WorldId); d["player_id"] = id(player); d["day"] = s.Day;
    d["population"] = m.Population; d["areas"] = int64_t(m.Areas); d["land_areas"] = int64_t(m.LandAreas);
    d["water_areas"] = int64_t(m.WaterAreas); d["named_people"] = int64_t(m.NamedCharacters);
    d["mean_unrest"] = m.MeanUnrest; d["events"] = int64_t(s.Events.size());
    d["seed"] = int64_t(s.Seed);
    double food = 0, treasury = 0, family_cash = 0, households = 0, producers = 0;
    int64_t living = 0, wars = 0, camps = 0, realms_count = 0;
    for (const auto &v : s.Evolution.Areas) food += v.second.FoodStored;
    for (const auto &v : s.Evolution.Domains) treasury += v.second.Treasury;
    for (const auto &v : s.Politics.Families) family_cash += v.second.Wealth;
    for (const auto &v : s.Market.Regions) { households += v.second.HouseholdCash; producers += v.second.ProducerCash; }
    for (const auto &v : s.Characters) living += v.second.Alive ? 1 : 0;
    for (const auto &v : s.Politics.Wars) wars += v.second.Active ? 1 : 0;
    for (const auto &v : s.Bandits.Camps) { camps += v.second.Active ? 1 : 0; food += v.second.FoodStored; }
    for (const auto &v : s.Domains) realms_count += s.Politics.RetiredDomains.count(v.first) ? 0 : 1;
    d["food_stored"] = food; d["treasury"] = treasury; d["family_cash"] = family_cash;
    d["household_cash"] = households; d["producer_cash"] = producers;
    d["money_total"] = treasury + family_cash + households + producers;
    d["initial_money"] = s.Market.InitialMoney;
    d["living_people"] = living; d["realms"] = realms_count; d["active_wars"] = wars; d["active_camps"] = camps;
    d["food_traded"] = s.Market.FoodTraded; d["wages"] = s.Market.Wages; d["taxes"] = s.Market.Taxes;
    d["evolution_enabled"] = s.Evolution.Policy.Enabled; d["economy_enabled"] = s.Market.Policy.Enabled;
    d["politics_enabled"] = s.Politics.Policy.Enabled; d["bandits_enabled"] = s.Bandits.Policy.Enabled;
    return d;
}
Dictionary CampaignBridge::geometry() const {
    Dictionary d; if (!initialized) return d;
    const auto &g = world.State().Geometry; d["name"] = str(g.Name);
    d["size"] = Vector2(g.Width, g.Height); Array cells;
    for (const auto &entry : g.Cells) {
        const auto &c = entry.second; Dictionary cell;
        cell["id"] = id(c.Key); cell["center"] = Vector2(c.Center.X, c.Center.Y);
        cell["height"] = int64_t(c.Height); cell["biome"] = int64_t(c.Biome);
        PackedVector2Array polygon; for (const auto &p : c.Polygon) polygon.push_back(Vector2(p.X,p.Y));
        cell["polygon"] = polygon; cells.append(cell);
    }
    d["cells"] = cells; return d;
}
Array CampaignBridge::areas() const {
    Array out; if (!initialized) return out;
    const auto &s = world.State();
    for (const auto &entry : s.Areas) {
        const auto &a = entry.second; Dictionary d;
        d["id"] = id(a.Key); d["water"] = a.IsWater; d["region_id"] = id(a.RegionId);
        auto region = s.Regions.find(a.RegionId);
        d["realm_id"] = id(region == s.Regions.end() ? 0 : region->second.DomainId);
        d["population"] = population(a); d["prosperity"] = a.Prosperity; d["unrest"] = a.Unrest;
        out.append(d);
    } return out;
}
Dictionary CampaignBridge::inspect_area(const String &value) const {
    C::Id key = 0; if (!initialized || !parse_id(value,key)) return {};
    const auto &s = world.State(); const auto found = s.Areas.find(key);
    if (found == s.Areas.end()) return {};
    const auto &a = found->second; Dictionary d;
    d["id"] = id(key); d["water"] = a.IsWater; d["population"] = population(a);
    d["region_id"] = id(a.RegionId); d["prosperity"] = a.Prosperity; d["unrest"] = a.Unrest;
    d["development"] = a.Development; d["terrain_effort"] = a.DiffusionCost;
    d["neighbors"] = ids(a.Neighbors); d["food_capacity"] = a.FoodCapacity;
    d["realm_name"] = "Unclaimed"; d["owner_name"] = "No regional owner";
    const auto region = s.Regions.find(a.RegionId);
    if (region != s.Regions.end()) {
        auto domain = s.Domains.find(region->second.DomainId);
        if (domain != s.Domains.end()) d["realm_name"] = str(domain->second.Name);
        auto owner = s.Entities.find(region->second.OwnerEntityId);
        if (owner != s.Entities.end()) {
            d["owner_name"] = str(owner->second.Name);
            auto head = s.Characters.find(owner->second.HeadCharacterId);
            if (head != s.Characters.end()) d["owner_name"] = str(head->second.Name);
        }
    }
    const auto economy = s.Evolution.Areas.find(key);
    if (economy != s.Evolution.Areas.end()) {
        d["food_stored"] = economy->second.FoodStored; d["food_produced"] = economy->second.Produced;
        d["food_consumed"] = economy->second.Consumed; d["food_wasted"] = economy->second.Wasted;
    }
    const auto market_area = s.Market.Areas.find(key);
    if (market_area != s.Market.Areas.end()) d["nutrition"] = market_area->second.Nutrition;
    const auto market = s.Market.Regions.find(a.RegionId);
    if (market != s.Market.Regions.end()) {
        const auto &m = market->second;
        d["household_cash"] = m.HouseholdCash; d["producer_cash"] = m.ProducerCash;
        d["food_price"] = m.FoodPrice; d["food_coverage"] = m.FoodCoverage; d["wage_coverage"] = m.WageCoverage;
        Array goods; for (double stock : m.Stock) goods.append(stock); d["goods"] = goods;
    }
    Array cohorts;
    for (const auto &c : a.Cohorts) {
        Dictionary row; row["population"] = c.Population; row["culture_id"] = id(c.CultureId);
        row["culture"] = str(s.Cultures.at(c.CultureId).Name); cohorts.append(row);
    } d["cohorts"] = cohorts; return d;
}
Array CampaignBridge::people() const {
    Array out; if (!initialized) return out;
    for (const auto &entry : world.State().Characters) {
        const auto &p = entry.second; Dictionary d;
        d["id"] = id(p.Key); d["name"] = str(p.Name); d["alive"] = p.Alive;
        d["player"] = p.Key == player; out.append(d);
    } return out;
}
Dictionary CampaignBridge::inspect_person(const String &value) const {
    C::Id key = 0; if (!initialized || !parse_id(value,key)) return {};
    const auto &s = world.State(); const auto found = s.Characters.find(key);
    if (found == s.Characters.end()) return {};
    const auto &p = found->second; Dictionary d;
    d["id"] = id(key); d["name"] = str(p.Name); d["alive"] = p.Alive; d["area_id"] = id(p.AreaId);
    d["age_days"] = (p.Alive ? s.Day : p.DiedDay) - p.BornDay;
    d["culture"] = str(s.Cultures.at(p.CultureId).Name); d["parents"] = ids(p.Parents);
    d["children"] = ids(world.ChildrenOf(key)); d["spouse_id"] = id(p.SpouseId);
    Array paths;
    for (size_t i = 0; i < C::DeityCount; ++i) {
        Dictionary path; const auto &a = p.Affinities[i]; const auto &b = p.Birthright[i];
        path["name"] = C::DeityName(static_cast<C::Deity>(i)); path["power"] = C::Power(a,s.Policy);
        path["inherited"] = a.BirthrightPower; path["earned_favor"] = a.EarnedFavor;
        path["transferred_favor"] = a.TransferredFavor; path["source_parent"] = id(b.SourceParentId);
        path["reserved_power"] = b.FullPower; path["released"] = b.Released; paths.append(path);
    }
    d["affinities"] = paths;
    auto family = s.Politics.CharacterFamilies.find(key);
    if (family != s.Politics.CharacterFamilies.end()) {
        d["family_id"] = id(family->second);
        auto estate = s.Politics.Families.find(family->second);
        if (estate != s.Politics.Families.end()) d["family_wealth"] = estate->second.Wealth;
    } return d;
}
Array CampaignBridge::realms() const {
    Array out; if (!initialized) return out;
    const auto &s = world.State(); const auto hierarchy = C::InspectHierarchy(s);
    for (const auto &entry : s.Domains) {
        const auto &r = entry.second; Dictionary d;
        d["id"] = id(r.Key); d["name"] = str(r.Name); d["retired"] = s.Politics.RetiredDomains.count(r.Key) != 0;
        d["head_id"] = id(r.HeadCharacterId); d["capital_id"] = id(r.CapitalAreaId);
        auto head = s.Characters.find(r.HeadCharacterId); d["ruler"] = head == s.Characters.end() ? String("Vacant") : str(head->second.Name);
        auto h = hierarchy.Domains.find(r.Key);
        d["population"] = h == hierarchy.Domains.end() ? int64_t(0) : h->second.Population;
        d["areas"] = h == hierarchy.Domains.end() ? int64_t(0) : int64_t(h->second.AreaIds.size());
        auto e = s.Evolution.Domains.find(r.Key);
        if (e != s.Evolution.Domains.end()) { d["treasury"] = e->second.Treasury; d["legitimacy"] = e->second.Legitimacy; }
        auto b = s.Market.Budgets.find(r.Key);
        if (b != s.Market.Budgets.end()) {
            d["tax_receipts"] = b->second.TaxReceipts; d["services_paid"] = b->second.ServicePaid;
            d["reserve_target"] = b->second.ReserveTarget; d["service_shortfall"] = b->second.ServiceShortfall;
        } out.append(d);
    } return out;
}
Array CampaignBridge::item_catalog() const {
    Array out;
    for (const auto &item : C::PlayerItems()) {
        Dictionary d; d["id"] = item.Key; d["name"] = item.Name; d["slot"] = int64_t(item.Slot);
        d["health"] = item.Health; d["essence"] = item.Essence; d["damage"] = item.Damage; out.append(d);
    } return out;
}
Dictionary CampaignBridge::inventory() const {
    Dictionary out; if (!initialized) return out;
    const auto found = world.State().Players.find(player); if (found == world.State().Players.end()) return out;
    const auto &p = found->second; out["origin"] = str(p.Origin);
    Array equipment; for (auto key : p.Equipment) equipment.append(id(key)); out["equipment"] = equipment;
    Array items;
    for (const auto &entry : p.Items) {
        Dictionary item; item["id"] = id(entry.first); item["definition"] = str(entry.second.Definition);
        items.append(item);
    } out["items"] = items; return out;
}
Dictionary CampaignBridge::equip_item(int64_t slot, const String &value) {
    C::Id key = 0; if (!initialized) return bad("world_not_initialized");
    if (slot < 0 || slot >= int64_t(C::EquipmentSlot::Count) || !parse_id(value,key)) return bad("item_request_invalid");
    return result(world.EquipPlayerItem(operation("equip"),player,static_cast<C::EquipmentSlot>(slot),key));
}
Dictionary CampaignBridge::warband() const {
    Dictionary out; Array troops, offers;
    out["troops"] = troops; out["offers"] = offers;
    if (!initialized) return out;
    const auto &s = world.State();
    out["world_id"] = str(s.WorldId); out["leader_id"] = id(player);
    out["capacity"] = int64_t(C::TroopCapacity(s,player));
    out["area_id"] = id(s.Characters.at(player).AreaId);
    for (const auto &entry : s.Troops) {
        const auto &t = entry.second;
        if (t.LeaderId != player) continue;
        Dictionary troop;
        troop["id"] = id(t.Key); troop["kind"] = "peasant";
        troop["name"] = "Peasant " + id(t.Key);
        troop["culture"] = str(s.Cultures.at(t.CultureId).Name);
        troop["home_area_id"] = id(t.HomeAreaId);
        troop["trained"] = C::IsTroopTrained(s,t.Key);
        troops.append(troop);
    }
    for (const auto &o : C::LocalRecruitment(s,player)) {
        Dictionary offer;
        offer["culture_id"] = id(o.CultureId); offer["culture"] = str(s.Cultures.at(o.CultureId).Name);
        offer["available"] = int64_t(o.Available); offer["approval"] = str(o.Approval);
        offers.append(offer);
    }
    out["troops"] = troops; out["offers"] = offers; return out;
}
Dictionary CampaignBridge::recruit(const String &culture_id, int64_t count) {
    C::Id culture = 0;
    if (!initialized) return bad("world_not_initialized");
    if (!parse_id(culture_id,culture) || count < 1 || count > 1000000) return bad("recruitment_request_invalid");
    return result(world.RecruitPeasants(operation("recruit"),player,world.State().Characters.at(player).AreaId,
        culture,static_cast<uint32_t>(count)));
}
Dictionary CampaignBridge::dismiss_troop(const String &troop_id) {
    C::Id troop = 0;
    if (!initialized || !parse_id(troop_id,troop)) return bad("troop_request_invalid");
    return result(world.DismissTroop(operation("dismiss"),player,troop));
}
Array CampaignBridge::recent_events(int64_t limit) const {
    Array out; if (!initialized) return out;
    const auto &events = world.State().Events;
    const auto count = std::min(events.size(), static_cast<size_t>(std::clamp<int64_t>(limit,0,200)));
    for (size_t i = events.size(); i > events.size() - count; --i) {
        const auto &e = events[i - 1]; Dictionary d;
        d["day"] = e.Day; d["kind"] = str(e.Kind); d["detail"] = str(e.Detail);
        d["actor_id"] = id(e.ActorId); d["operation_id"] = str(e.OperationId); out.append(d);
    } return out;
}
String CampaignBridge::snapshot_text() const {
    if (!initialized) return {};
    std::ostringstream out; if (!C::Save(world.State(),out)) return {}; return str(out.str());
}
Dictionary CampaignBridge::restore_snapshot(const String &text, const String &player_id) {
    C::Id key = 0;
    if (!parse_id(player_id,key) || !key || text.length() > 64 * 1024 * 1024) return bad("checkpoint_invalid");
    C::Snapshot state; std::istringstream input(text.utf8().get_data());
    auto checked = C::Load(input,state); if (!checked) return result(checked);
    if (!state.Players.count(key) || !state.Characters.count(key)) return bad("checkpoint_player_missing");
    C::World candidate; checked = candidate.Restore(state); if (!checked) return result(checked);
    world = std::move(candidate); player = key; initialized = true; return result({true,"checkpoint_restored",""});
}
