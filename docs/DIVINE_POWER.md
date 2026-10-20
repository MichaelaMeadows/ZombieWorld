# Divine powers, favor and inheritance

Migrated and reconciled for Godot on **2026-10-20** from `F:\DarkLineage\Birthright.md`, `F:\DarkLineage\docs\world-systems.md` and `F:\DarkLineage\docs\divine-inheritor.md`. This is the detailed companion to the [game description](../Birthright.md).

The portable rules and progression catalog are imported. Their presence does not imply that every command or ability has a Godot interface. In particular, Godot combat currently has no campaign divine casting or Essence integration.

For the proposed expansion, see [Divine skill trees](DIVINE_SKILL_TREES.md): seven branching trees and 70 designed nodes spanning character, troop and empire benefits. That catalog is a design draft; the implementation status and four existing native nodes below remain unchanged.

## The seven paths

Characters have independent Strife, Harmony, Death, Life, Nature, Order and Chaos affinities. The thematic pairs are Strife/Harmony, Death/Life and Nature/Order. Combining opposing paths is not currently penalized or prohibited.

- **Strife:** battle, terror, personal force and allied bloodlust.
- **Harmony:** peaceful cooperation, support, alliances and the welfare of followers.
- **Death:** intended necromancy, sacrifice and blood magic.
- **Life:** intended birth, growth and healing.
- **Nature:** intended elemental force, shrines and restoring wilderness.
- **Order:** intended crafting, construction and manmade creation.
- **Chaos:** implementation name for the seventh path; final identity and powers remain open.

Only Strife and Harmony have the bounded player progression catalog below. The other five have functioning affinity, favor and inheritance records, but no authored player combat trees. A demographic birth, economic building, heal or ordinary kill is not automatically a divine reward.

Political domains are separate from these paths. Culture patronage exists; a general worship/population-to-ruler power transfer does not.

## Power and favor

Each affinity separates three persistent sources:

- **Birthright power:** active inherited strength, backed by genealogy and grant receipts.
- **Earned favor:** accumulated personal effort, recorded separately from ancestry.
- **Transferred favor:** strength received through transfers, not relabeled as personal effort.

With `L = floor(power)`, the default cumulative favor-equivalent cost is:

```text
next whole-point cost = 100 + 50 × L
C(power) = 100 × L + 25 × L × (L − 1)
           + (power − L) × (100 + 50 × L)

power = inverseC(C(active birthright) + earned favor + transferred favor)
```

The full favor-equivalent costs of powers 1, 2 and 3 are 100, 250 and 450. An origin with one inherited power and zero earned favor needs 150 additional favor to reach power 2. Fractions count: that origin plus 75 earned favor has power 1.5.

Costs rise with strength. Transferring the entire reserve of another power-50 Strife character to a power-50 winner gives approximately **71.32**, not 100. The implementation's power safety bound of one million is not a designed progression cap. Rules are versioned; costs and limits are prototype tuning.

Learning, assigning, equipping and casting do not spend persistent favor. Ability points are a derived allocation budget; transient combat resources are separate.

Authority: [World.h](../native/campaign/upstream/Source/Birthright/Campaign/World.h) and [World.cpp](../native/campaign/upstream/Source/Birthright/Campaign/World.cpp).

## Inheritance and release

For each child's path:

1. Choose two different priority paths for the child. The primary receives 100% of source power, the secondary 80%, and each other path 30%.
2. Select the stronger available parent for that path. Each parent can allocate that path to only one ordinary child. If the stronger parent's reservation is used, try the other parent. If neither is available, there is no automatic grant. Equal powers currently prefer the first listed parent.
3. Snapshot the source's power at birth. Later parental growth does not expand that grant.
4. Activate 20% immediately. Reserve the remaining 80% for the source parent's death or a living-parent release ceremony.

Different paths can have different source parents. A parent with power 10 provides a full primary grant of 10: 2 active and 8 pending. As a secondary path that would be 8 total, 1.6 active and 6.4 pending. Fractional budgets prevent low-power inheritances from rounding away.

The immediate grant does not drain the parent. A release ceremony requires a living parent and child in the same area, releases pending grants from that parent to that child atomically, and consumes the parent's reserve. Spending order is transferred favor, earned favor, then inherited reserve. It can fail validation if the available reserve or resulting state is invalid.

