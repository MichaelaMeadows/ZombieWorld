# The Briarwatch — Autonomous World foundation

For the intended game, read the [game description](Birthright.md) and
[divine powers, favor and inheritance](docs/DIVINE_POWER.md). These distinguish
the design and imported simulation rules from currently exposed Godot features.

Open project.godot with **Godot 4.7.2 stable** and press **F5**.
The project now starts at the **Living world** map. Choose an origin and click
**New world**, then inspect land, people/lineage, items, realms and the journal.
Advance time with **+1 / +30 / +365 days**. Save and Load use a new Godot world
checkpoint; no Unreal saves are imported. Wheel zooms, middle-drag pans.

Open **Warband** in the world inspector to recruit local volunteers or dismiss
troops. **Combat sandbox** brings your current warband as human AI allies; an
empty warband enters solo. Press **F6 during play** to return to the world.
Sandbox losses do not remove campaign troops. Campaign equipment remains separate.
The sandbox player starts with a two-handed greatsword: more range and damage,
slower swings, and 30% knockdown for one second on surviving enemies.
For the original sword comparison, open `game/sword_arena.tscn` and press **F6**.
See `docs/STATUS_EFFECTS.md` for weapon tuning and reusable effect definitions.

To fight the new **club bandits and bandit archer**, open
`game/bandit_arena.tscn` in Godot and press **F6** (Run Current Scene).
This standalone encounter has two club bandits and one archer. Arrows can be
blocked, dodged or stopped by cover. It starts without campaign allies.
See [Bandit assets and animation setup](docs/BANDITS.md) for reusable scenes,
sprite sheets and weapon settings.

To inspect modular equipment, open `game/equipment_workbench.tscn` and press F6.
Select a unit, facing and animation, scrub frames, and toggle armor or weapons.
It compares original art with the layered kit at enlarged and battle sizes.
See [Modular sprite framework](docs/MODULAR_SPRITES.md) for authoring and setup.

This is an additive fantasy sword-combat slice based on the MIT-licensed
stesproject/godot-2d-topdown-template v1.9.4. Original demos remain in scenes/.

Controls: WASD walk, hold Shift to run, mouse aim freely (including diagonals),
left click light cut, Q heavy cleave,
right click hold block, Space dodge, E rest at watchtower, Escape pause,
F3 tuning/metrics, R restart after defeat or victory.

In PowerShell:

```powershell
# On a clean checkout: Python, Git and MSVC C++ build tools are required.
.\tools\build_campaign.ps1 -Bootstrap -Tests
.\tools\godot.ps1
.\tools\godot.ps1 --headless --editor --import --quit
.\tools\godot.ps1 --headless res://tests/run_tests.tscn --quit-after 600
.\tools\godot.ps1 --headless res://tests/encounter_smoke.tscn --fixed-fps 60 --quit-after 900
.\tools\verify.ps1
.\tools\verify_campaign.ps1 -Godot $env:GODOT_BIN
```

Set GODOT_BIN to your Godot console executable if it is installed elsewhere.
No .NET SDK is needed: presentation/combat use GDScript and the campaign uses
a native C++ extension. Compatibility rendering is
selected. First import creates Godot's cache; dependencies and selected CC0 art
are included. Settings are stored in Godot's user data directory, separately from
the upstream demo preferences.

For original template demonstrations, open scenes/menus/start_screen.tscn and
press F6. Their original keyboard bindings remain unchanged.

See docs/MILESTONES.md for verified scope and remaining quality gates.
See [World systems](docs/WORLD_SYSTEMS.md) for the migration, native build and
current world-interface scope.
