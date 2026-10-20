# Sword tuning and controls

- WASD: movement. Mouse: four-direction facing, independent of movement.
- Left click: immediate light cut. Q: heavy cleave.
- Right mouse held: frontal block. Space: dodge in movement direction, or facing.
- E: recover health and stamina near the blue watchtower.
- Escape: pause/resume; settings includes sound, impact intensity, shake and numbers.
- F3: live combat lab. R: restart after death or victory.

Heavy is a separate key instead of hold-to-charge: light attacks respond on press,
without waiting to decide whether a click was intended as a hold. Rebinding UI is
future work; custom controls are registered in InputMap at arena startup.

Player light: 120 ms startup, 100 ms active, 220 ms recovery, 22 damage,
64px reach, 100° arc, 14 stamina, 180 ms stagger, 45 ms local hit-stop.
Heavy: 420/140/360 ms, 42 damage, 74px reach, 110° arc, 30 stamina,
480 ms stagger, 75 ms local hit-stop.
Raider light: 280 ms startup and 280 ms recovery, 18 damage.
The iron warden uses heavy cleaves and resists light stagger.

Dodge: 22 stamina, 280 ms movement at 480px/s, 190 ms invulnerability.
Attack startup/active frames cannot be dodge-canceled; recovery can.
Only the final 130 ms of recovery accepts a buffered next attack.
Stamina regenerates at 27/s after action recovery delay; holding block suspends it.
Block absorbs frontal damage and spends the attack's stamina cost. Insufficient
stamina or a hit from behind bypasses it.

The same attack timeline selects sprite wind-up frames 0–2, active frames 3–4,
and recovery frame 5. Damage and animation are not driven by separate timers.
Ground telegraphs/trails use the same authored reach and arc.
Local hit-stop pauses only the affected actor's motion/timeline; other actors,
project time and presentation continue. Engine.time_scale is never modified.

F3 edits per-player resource copies for startup, recovery and reach. These are
session-only changes. Final balancing belongs in game/content/*.tres.
Automated tests establish rules, not enjoyment. Extended human playtesting is
still required to sign off milestone 2's subjective quality gate.
