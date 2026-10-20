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
Greatsword sweep/cleave hit arcs are 93.5°/102° (15% narrower than 110°/120°).
Their equipped silhouettes animate a wider windup and follow-through, holding
the extended strike for the first 35% of recovery before returning to guard.
This presentation does not extend the active damage window or weapon reach.
Zombie swipe: 100 ms startup, 70 ms active, 170 ms recovery, 12 damage,
34px reach, 100° arc, 8 stamina. Brute swipe: 130/80/210 ms, 18 damage,
38px reach, 100° arc, 10 stamina. The brute resists light stagger.
The shared hit check includes 12px of target body allowance: maximum center-to-center
distance is 46px (50px for the brute). AI begins a swipe at 42/46px.
Zombie attacks are authored in zombie_swipe.tres and zombie_brute_swipe.tres.

WASD walks at 155px/s; Shift runs at 245px/s. Walking and running use distinct
four-pose contact/passing cycles, including front/back views. The greatsword
player's calibrated walk cycle covers 100px (~0.65s at full speed); running
covers 132px (~0.54s). NPC stride settings are unchanged.
Playback advances with actual displacement, so blocked movement and hit-stop
do not make the feet race in place. Run is suppressed by block/attack/status
constraints. Animation follows travel direction; attack aim follows the mouse.
Player and NPC attacks use a continuous normalized aim vector, locked at startup.
Eight-direction presentation selects separate diagonal attack poses without
rotating the whole body. Damage, reach, stamina costs and knockdown are unchanged.

Dodge: 22 stamina, 220 ms movement at 480px/s (~106px), 190 ms invulnerability.
Directional burst poses crouch, lean and recover over the dodge timeline;
equipment follows the same transform. Dodge momentum ends with the action.
Sprinting drains 20 stamina/s while moving. Exhaustion switches to walking
until at least 20 stamina has recovered, preventing speed flicker while Shift
stays held. Idle, blocked movement, attacks, dodges and hit-stop do not charge
sprint stamina. Regeneration resumes 800 ms after the last sprint expenditure.
Attack startup/active frames cannot be dodge-canceled; recovery can.
Only the final 130 ms of recovery accepts a buffered next attack.
Stamina regenerates at 27/s after action recovery delay; holding block suspends it.
Block absorbs frontal damage and spends the attack's stamina cost. Insufficient
stamina or a hit from behind bypasses it.

The same attack timeline selects sprite wind-up frames 0–2, active frames 3–4,
and recovery frame 5. Damage and animation are not driven by separate timers.
Ground telegraphs/trails use the same authored reach and arc.
Zombie.png is a transparent 1374×1145 sheet, six columns and five rows of 229px cells:
idle, walk, right swipe (mirrored for left), down swipe, up swipe. Per-frame foot
offsets in ArenaActor keep the feet grounded across all directions. The brute
uses the same animation with a larger scale and darker tint. Generation provenance
and the full image prompt are saved beside the sheet in Zombie.prompt.txt.
Local hit-stop pauses only the affected actor's motion/timeline; other actors,
project time and presentation continue. Engine.time_scale is never modified.

## Shared impact and responsiveness

Hit-stop now preserves stagger, recoil and buffered-attack time instead of
consuming them while the unit is frozen. A frame that ends hit-stop advances the
action only by its unpaused remainder. Status durations and damage immunity still
use world/physics time, so a one-second knockdown remains one second.

The final 130 ms of recovery accepts one next attack, including a press exactly
at the boundary. Consuming the queue happens before its deadline is discarded.
The unit can aim during recovery; the old swing keeps its original damage
direction, and the next swing snapshots the new facing. Interrupts and dodges
clear queued attacks. Player movement and aim are sampled before the actor tick.

Voluntary movement accelerates on physics time. Knockback is a separate velocity
that decays at 800 px/s² after hit-stop; it is no longer repeatedly scaled by
frame delta and erased by movement friction. Stagger cancels voluntary momentum,
while armor retains its existing resistance to light interruption. Collision
still resolves through CharacterBody2D. These rules apply to player and NPC units.

Accepted damage produces a brief white flash and directional recoil on all rigs.
Blocks produce a shorter blue flash and local impact pause without a hurt pose.
Healing no longer triggers hurt feedback. Existing impact sparks, audio and
damage-number settings remain in use.

Melee and ranged AI reconsider immediately after recovery and when their authored
post-attack delay ends. That delay counts ready time, so hit-stop cannot consume
it. Regular target/path decisions retain their 180 ms interval; no new per-frame
roster scans are introduced. Reaching the last waypoint clears movement intent.

Regression scene: `tests/combat_responsiveness_tests.tscn`, included by
`tools/verify.ps1`. It covers shared rules across players, allies, zombies,
brutes, club bandits and archers, plus recovery buffering and AI wake-up timing.
Weapon timing, damage, reach, stamina costs and status probabilities are unchanged.

F3 edits per-player resource copies for startup, recovery and reach. These are
session-only changes. Final balancing belongs in game/content/*.tres.
Automated tests establish rules, not enjoyment. Extended human playtesting is
still required to sign off milestone 2's subjective quality gate.
