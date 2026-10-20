#include "World.h"
#include <algorithm>
#include <set>

namespace Birthright::Campaign {
Result World::RecordTroopBattleCasualties(const std::string& operation, const WarbandBattleInput& input, const WarbandBattleResult& result) {
    const auto checked=CheckOperation(operation); if (!checked) return checked;
    const auto leader=StateData.Characters.find(input.LeaderId);
    if (input.Version!=1 || result.Version!=1 || input.WorldId!=StateData.WorldId ||
        input.EncounterId.empty() || input.EncounterId.size()>128 || result.EncounterId!=input.EncounterId || operation.size()>900 ||
        leader==StateData.Characters.end() || leader->second.AreaId!=input.AreaId || !StateData.Areas.count(input.AreaId) ||
        StateData.Areas.at(input.AreaId).IsWater || input.Troops.size()!=TroopCount(StateData,input.LeaderId) || result.Casualties.empty())
        return {false,"battle_casualty_context_invalid",input.EncounterId};
    std::set<Id> deployed, casualties;
    for (auto id:input.Troops) {
        const auto troop=StateData.Troops.find(id);
        if (!deployed.insert(id).second || troop==StateData.Troops.end() || troop->second.LeaderId!=input.LeaderId)
            return {false,"battle_roster_stale",std::to_string(id)};
    }
    for (auto id:result.Casualties)
        if (!deployed.count(id) || !casualties.insert(id).second)
            return {false,"battle_casualty_not_deployed",std::to_string(id)};
    auto next=StateData;
    next.Events.push_back({operation,"troop_battle_casualties",next.Day,input.LeaderId,0,
        "encounter="+input.EncounterId+" count="+std::to_string(casualties.size())});
    for (auto id:casualties) {
        next.Troops.erase(id);
        next.Events.push_back({operation+"/troop/"+std::to_string(id),"troop_battle_death",next.Day,input.LeaderId,0,
            "troop="+std::to_string(id)+" encounter="+input.EncounterId});
    }
    // No civilian refund, recruitment-pool reset or troop-ID reuse on death.
    return Commit(std::move(next));
}
}