A split ceremony reallocates the original reservation's favor-equivalent budget through positive fractions totaling one. It cannot reclaim already active/released power or overwrite a grant from the other parent. Splitting allocates a promise; it does not itself vest power or spend donor reserve. Release or death does that later.

Reservations survive a child's death, preventing silent reuse. Lifetime grant receipts prevent already spent immediate birthright from being granted again when the remainder vests.

Recording a source parent's death processes pending inheritance, dissolves marriage and vacates offices; enabled succession policies can choose replacements. The core birth command requires two married living adults, with adulthood at 6,570 days by default. Adoption, illegitimate heirs, fertility, lifespan, split fees and player death/retirement remain design work.

**Godot:** recorded ancestry, affinities and inheritance sources can be inspected. Marriage, birth, split and release are not exposed player commands. NPC family simulation uses the imported rules.

## Sanctioned Strife duels

The core command requires adult opponents in one area. A trusted host must certify consent/eligibility and the real outcome. It transfers the loser's entire current Strife reserve in favor-equivalent units, clears that reserve and records the loser's death. Other paths are not transferred to the winner.

Death currently also honors pending child inheritance, including reserved Strife. Whether a sanctioned duel should instead forfeit unvested Strife grants is unresolved. Preserve the existing behavior until that design choice is made.

Godot has no sanctioned-duel adapter. Sandbox kills, ordinary attacks and winner declarations do not invoke this transfer.

## Player origins

The current choices are permanent:

- **Heir of the Ember Oath** (`ember_heir`): Strife, with deceased parent Aren of the Ember Oath.
- **Heir of the Concord** (`concord_heir`): Harmony, with deceased parent Mira of the Concord.

Both give an adult player recorded ancestry, a fully released one-power birthright with provenance, and zero newly earned favor. This is fully released because the source parent is deceased, not an exception granting every newborn full power.

The core learns and assigns the first blessing and creates starter inventory. Godot exposes these origins before **New world**, but does not yet expose or cast the recorded blessing. Current onboarding creates a fresh world; legacy Unreal save conversion is outside scope.

## Earning favor

Current player reward tuning is:

These rewards follow the player's chosen origin: Harmony rewards apply to
Concord heirs and the Strife camp reward applies to Ember Oath heirs. Merely
having some power in the other path does not unlock its origin reward policy.

- **Harmony recruitment:** 5 favor per recruited troop.
- **Harmony tending:** 2 per troop per tending day. Tending advances one campaign day.
- **Harmony training:** 10 per troop trained once. Training takes two campaign days and permanently adds 25 maximum battle health to that troop.
- **Strife camp clearance:** 75 for a certified persistent frontier-camp clearance with a living hero, once per camp.

Harmony no longer earns favor from casualty-free camp victories. That BR-013 policy was superseded by BR-079; the source document retained both descriptions, so this migrated reference resolves the conflict.

Time is the current care/training cost. Warband wages, carried supplies and neglect penalties are not implemented. Two eligible Strife clears supply the 150 favor needed to move an untouched origin from power 1 to 2.

Random skirmishes, field tests, retreats, defeats, NPC clearances and repeated result evidence do not award player camp favor. Persistent camp outcome and reward commit atomically with camp-ID receipts.

**Godot today:** recruitment uses the real core command and can earn Harmony favor. Tending, training and persistent player camp clearance are not exposed. The combat sandbox grants no favor or loot and writes no casualties back.

NPC Practice, Govern and Cultivate activities can award culture-patron favor after validating life state, location, cooldown and applicable office/territory/funding requirements. Autonomous planners exclude protected players. Advancing days alone gives the player no favor, including the legacy daily-steward fallback. The trusted `AwardFavor` API provides validation/accounting, not permission for a client to invent deserving actions or amounts.

Authority: [PlayerProgression.cpp](../native/campaign/upstream/Source/Birthright/Campaign/PlayerProgression.cpp), [WorldRecruitment.cpp](../native/campaign/upstream/Source/Birthright/Campaign/WorldRecruitment.cpp) and [WorldEvolution.cpp](../native/campaign/upstream/Source/Birthright/Campaign/WorldEvolution.cpp).

