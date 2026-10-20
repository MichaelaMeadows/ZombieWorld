# Performance evidence

Measured 2026-10-20 using Godot 4.7.2 Compatibility on NVIDIA GeForce RTX 4070.
The rendered 12-second encounter ran at 1280 × 720 with one player and three
full NPC actors, 720 physics frames, AI, navigation, collisions, HUD and impacts.

- Mean rendered frame interval: 17.00 ms (includes startup).
- 95th percentile frame interval: 16.82 ms.
- Slowest sampled NPC decision: 41 microseconds.
- Combat impacts: 8; scripted actions: 9.
- Hero finished on 40 health; scripted replay did not defeat the warden.
- No new script/runtime errors during the replay.

Raw measurements: verification/encounter-rendered.json and its log.
These are engine frame-interval observations, not isolated GPU render timings.
The replay disables audio playback; it still constructs the cached sound palette.
User hardware load and vsync affect the results. This is not a 20/100-actor result
and does not establish a scaling guarantee.

Headless fixed-fps replay verifies behavior only: its 16.67 ms reported delta is
synthetic and must not be interpreted as a performance measurement.

F3 displays FPS, process/physics time, active opponents, combat events, AI states,
decision duration and navigation configuration. Navigation requests run in the
staggered 180 ms decision layer. Small-roster separation is quadratic and should
be replaced by a measured spatial query before scaling to large populations.

Run `.\tools\verify.ps1` for import, 48 assertions, encounter and layout checks.
Use `-Rendered` to add brief automatic 1280×720 and 1920×1080 screenshots.
Run tests/encounter_smoke.tscn normally for a 12-second graphical profile.
The initial State GDScript shutdown leak is documented and narrowly allowlisted;
other errors, script failures or missing completion markers fail verification.
