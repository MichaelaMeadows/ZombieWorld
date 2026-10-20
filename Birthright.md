# Birthright — Game vision and implementation guide

Migrated from `F:\DarkLineage\Birthright.md` and updated for this Godot project on **2026-10-20**. Birthright is the original internal campaign/module name; DarkLineage is the source Unreal project. This guide preserves its eight-section game description. The world, items, economy and lineage carry forward; combat uses this project's Godot systems.

**Vision** describes the intended game. **Core** means a rule exists in the imported portable simulation, which does not necessarily make it a player action. **Godot** identifies currently exposed behavior. **Open** means unresolved design or unfinished integration. Numerical balance values are provisional unless identified as confirmed inheritance rules.

Start here for the game concept, use [Divine powers and inheritance](docs/DIVINE_POWER.md) for detailed progression rules, and [Living world](docs/WORLD_SYSTEMS.md) for current controls, build instructions and validation. This document does not import existing Unreal saves, revive its battle implementation or declare its historical acceptance results to be Godot results.

## 1. Overview

You are a divine inheritor: a person whose ancestry carries a trace of god-touched power. Begin as an adventurer, gather followers, acquire influence and holdings, and eventually shape a realm and a dynasty. Other inheritors have ambitions of their own. Population, resource pressure, family turnover and political competition should keep the world changing around you.

The inspirations are Mount & Blade: Bannerlord, the D&D Birthright setting, and large empire and simulation games. The direction is an original setting, pantheon and visual identity, combining direct top-down action with leadership and a strategic campaign:

**Fight and lead → gain resources, reputation and divine favor → develop followers and holdings → contest territory → inherit consequences.**

This is a sandbox for emergent stories. A growing demonic empire is a possible later narrative element, not an implemented campaign. An approximate one inheritor per 1,000 people and spontaneous awakening remain vision targets. Generation does not enforce that rarity; named-character counts are not a measure of inheritor prevalence.

Single-player comes first. Future co-op and large battles remain aspirations requiring their own implementation and measurements. The source project's 1,000-active-unit target is not a demonstrated Godot capacity.

**Godot:** choose a Strife or Harmony origin, create a world, inspect its people and economy, advance days, equip owned items, recruit or dismiss local troops, save/load a new checkpoint, and enter the combat sandbox with the current warband. The full adventurer-to-ruler-to-heir loop is unfinished. Travel controls, player government, playable succession and persistent combat rewards remain future integrations.

## 2. The World

### Geography and ownership

An **area** is the smallest campaign geography unit. Areas compose **regions**, which belong to political **domains** such as realms or empires. A region's owning entity, its current named head, its family and its realm's sovereign are distinct identities.

Areas contain culture cohorts, development, prosperity, unrest, settlement suitability, traversal cost, neighbors, influence and claims. Geometry supplies polygons, heights and biomes. Food stocks and production belong to associated economic records. Source geographical metadata does not imply a completed climate, resource-deposit or individual-settlement simulation.

The current Duvaleia map has **6,069 cells: 4,175 land and 1,894 water**. Fresh Godot generation uses seed 42 and requests six empires with six owned regions each. These are starting settings, not permanent lore. Water is visible but has no land population, ownership or walking links. Unclaimed and contested land can exist.

Political ownership and cultural influence are separate: changing a border does not instantly replace the inhabitants or their culture.

### People, culture and time

Ordinary inhabitants are aggregated into culture cohorts with birth, death and migration state. Named people retain parents, spouse, children, age, location, life state and seven divine affinities. Named promotion removes a person from a cohort rather than duplicating population. Enabled family policies produce NPC marriages, births, deaths and office succession.

Cultures have patron profiles and occupational tendencies. Terrain, roads and development affect connectivity and movement. Cultural influence changes faster than conservative neighboring population migration. Ruler conversion, suppression and full cultural economic/diplomatic modifiers remain unfinished.

Campaign time advances explicitly; it does not follow real-world elapsed time. Daily updates and slower campaign-day policies drive the simulation. Godot enables evolution, politics, markets and frontier camps when creating a world. Its current time controls advance 1, 30 or 365 days.

**Core:** land travel uses connected routes weighted by terrain, roads, development and source-cell distance. These are abstract travel costs, not calibrated kilometers per day. **Godot:** clicking a cell inspects it; it does not move the player. Sea travel and a player travel interface remain future work.

