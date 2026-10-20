# DarkLineage world and simulation in Godot

Planning date: 2026-10-20. Status: first implementation delivered; scope revised at the user's request.

## Recommended direction

Keep this project's Godot combat and bring DarkLineage's existing campaign simulation into it as a native C++ library behind a small GDExtension. Build the overworld, campaign session and management interface in Godot. Legacy player-save migration and battle integration are excluded from this migration.

The important discovery is that `Source/Birthright/Campaign` already contains a largely engine-independent simulation. `tools/World/Run-World.ps1` compiles the production campaign sources and tests with a standalone C++17 toolchain. Replicating the behavior therefore does not require rewriting the economy, inheritance or politics in GDScript.

This preserves the difficult, interconnected rules while letting the playable game use the movement, melee timing, dodge, block, stamina, status effects and presentation already present here. GDExtension is Godot's supported mechanism for loading native libraries without rebuilding the engine; `godot-cpp` supplies its official C++ bindings. Pin and validate the bindings, engine API and build configuration against this project's Godot 4.7.2 installation during the first spike. [Godot GDExtension documentation](https://docs.godotengine.org/en/stable/engine_details/engine_api/gdextension/what_is_gdextension.html), [godot-cpp compatibility](https://docs.godotengine.org/en/stable/tutorials/scripting/cpp/about_godot_cpp.html).

Assumptions for this plan:

- Windows desktop and single-player are the first delivery targets.
- The strategic map remains a continental map. The existing Godot combat is a separate sandbox; campaign-to-battle transitions and specific battle simulations are outside this migration.
- Preserve the actual DarkLineage geography and simulation behavior. Local encounter art and combat use this Godot project's style.
- Keep strategic NPC wars abstract initially. Large armies do not all become live combat actors.
- Campaign item, origin and lineage records migrate independently of combat. Existing combat behavior remains its own implementation.

## What the source actually contains

Paths in this section are relative to the DarkLineage project, inspected at `F:\DarkLineage`. Source code takes precedence over historical task documents, several of which describe earlier behavior.

### Geography and travel — reuse rules and data; rebuild presentation

`MapGeography.*`, `EmpireGeneration.*`, `WorldTraversal.*`, `WorldTravel.*`, and the geography importer define:

- Azgaar-derived polygons, elevation, biomes, roads and land adjacency.
- One campaign area per source cell, including inert water cells.
- Procedural empires, regions, owning entities, cultures, frontier claims and influence.
- Terrain/development/road-weighted travel with deterministic route selection and fractional campaign time.
- Disconnected land masses without fabricated ocean connections.

The current source map is `ArtSource/WorldMaps/duvaleia.full.json`. `tools/World/default-world.json` specifies seed 42, six empires, six regions per empire, claim radius 20 and influence radius 30. Preserve these as a baseline preset, not hardcoded constants. Existing saves supply their own embedded geography and policy.

The older `continental-atlas` PNGs are artwork references; do not substitute their coastlines for the authoritative Duvaleia graph. Azgaar supplies geography, while Birthright generates its own societies and rules.

### Population, culture and named lives — reuse

`World.*` and `WorldEvolution.*` cover aggregate cultural cohorts, births/deaths with fractional remainders, migration, prosperity, unrest, named marriages and children, mortality, succession, office appointments and activity choices. Ordinary inhabitants are population records; only selected people have individual genealogy.

Retain all seven divine affinities: Strife, Harmony, Death, Life, Nature, Order and Chaos. Preserve nonlinear power costs, separate inherited/earned/transferred reserves, parent-specific grants, reservations, release ceremonies and replay receipts. Preserve the protected-player boundary against autonomous NPC behavior.

### Economy and estates — reuse together

`WorldEvolution.*`, `WorldMarket.*`, `WorldMarketTypes.h`, `WorldBudget.cpp` and persistence files implement:

- Production, food reserves, seasonal supply, consumption and elapsed-time spoilage.
- Regional food exchange, household affordability and accumulated nutrition.
- Household/producer cash, wages, rents, taxes and treasury spending.
- Materials, tools and luxuries, demand-aware production and investment.
- Estate construction, dividends, depreciation and inheritance.
- Receipt-based public budgets, reserves and distribution diagnostics.

Food remains authoritative in area economy records. Markets must not acquire a second food inventory, and UI summaries must not become extra money accounts. Current source has market policy version 4; earlier descriptions of money sinks and simpler budgets are superseded.

### Politics, military and frontier threats — reuse simulation

`WorldPolitics.*`, `WorldMilitary.cpp`, `WorldBandits.*` and `WorldDefense.cpp` cover holdings, ambitions, loyalty, friendship, grievances, factions, rebellion, territorial wars, truces, levies, upkeep, supply/readiness, casualties and demobilization.

Persistent camps raid actual food stocks, consume/spoil captured food and can grow, starve, be cleared or disperse. Landowners can fund defense expeditions using real estate money and civilian cohorts; expeditions travel, fight and return survivors. Keep these existing strategic rules independent of local combat AI.

### Player campaign systems — reuse policy; adapt effects

`WorldRecruitment.cpp`, `WorldWarbandBattle.cpp` and `PlayerProgression*` provide:

- Local volunteer eligibility, finite recruitment pools and persistent troop IDs.
- Population-backed recruitment and permanent troop casualties.
- Two playable origins, equipment/inventory records and divine unlock choices.
- Warband care and training through campaign-time commands.
- Seven affinity records, with authored playable trees currently limited to Strife and Harmony.

Latest source awards Harmony for recruiting, tending and training; older documentation describing Harmony camp rewards is superseded. Strife camp-clearance rewards remain. Preserve the current command policies first, with later balance changes recorded separately.

### Existing limitations to preserve honestly

Full diplomacy actions, player domain management, player succession, maritime travel, general goods trading, physical strategic army battles, siege/escort objectives, and warband wages/carried supplies are not all implemented playable systems. NPC succession and strategic army supply exist, but are different features. Recreating placeholders does not complete those systems.

`Reference/NpcEmpireScaffold` is a historical scaffold, not the production simulation. Use `Source/Birthright/Campaign` as the source of truth.

## Revised delivery sequence

The user explicitly removed old-save conversion and specific battle simulations from the migration on 2026-10-20. This sequence supersedes the earlier encounter-first plan.

1. **Portable core and native bridge.** Import the production simulation with a hash manifest, compile it independently of Unreal, and expose fresh world generation, time advancement and read-only views to Godot.
2. **Working world interface.** Show Duvaleia with political/population/unrest/terrain layers, inspect areas and regional markets, browse people and ancestry, manage owned equipment, inspect rulers/estates, and read causal events.
3. **New Godot persistence.** Save worlds created in this project with their current IDs, clocks, genealogy, policies and inventories. No Unreal save browser, compatibility UI or transfer workflow.
4. **Broader campaign interaction.** Expose travel and selected existing social/estate commands as useful player actions, add richer genealogy and ownership navigation, and improve observability of market conditions and demographic changes.
5. **World validation and scale.** Preserve exact command/accounting invariants, deterministic time advancement and long-run scenario coverage. Profile large worlds independently of rendered combat.

## Implemented first slice

- The portable C++ source and original regression harness live in `native/campaign/upstream/`, with `PROVENANCE.json` recording original file hashes.
- `native/campaign/bridge/` owns one campaign and returns Godot dictionaries/arrays. Persistent IDs cross the boundary as decimal strings; no Unreal runtime classes or battle methods are exposed.
- `game/campaign/campaign_session.gd` owns the native instance across scenes. Background advancement has exclusive ownership until a completed result is published. UI cannot read or mutate the campaign while it advances.
- `game/world/world.tscn` is the new startup scene. It exposes the world, items, lineage, regional economy, rulers/estates and event journal.
- Geography is cached separately from dynamic political/population projections. The imported map has 6,069 cells, 4,175 land cells and 2,028 road links.
- Fresh initialization enables evolution, politics, markets and camps in their source-defined order, then creates a protected player with either existing origin.
- New Godot checkpoints preserve full state and validate a staged write before replacing the primary, keeping a previous-generation backup. They are separate from Unreal saves.
- The existing combat arena is available from the map's Combat sandbox button. A subsequent user request connects the player's current warband as allied human troops; recruitment and dismissal are available in the world inspector. Runtime F6 returns to the world; no battle results are applied.

## Authority and simulation rules

The imported core is the sole owner of population, genealogy, money, inventories, time, policies and receipts. Godot views only project committed state. Existing `AdvanceDays` ordering and absolute-day cadences remain unchanged. A large population remains aggregate records, rather than a node per inhabitant.

Item bonuses are campaign metadata in this slice. They do not modify sandbox health, stamina or attacks. Internal source contracts supporting strategic war/camp rules remain with the portable core to avoid changing those rules, but no physical battle host is migrated or exposed.

Use the original world tests plus bridge checks for generation, rejected-command rollback, equip validation, batched/daily equivalence, protected-player behavior, currency accounting and fresh-world persistence. UI tests exercise each inspector and the real rendered map. Current combat tests remain regression checks, not work on DarkLineage battle scenarios.

## Follow-up work

- Interactive travel using the already imported weighted graph and travel rules.
- Richer family tree, culture and holdings navigation; social and estate actions through validated commands.
- More discoverable economic diagnostics and event filtering, including household affordability versus food availability.
- New item authoring and inventory interactions beyond the three source items, if desired.
- Export packaging and measurements for full worlds; the native Windows debug/release build path is in place.

These are extensions of the migrated authority. Do not create a second GDScript economy or lineage model. Co-op, seamless exploration, player succession, expanded divine combat trees, old-save conversion and large physical battle simulations are outside this slice.

See [World systems implementation](WORLD_SYSTEMS.md) for build, controls, validation and current limitations.
