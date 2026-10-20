# Living world — first migration slice

Design references: [game description](../Birthright.md) and
[divine powers, favor and inheritance](DIVINE_POWER.md), migrated and reconciled
with the Godot implementation on 2026-10-20.

Implemented 2026-10-20. DarkLineage's world simulation now runs inside this Godot project. The migration reuses production campaign code, with a new native bridge and Godot interface. A subsequent warband integration supplies the current player roster to the Godot combat sandbox. It does not convert existing Unreal saves or import Unreal battles.

## Play

Run the project and choose an origin before **New world**:

- **Ember Oath** starts with Strife ancestry.
- **Concord** starts with Harmony ancestry.

Both use the source's origin, lineage and owned-item rules. Each fresh world uses seed 42, Duvaleia geography and six procedurally generated empires. Population, families, markets, politics and camps advance together.

Use the mouse wheel to zoom, middle-drag to pan, and click a cell to inspect it. Choose political, population, unrest or terrain layers. The right-hand selector opens:

- **Area & economy:** owner, population, cohorts, prosperity, unrest, local food/nutrition and regional market conditions.
- **People & lineage:** named people, recorded parents/spouse/children, seven divine affinities, inheritance sources and family wealth.
- **Items & equipment:** the three existing source items, ownership and validated equip/unequip actions.
- **Realms & estates:** rulers, populations, treasuries, family/household/producer cash and public budgets.
- **World journal:** recent source events and their decision details.
- **Warband:** current troops and capacity, local recruitment offers and dismissal.

Time advances only through the +1/+30/+365-day buttons. Simulation runs on an owned worker thread; the map remains visible, and state-reading/mutating controls reject requests until the completed state is published. The UI never participates in economic calculations.

**Save/Load** uses `user://campaigns/world.json`, with a previous generation at `.bak`. It saves only worlds created here. The primary is validated before backup rotation and a newly staged file is checked before installation. Loading invalid files leaves the current world intact; a damaged primary can recover its previous backup. There is no automatic save on quit and no promise of hardware/power-loss durability.

**Combat sandbox** opens the existing greatsword arena with the player and every
troop in their current warband; an empty warband enters solo. Troops use human
sword actors, stable campaign IDs and the player's team. Both sides select living
hostile targets, and allies follow the hero when not fighting. Peasants start at
90 health, with the source's +25 bonus for already-trained troops. Re-entry reads
the current roster again. Runtime **F6** returns to the world; sandbox casualties
and rewards do not change campaign records. Player equipment remains separate.

## Code and ownership

`native/campaign/upstream/` contains the original portable campaign sources and test harness. `PROVENANCE.json` records SHA-256 hashes of the imported source files. They are unchanged by this slice; original source names and schema migrations remain internal implementation details.

This includes aggregate populations/cultures, geography and traversal, named families/succession, birthright, nonlinear favor, player origins/items, food production/trade/spoilage/nutrition, household and producer cash, estates, fiscal budgets, political relationships, factions, strategic wars and frontier camps/defense. Internal strategic conflict rules remain part of world evolution; no Unreal battle implementation is imported.

`native/campaign/bridge/` owns a single `World` and exposes fresh generation, advancement, queries, item equipment and snapshots. It uses the production validators and mutators. Unsigned identities cross into Godot as decimal strings, preserving their full range. There is no engine object per civilian, area, market or family.

`game/campaign/campaign_session.gd` retains that object across scenes. It owns the worker lifecycle, cached summary and new checkpoint files. While a worker advances the world, no main-thread code queries or mutates it.

`game/world/` provides the map and inspectors. Geometry is cached once per campaign object. Dynamic area projections refresh after committed changes. Screen-space picking is converted to source-map coordinates and tested against actual cell polygons using a spatial bucket index.

The Godot combat scripts, loadouts and status rules remain separate.

## Data provenance

`tools/world/source/duvaleia.full.json` was copied from the source project's `ArtSource/WorldMaps/duvaleia.full.json`.

- Source SHA-256: `552c1b91b0910c678743b7a858c8a8c6324e952fb0818d2cca538886d26a9b61`.
- Normalized by the original `ImportAzgaar.py` into `game/content/world/duvaleia.geography`.
- 6,069 cells: 4,175 land and 1,894 water.
- 2,028 road links; 148 source sea routes remain excluded from walking.
- Land adjacency and disconnected islands are preserved.