## 3. The Economy

### Production and exchange

The vision is a productive world in which areas provide food/materials and developed places turn inputs into tools and goods. Development should support population and processing while affecting extraction. The proposed universal tradeoff of more development versus less extraction is not the implemented formula.

The core aggregates stocks and accounts by area/region rather than modeling every business and shopper. It has four goods:

- **Food:** produced, stored, consumed, exchanged, spoiled or stolen. Area records own stocks; regions organize distribution and storage capacity.
- **Materials:** the main durable recipe input, without a finished mining/forestry deposit system.
- **Tools:** made using materials.
- **Luxuries:** made using materials and tools; household purchases follow food needs and a cash buffer.

Recipes are bounded by inputs and demand. Estate construction spends family cash and materials/tools, then adds productive capital after a delay. Autonomous investment responds to need and reserves. A complete player building or buy/sell interface does not exist.

### Food and hardship

Production responds to labor, seasons, nutrition, productive capacity and investment. Recruited or mustered troops leave civilian cohorts, reducing local labor. Households receive available subsistence food up to a provisional 80% of need, then buy food with cash. Food scarcity and inability to afford available food are separate causes of hardship.

Connected regional markets exchange food under throughput and price policies; closures and wars between endpoints can block trade. Interregional exchange currently moves food, not all goods. Durable goods use regional inventories and simplified prices. Merchant caravans and maritime markets remain future work.

Opening food reserves decay once per campaign day, with a provisional annual compounded spoilage rate of 12.5%. Transfers do not cause another spoilage step. Overflow, spoilage, consumption and bandit exports are accounted separately. Nutrition responds gradually, recovers more slowly than it deteriorates, and affects labor and demographics.

### Money, estates and public spending

Money moves among households, producers, family estates and treasuries. Producers pay wages; households buy goods; taxes and taxed rents/dividends fund treasuries; estates earn income and invest. With markets active, transfers conserve existing currency after explicit activation liquidity.

Public budgets use actual tax receipts. The default policy normally allocates 90% of receipts after estimated daily military obligations, building reserves and releasing affordable amounts during shortfalls. Reserves are targets within one treasury, not additional money. Public services pay producers with population/hardship weighting. Owner-led camp defense is funded separately by the owner's family.

Family wealth and productive estates survive succession. Strategic armies require funding and food; supply/pay weakness affects readiness. The player's peasant warband currently has **no recruitment price, wages, carried-food upkeep or neglect penalties**. Strategic army economics must not be mistaken for player warband upkeep.

**Godot:** economy, estates and budgets are inspectable and advance through the core. Player trading, construction and taxation interfaces remain future work. Accounting checks do not establish economic equilibrium or final balance.

## 4. How Domains Function

A political domain can ultimately represent a kingdom, empire, republic, guild or another institution. The current core models territorial realms, named heads, regional owning entities and landed families. Distinct government mechanics for republics, guilds or research institutions have not been authored.

The vision includes concentrating a domain's divine power into a few people, usually a ruler. Patron profiles and personal favor already exist, but **there is no general population-worship-to-ruler power transfer system**. Political domains and divine paths are separate concepts.

Divine power increases a lord's desired holdings. Loyalty responds to holdings, friendship, legitimacy and economic conditions. Rulers maintain relationships through funded visits or grants. Family identity currently follows a simple first-parent ancestral rule, not a finished legal inheritance system.

Sustained grievances can form factions. Rebellion requires organization, eligible leadership, supporting non-capital territory and conflict/cooldown eligibility. Supporting owners need current grievances; old faction membership alone is insufficient. Secession creates a new realm and civil war. Retired realms retain historical IDs.

Strategic wars use bounded territorial goals, land reachability, funding and cooldown rules. Levies come from real cohorts, consume food and suffer permanent losses. Survivors return to their original area/culture on demobilization. Their resolution remains abstract world simulation; strategic armies are not automatically deployed into the Godot arena.

NPC family evolution and office succession exist. The player is excluded from automatic NPC family/activity/mortality selection. This does not decide final player aging, death or heir selection. An office can remain vacant without an eligible successor.

Persistent frontier camps raid reachable food, stockpile supplies, pay upkeep, grow or decline when unfed. Camp strength is an abstract threat measure, not a civilian-backed bandit population. Eligible landed owners can fund cohort-backed expeditions that travel, consume supplies and take losses.

