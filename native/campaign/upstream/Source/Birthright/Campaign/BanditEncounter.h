#pragma once
#include <cstdint>
#include <vector>

namespace Birthright::Campaign {
enum class BanditKind : std::uint8_t { Peasant, Raider, Archer };
// Temporary encounter input, separate from persistent campaign troop records.
struct BanditEncounter {
    std::uint32_t Version = 1, Seed = 0;
    std::vector<BanditKind> Enemies;
    bool IsValid() const {
        if ((Version!=1 && Version!=2) || Enemies.size()<3 || Enemies.size()>(Version==1?8u:24u)) return false;
        unsigned archers=0, melee=0;
        for (auto k:Enemies) {
            if (k==BanditKind::Archer) ++archers;
            else if (k==BanditKind::Peasant || k==BanditKind::Raider) ++melee;
            else return false;
        }
        return archers<=2 && melee>0;
    }
};
inline BanditEncounter MakeBanditEncounter(std::uint32_t seed) {
    BanditEncounter result; result.Seed=seed;
    // Specified integer PRNG: injected seeds reproduce content without a rendered world.
    std::uint32_t state=seed ^ 0xA341316Cu;
    if (!state) state=1;
    auto next=[&]() { state^=state<<13; state^=state>>17; state^=state<<5; return state; };
    const unsigned count=3+next()%6; unsigned archers=0;
    result.Enemies.push_back(BanditKind::Peasant);
    for (unsigned i=1;i<count;++i) {
        auto kind=static_cast<BanditKind>(next()%3);
        if (kind==BanditKind::Archer && archers==2) kind=BanditKind::Raider;
        if (kind==BanditKind::Archer) ++archers;
        result.Enemies.push_back(kind);
    }
    return result;
}
}
