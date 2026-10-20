#include "PlayerProgression.h"
#include <iomanip>
#include <istream>
#include <ostream>

namespace Birthright::Campaign
{
void WritePlayerProgression(const Snapshot& s, std::ostream& o)
{
    o << "PLAYER_PROGRESSION 1 " << s.Players.size() << '\n';
    for (const auto& entry : s.Players)
    {
        const auto& p = entry.second;
        o << entry.first << ' ' << p.ContentVersion << ' ' << std::quoted(p.Origin) << ' ' << p.Ancestor << ' ' << p.NextItemId << '\n';
        o << p.Learned.size(); for (const auto& key : p.Learned) o << ' ' << std::quoted(key); o << '\n';
        for (const auto& key : p.AbilitySlots) o << std::quoted(key) << ' '; o << '\n';
        o << p.Items.size() << '\n'; for (const auto& item : p.Items) o << item.first << ' ' << std::quoted(item.second.Definition) << '\n';
        for (auto id : p.Equipment) o << id << ' '; o << '\n';
    }
}
Result ReadPlayerProgression(std::istream& in, Snapshot& s)
{
    auto bad = []() { return Result{false, "player_save_invalid", "Invalid player progression record"}; };
    std::string tag; unsigned version = 0; std::size_t count = 0;
    if (!(in >> tag >> version >> count) || tag != "PLAYER_PROGRESSION" || version != 1 || count > s.Characters.size()) return bad();
    for (std::size_t i = 0; i < count; ++i)
    {
        Id id = 0; PlayerProgression p; std::size_t nodes = 0, items = 0;
        if (!(in >> id >> p.ContentVersion >> std::quoted(p.Origin) >> p.Ancestor >> p.NextItemId >> nodes) || nodes > DivineNodes().size()) return bad();
        for (std::size_t n = 0; n < nodes; ++n) { std::string key; if (!(in >> std::quoted(key))) return bad(); p.Learned.push_back(key); }
        for (auto& key : p.AbilitySlots) if (!(in >> std::quoted(key))) return bad();
        if (!(in >> items) || items > 256) return bad();
        for (std::size_t n = 0; n < items; ++n) { CarriedItem item; if (!(in >> item.Key >> std::quoted(item.Definition)) || !p.Items.emplace(item.Key, item).second) return bad(); }
        for (auto& item : p.Equipment) if (!(in >> item)) return bad();
        if (!s.Players.emplace(id, std::move(p)).second) return bad();
    }
    return {true, "accepted", ""};
}
void ExportPlayerProgression(const Snapshot& s, std::ostream& o)
{
    // All string values resolve to validated, fixed catalog identifiers (no user text).
    o << ",\n\"playerProgression\":["; bool comma = false;
    for (const auto& entry : s.Players)
    {
        if (comma) o << ','; comma = true; const auto& p = entry.second;
        o << "{\"character\":" << entry.first << ",\"contentVersion\":" << p.ContentVersion << ",\"origin\":\"" << p.Origin << "\",\"ancestor\":" << p.Ancestor << ",\"learned\":[";
        for (std::size_t i = 0; i < p.Learned.size(); ++i) { if (i) o << ','; o << '"' << p.Learned[i] << '"'; }
        o << "],\"abilitySlots\":[";
        for (std::size_t i = 0; i < p.AbilitySlots.size(); ++i) { if (i) o << ','; o << '"' << p.AbilitySlots[i] << '"'; }
        o << "],\"items\":["; bool itemComma = false;
        for (const auto& item : p.Items) { if (itemComma) o << ','; itemComma = true; o << "{\"id\":" << item.first << ",\"definition\":\"" << item.second.Definition << "\"}"; }
        o << "],\"equipment\":[";
        for (std::size_t i = 0; i < p.Equipment.size(); ++i) { if (i) o << ','; o << p.Equipment[i]; }
        o << "],\"nextItemId\":" << p.NextItemId << '}';
    }
    o << ']';
}
}