**Godot:** realms, estates, people and events are inspectable. Player governance, diplomacy, land acquisition and certified camp-assault results remain future integrations.

## 5. Divine Powers

The pantheon has three opposed pairs and a seventh path. Every named character can hold all seven affinities; the core does not enforce exclusion or penalties for opposing paths. Deity names, complete obligations and most ability catalogs remain to be authored.

- **Strife:** war, terror and battle, especially worthy opponents; personal strength, fear and allied bloodlust. Sanctioned death duels can transfer Strife reserve. Generic kill rewards and worthy-opponent scoring are not implemented.
- **Harmony:** peaceful cooperation, alliances and helping others; intended population/morale benefits, group fighting and successful rule over happy populations. The current player reward policy concerns recruiting and caring for followers.
- **Death:** intended necromancy, sacrifice and blood magic, including undead and blood-created creatures. Affinity and inheritance exist; those gameplay systems do not.
- **Life:** intended birth, growth and healing of people, animals and plants. Ordinary demographic births or existing combat healing do not automatically award Life favor.
- **Nature:** elemental force, including lightning and fire; chaotic or destructive without being inherently malicious. Practice, shrines and wilderness restoration are intended themes, not completed earning loops.
- **Order:** building, crafting and manmade creation, including improved weapons and armor. Existing economic construction does not automatically award Order favor.
- **Chaos:** the current seventh-path identifier. The original outline left its identity and unaligned role undefined. Its final theme, obligations and abilities remain open; Nature must not be silently redefined to resolve the overlap.

The imported catalog contains only two small ability trees: Strife's **Sunburst → Lance** and Harmony's **Divine rally → Mend**. These definitions and learning rules are present in the core, but their build UI and casts are not connected to Godot combat. See [Divine powers and inheritance](docs/DIVINE_POWER.md).

The proposed [Divine skill trees](docs/DIVINE_SKILL_TREES.md) expand all seven paths into 70 connected nodes, with distinct character, troop and empire specialties. This is an authored design catalog awaiting implementation; its proposed Chaos identity remains provisional.

## 6. Gaining Divine Power

Persistent strength comes from separately recorded **inherited birthright**, **earned favor** and **transferred favor**. Power is derived through a nonlinear cost curve; it is not a combat resource bar.

