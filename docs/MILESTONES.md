# Implementation status

## 0 — Baseline established

Commit 9be2806. Upstream v1.9.4 cloned, inspected and imported before modification.
Godot 4.7.2 Compatibility verified. Original main menu and playground_01 launched
graphically, movement/attack keys exercised. Headless boot and audit recorded.
Known clean-clone icon UID repaired. Inherited shutdown warning documented.

## 1 — Playable character established

Commit 276b200. Additive game/ arena, shared CharacterEntity adapter,
CC0 Tiny Swords sprites/terrain/props/watchtower, collision, smooth camera,
mouse-facing, health HUD and pause/restart. Visual check caught and fixed tree
animation slicing and off-screen title; screenshot saved in verification/.
Import and headless boot passed. Longer movement-feel testing remains desirable.

## 2 — Melee implementation and verification

Shared resource-driven combat, light/heavy sword attacks, block, dodge, stamina,
buffering, stagger/knockback, death, armor, directional arcs, local hit-stop,
particles, flashes, synthesized sound palette, damage numbers and camera feedback.
Three autonomous enemies with two melee profiles, static-grid navigation,
local separation and timed decisions. Live tuning, metrics, settings and restart.

Automated rule/UI assertions and a bounded scripted encounter are in tests/.
Screenshots and measured reports are in docs/verification/. Human repeated
combat-feel validation remains a gate; passing tests does not close it.

Final verification: 48 assertions, zero failures; import passes; all three original
playgrounds boot headlessly; 12-second encounter runs without new runtime errors;
HUD/debug/pause/settings bounds pass at 720p and 1080p. Brief rendered screenshots
were inspected. Extended manual playtesting was not completed. The rendered
four-actor replay measured 17.00 ms mean and 16.82 ms p95 frame interval.
The inherited State resource shutdown warning remains.

Files: new game/combat/, game/content/, game/ai/ and presentation feedback; extended
game/actors/, game/ui/ and arena composition. New tests/ scenes, tools/verify.ps1,
GETTING_STARTED.md and all requested docs. Reused upstream CharacterEntity
movement/facing and HealthController clamping/events; upstream gameplay unchanged.

## Deferred, intentionally

Milestone 3: two allied NPCs, group orders, richer tactical behaviors.
Milestone 4: full menu/inventory/UI component catalogue, display settings/rebinding.
Milestone 5: settlement, jobs, resources and simulation clock.
Milestone 6: stable-ID versioned world persistence and 20/100-actor benchmarks.
Milestone 7: scenario definitions and genre separation proof.
Ranged combat is deferred until the initial sword encounter is approved.

Next recommended task: play the sword encounter, tune the committed definitions,
then implement allied guard/follow/retreat through the same intent interface.

## Living world migration — first slice, 2026-10-20

The user redirected world development toward reusing DarkLineage's world, items,
lineage and economy, excluding legacy save migration and battle integration.
This supersedes the generic simulation/persistence direction above.

Imported the portable production campaign core and Duvaleia geography. Added a
Windows native Godot bridge, persistent campaign session, background time
advancement, world map, area/market inspectors, lineage browser, item equipment,
realm/estate views, event journal and fresh Godot save/backup recovery. The
existing combat remains separately accessible.

Verification: debug/release native builds; 150/150 original portable tests;
45 headless bridge assertions and 46 rendered assertions; visual inspection at
1280×720; existing import, combat, movement, zombie, encounter and layout checks.
The inherited shutdown resource warning remains. No export-package or large
rendered-combat performance claim is made.

Next world work: expose existing travel/social/estate commands and deepen
genealogy/economy navigation. See [World systems](WORLD_SYSTEMS.md) and the revised
[migration plan](DARKLINEAGE_MIGRATION_PLAN.md).