## Ability points and blessings

Each whole power point supports one ability point in its own path. Power 1.9 supports one point; power 2 supports two. Allocating points does not reduce favor or divine strength.

**The seven divine power types are non-interchangeable.** Every node's power requirement and allocation cost use its own path exclusively. Strife power cannot unlock or pay for Harmony, Death, Life, Nature, Order or Chaos nodes, and the same separation applies to every type. Unused points and respec refunds stay within their original path. A character may develop several paths, but their powers are never summed to qualify for a node.

The imported catalog contains:

- **Strife — Sunburst** (`strife_sunburst`): power 1, cost 1, no prerequisite; source ability ID `Burst`.
- **Strife — Lance** (`strife_lance`): power 2, cost 1, requires Sunburst; source ability ID `Bolt`.
- **Harmony — Divine rally** (`harmony_rally`): power 1, cost 1, no prerequisite; source ability ID `Rally`.
- **Harmony — Mend** (`harmony_mend`): power 2, cost 1, requires Divine rally; source ability ID `Mend`.

Free respec outside battle clears learned nodes and assignments, preserving origin, ancestry, items, power and favor. If power falls, choices remain recorded but can become dormant; power, prerequisite and allocation validation determine what is usable.

These stable content IDs preserve the catalog, not Unreal combat behavior. Godot needs its own ability implementations and a bridge/UI for learning, assignment and respec before these become playable. Do not interpret old Unreal key bindings or its twenty-slot combat lab as current Godot controls.

## Essence, stamina and equipment

**Essence** was the source game's transient casting resource, starting at 100 plus equipment capacity and regenerating during combat. It was not saved back as earned favor. Godot's existing **stamina** serves its current action combat. This migration does not equate them or introduce a second persistent power counter; the future casting-resource integration remains unfinished.

The imported starter catalog contains three individually owned items:

- **Heir's sword:** weapon slot, +4 basic attack damage.
- **Travel coat:** armor slot, +20 health capacity.
- **Lineage signet:** charm slot, +20 Essence capacity.

These are catalog modifiers for the source's derived player combat build. Inventory ownership and validated equip/unequip persist in Godot, but these modifiers currently do not change the sandbox hero's damage, health or resources. Modular visual equipment is a separate presentation feature, not proof of campaign-stat integration.

The core inventory limit is 256 individually identified items per character. Trading, loot acquisition, stacking, durability and consumables are not completed campaign loops.

## Divine power and followers

Troop capacity uses current whole path levels:

```text
capacity = max(10, floor(5 × (strongest level + 0.30 × other levels summed)))
```

Only one tied strongest path has full weight. Levels 4, 3 and 2 permit 27 troops; zero power still permits 10. A reduction leaving the band above capacity is rejected rather than automatically deserting troops.

Godot's Warband panel uses this authority, and the sandbox deploys every current player troop. Trained troops receive the existing +25 health bonus even though training is not yet exposed. Sandbox casualties remain temporary.

## Authority and remaining work

Keep persistent progression in the imported `World` and its versioned [PlayerProgressionTypes.h](../native/campaign/upstream/Source/Birthright/Campaign/PlayerProgressionTypes.h) / [PlayerProgression.cpp](../native/campaign/upstream/Source/Birthright/Campaign/PlayerProgression.cpp). Commands validate ownership, life state, prerequisites, budgets, slots, compatibility and replay IDs before commit. The host must additionally enforce authenticated identity and encounter state.

The current [native bridge](../native/campaign/bridge/campaign_bridge.cpp) exposes origins, inspection, equipment and recruitment/dismissal, not the full progression command set. New Godot controls should call this authority instead of recalculating power or making independent favor ledgers in GDScript.

Remaining integrations include tending/training, a divine build interface, Godot blessing casts, equipment-derived hero stats and certified persistent encounters. Remaining design choices include most path obligations/abilities, seventh-path identity, awakening, worship concentration and player succession. Neither missing integration nor unresolved design is presented here as completed gameplay.

This documentation update was checked against the imported rules/catalog and current bridge. It changes no game rules; existing runtime test evidence belongs in [Living world](WORLD_SYSTEMS.md).