Each successive whole power point costs more favor: 100 to reach power 1, then 150 to reach 2, then 200 to reach 3 under current defaults. The detailed formula, fractional levels and transfer example are in [Divine powers and inheritance](docs/DIVINE_POWER.md#power-and-favor).

The trusted core validates favor awards and receipts; gameplay hosts must establish eligible actions. NPC Practice, Govern and Cultivate activities award patron favor, subject to eligibility, location, cooldown and applicable office/funding requirements. Protected players receive no favor merely from advancing time.

Current player tuning grants Concord heirs Harmony favor: 5 per recruit, 2 per troop per tending day, and 10 per once-only troop training. Ember Oath heirs earn 75 Strife favor per certified persistent camp clearance with a living hero. These reward policies follow the chosen origin. The older Harmony reward for casualty-free camp victories is superseded.

**Godot:** recruitment can earn Harmony favor. Tending/training and certified camp clearances are not exposed; sandbox kills and victories grant no persistent favor. Sanctioned Strife duels also remain a core-only command.

Wider path-specific obligations, anti-farming design, worship concentration, awakening and most ability unlocks remain open.

## 7. Player Growth

### Birthright and dynasty

These confirmed inheritance rules carry forward:

1. Each child has two distinct priority paths: 100% source strength for the primary, 80% for the secondary, and 30% for the other five.
2. Each path uses the stronger available parent. If already allocated to an ordinary child, try the other parent; if neither is available, there is no automatic grant. Equal powers currently prefer the first listed parent.
3. Only 20% is active at birth. The rest is reserved until the source parent's death or a living-parent release ceremony. Paths can have different source parents.
4. Grants snapshot parental power at birth. Later growth does not increase them. Immediate birthright does not drain the parent; a living-parent release consumes reserve.
5. An explicit split can redistribute the original reserved budget without reclaiming released power or granting it twice. A deceased child's reservation is not automatically recycled.

A parent with power 10 supplying a primary path grants 2 active power and reserves 8. Nonlinear favor accounting and lifetime receipts govern later release. Full ceremony, split and duel interactions are documented in [Divine powers and inheritance](docs/DIVINE_POWER.md#inheritance-and-release).

The core birth command requires married living adults; the default adult age is 6,570 days. Player romance, adoption, fertility, inheritance law and playable death-to-heir are unfinished.

### Followers and capacity

Current troop capacity is:

```text
max(10, floor(5 × (strongest whole path level
                  + 0.30 × sum of other whole path levels)))
```

Only one tied strongest path gets full weight. Levels 4, 3 and 2 permit 27 troops; no power still permits 10. Current inherited, earned and transferred strength all contribute. A power reduction that would leave the band oversized is rejected; automatic desertion does not exist.

Local recruitment offers peasants. Shared culture grants full approval, matching ancestry 75%, and an inherited connection to the local culture's primary patron 50%; otherwise lineage is untrusted. Farmers, prosperity, unrest, approval, population and depletion determine availability. The initial quote caps at 20 volunteers per culture; reopening the panel does not reroll it.

Recruits leave civilian cohorts and receive stable troop IDs. Dismissal does not refill the volunteer pool or return them to civilian population. Timed volunteer replenishment and an upgrade tree remain future work. Godot exposes recruitment/dismissal and deploys this roster in the sandbox.

### Origins, abilities and equipment

Ember Oath and Concord origins create recorded deceased-parent ancestry and a fully released one-power Strife or Harmony birthright. The imported core records the first blessing and three starter items. Whole power determines ability-point budgets; respec affects learned choices, not ancestry or accumulated favor.

Godot exposes origin choice, lineage inspection and validated equipment changes. Equipment bonuses and divine builds do not yet modify the sandbox hero. The lineage browser follows recorded relatives; the Unreal graphical tree was not migrated. Player government, meaningful loot progression and succession remain unfinished.

## 8. Battles

Combat continues in the existing Godot top-down action system. Its weapons, attacks, stamina, status effects, animation and AI evolve independently of Unreal's GAS, combat lab, abilities, bindings and battle scenarios.

**Combat sandbox** deploys the player plus every troop in their current warband. An empty warband deploys only the player. Allies use human sword actors, attack hostile teams and follow the hero when not fighting. Troops retain campaign IDs; already-trained troops receive the source's +25 health bonus.

Sandbox losses and rewards are temporary. Returning to the world does not remove troops or award favor, and re-entry reads the current campaign roster. The original Unreal behavior of permanent encounter casualties is not claimed here. Strategic wars and owner expeditions continue their abstract simulation separately.

Future persistent encounters must certify results through the campaign authority, preserve identity and prevent duplicate losses/rewards. Divine casts, equipment-derived hero stats, camp assaults, player succession and strategic-army deployment require explicit integration into Godot combat. The old Unreal battle implementation and specific simulations are outside this migration's scope.

See [Combat tuning](docs/COMBAT_TUNING.md), [Status effects](docs/STATUS_EFFECTS.md), [AI behavior](docs/AI_BEHAVIOR.md) and [Living world](docs/WORLD_SYSTEMS.md) for the current implementation and evidence. No rendered battle-capacity claim follows from a large simulated civilian population.

## Maintenance and migration record

Adapted on 2026-10-20 from the source's BR-075 eight-section guide, its BR-013 progression additions, and the later BR-079/082 clarifications in `F:\DarkLineage\docs\divine-inheritor.md`. Confirmed inheritance details also come from `F:\DarkLineage\docs\world-systems.md`. Stale Unreal controls, save migration instructions, battle acceptance claims and superseded Harmony rewards were replaced with current Godot behavior.

The imported [campaign source](native/campaign/upstream/Source/Birthright/Campaign/World.h), [progression catalog](native/campaign/upstream/Source/Birthright/Campaign/PlayerProgression.cpp), [native bridge](native/campaign/bridge/campaign_bridge.cpp) and [Godot world interface](game/world/world.gd) determine implementation status. [Migration plan](docs/DARKLINEAGE_MIGRATION_PLAN.md) records the broader migration approach.

Update this guide and the divine-power reference when rules or exposed actions change. Preserve unresolved decisions: seventh-path identity, awakening/rarity, full path obligations, worship-to-ruler power, development versus extraction, player government/succession and strategic-army deployment. Documentation migration does not settle them.
