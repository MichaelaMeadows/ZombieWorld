#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Birthright::Campaign
{
enum class EquipmentSlot : std::uint32_t { Weapon, Armor, Charm, Count };
struct CarriedItem { std::uint64_t Key = 0; std::string Definition; };
// Character IDs refer to the existing genealogy. These records never own combat vitals.
struct PlayerProgression
{
    std::uint32_t ContentVersion = 1;
    std::string Origin;
    std::uint64_t Ancestor = 0;
    std::vector<std::string> Learned;
    std::array<std::string, 4> AbilitySlots{};
    std::map<std::uint64_t, CarriedItem> Items;
    std::array<std::uint64_t, 3> Equipment{};
    std::uint64_t NextItemId = 1;
};
}
