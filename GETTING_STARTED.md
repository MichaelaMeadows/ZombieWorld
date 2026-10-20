# The Briarwatch — Autonomous World foundation

Open project.godot with **Godot 4.7.2 stable** and press **F5**.
This is an additive fantasy sword-combat slice based on the MIT-licensed
stesproject/godot-2d-topdown-template v1.9.4. Original demos remain in scenes/.

Controls: WASD move, mouse face, left click light cut, Q heavy cleave,
right click hold block, Space dodge, E rest at watchtower, Escape pause,
F3 tuning/metrics, R restart after defeat or victory.

In PowerShell:

```powershell
.\tools\godot.ps1
.\tools\godot.ps1 --headless --editor --import --quit
.\tools\godot.ps1 --headless res://tests/run_tests.tscn --quit-after 600
.\tools\godot.ps1 --headless res://tests/encounter_smoke.tscn --fixed-fps 60 --quit-after 900
.\tools\verify.ps1
```

Set GODOT_BIN to your Godot console executable if it is installed elsewhere.
No .NET SDK is needed: project gameplay is GDScript. Compatibility rendering is
selected. First import creates Godot's cache; dependencies and selected CC0 art
are included. Settings are stored in Godot's user data directory, separately from
the upstream demo preferences.

For original template demonstrations, open scenes/menus/start_screen.tscn and
press F6. Their original keyboard bindings remain unchanged.

See docs/MILESTONES.md for verified scope and remaining quality gates.