Regenerate the geography:

```powershell
python native/campaign/upstream/tools/World/ImportAzgaar.py `
  --input tools/world/source/duvaleia.full.json `
  --output game/content/world/duvaleia.geography
```

This creates geography, not a migrated save. Each New world action generates new society records and configures evolution, politics, market policy and camps using the source rules.

## Build

Requirements: Windows x64, Python 3, Git, Visual Studio C++ build tools/Windows SDK and Godot 4.7.2. .NET is not used.

```powershell
.\tools\build_campaign.ps1 -Bootstrap -Tests
.\tools\build_campaign.ps1 -Target template_release
```

Bootstrap downloads official `godot-cpp` tag `godot-4.5-stable`, pinned to `e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`, and SCons 4.9.1 into ignored workspace directories. The 4.5 API subset is tested on the installed Godot 4.7.2 engine. The small binding profile includes RefCounted, FileAccess and OS. No custom engine build is required.

Debug/release DLLs are generated under `addons/campaign/bin/`; build artifacts and dependency checkouts are ignored. A clean checkout requires the build step before opening the game. Stop running game/editor instances if Windows reports a library is in use while rebuilding.

Godot's extension loader uses `addons/campaign/campaign.gdextension`. A future export must include the native DLL for the target configuration and the `.geography` data using an export include filter. A packaged executable has not been validated in this slice.

## Validation

Final checks for this slice:

- Windows debug and release native libraries compiled.
- All **150/150 original portable campaign tests passed** in the migrated build. An initial harness run lacked the expected output directory; the build script now creates it in the ignored test build directory.
- Godot bridge checks: **45 assertions passed headlessly**, including 30-day versus daily exact equivalence, conservation of currency, protected-player behavior, atomic rejection, equipment ownership/slot validation, and new checkpoint/backup recovery.
- Rendered world checks: **46 assertions passed**, exercising all five inspectors and capturing the actual 1280×720 map. The screenshot was visually reviewed.
- Existing Godot import, combat, movement, zombie, encounter and HUD layout checks passed.
- The inherited shutdown resource warning remains; no new runtime script errors were reported in the final checks.

Commands:

```powershell
# Set GODOT_BIN to the installed Godot console executable.
.\tools\verify_campaign.ps1 -Godot $env:GODOT_BIN
.\tools\verify_campaign.ps1 -Rendered -Godot $env:GODOT_BIN
.\tools\verify.ps1
```

The campaign checks use `user://campaign-test/world.json`, separately from the playable checkpoint. They deliberately corrupt only that test file to verify recovery.

Evidence: `docs/verification/campaign-release-build.log`, `campaign-tests-runtime.log`, `world-combat-regression.log` and `campaign-world.png`. Runtime logs may be ignored by version control.

## Current boundaries

This is a working foundation and inspection interface, not a complete campaign game UI. Weighted travel and social/estate commands are present in the core but are not yet exposed as player actions. Recruitment/dismissal are available in the Warband inspector. The lineage browser follows recorded relationships; it is not a graphical family tree. The item catalog is the source's three authored items.

The simulated world's behavior is preserved, including abstract political conflict. There are no DarkLineage battle scenes, no old-save import controls and no second GDScript simulation. Combat performance, multiplayer, export packaging, large-world frame budgets and human economic balance are separate work.

## Warband sandbox follow-up

Verified empty/nonempty/reduced rosters, persistent IDs, safe initial placement,
human presentation, hostile targeting, friendly-damage rejection, follow behavior
and temporary sandbox casualties: 41 headless assertions and 42 rendered assertions
passed. The expanded six-tab world suite passed 46 headless assertions; existing
combat/movement/zombie/encounter/layout regressions passed. Both native DLL
configurations were rebuilt.

Run the focused roster checks with:

```powershell
.\tools\godot.ps1 --headless res://tests/campaign/warband_sandbox_tests.tscn
```

The rendered capture is `docs/verification/warband-sandbox.png`. Tests generate
their own fresh world and never overwrite the player's checkpoint.
