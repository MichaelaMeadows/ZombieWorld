# Bandit characters

Open `game/bandit_arena.tscn` and press F6 in the editor. The encounter contains
two club bandits and an archer. Controls match the existing combat arena.
The main F5 world scene remains the campaign map.

## Assets

Each character has 56 authored transparent poses across movement, combat and
the shared supplementary diagonal sheet:

- `assets/art/bandits/club_motion.png` and `archer_motion.png`: four columns,
  seven rows—idle, walk right/down/up, run right/down/up.
- `club_combat.png` and `archer_combat.png`: four columns, six rows—attack
  right/down/up/northeast, hurt, death.
- `diagonal_attacks.png`: four columns, two rows—club southeast attack,
  archer southeast attack.

Western directions mirror eastern poses. Attack aim is continuous; the visual
selects the nearest of eight facing sectors. Attack frames are ready, full
wind-up, strike/release, recovery. Directional resting guards use recovery poses;
the horizontal idle uses a four-frame breathing loop.

Weapons are painted into the hand/arm poses. The club is iron-banded wood; the
archer carries a wooden bow and quiver. Released arrows are lightweight Godot
vector geometry. No separate orbiting weapon sprite is used.

These are variable-bound atlases, not uniformly spaced cells. The
`bandit_*_sprites.tres` resources contain all rectangles, foot anchors and scale
values. Native AtlasTexture regions reference the original PNGs without
resampling them. `tools/build_bandit_resources.py` reconstructs the resource
metadata from saved bounds; it does not modify artwork.

Walking and running advance by actual ground distance, using 144px and 168px
per full cycle respectively. Walls and status effects therefore slow or stop the
animation with movement. Hurt lasts 0.24 seconds; death takes 0.85 seconds and
holds its final pose. Knockdown still uses the shared prone/status presentation.

## Reuse and tuning

Instantiate `game/actors/bandit_club.tscn` or `bandit_archer.tscn`. Set `arena`
before adding the actor to an encounter and include it in that arena's `actors`
registry for AI targeting and melee hits. Unassigned NPC IDs are made unique.

`ActorArchetype` resources keep stats, movement, weapon and presentation separate
from runtime state:

- Club bandit: 80 health, 110 walk / 175 run speed, 18-damage swing with 64px
  reach and 0.38-second startup. The reusable loadout also includes a slower
  28-damage heavy smash; the default AI uses its primary swing.
- Archer: 65 health, the same movement speeds, 14-damage arrow, 420px maximum
  travel, 500px/second speed, 0.65-second draw. It retreats inside 150px, shoots
  within approximately 344px, and repositions when an ally or obstacle blocks
  line of sight. Aim locks during the draw, allowing sidesteps.

`AttackDefinition.delivery` chooses melee or projectile delivery. Both use the
same damage, team, block, immunity and on-hit status policy. Each bow release
spawns one arrow. A swept ray detects thin walls even at high speed; allies stop
arrows without friendly damage. Arrows use their flight direction for shields
and knockback, independent of where the shooter moves afterward. Knockdown or
stagger interrupts an unreleased draw. Released arrows continue independently;
if their source node is freed, they expire safely.

The standalone test encounter does not import or modify the campaign warband.
To add these archetypes elsewhere, assign the arena's `enemy_archetypes` array.

## Verification and provenance

`tests/bandit_tests.tscn` checks all movement frames, eight attack directions,
hurt/death selection, club damage, arrows, defenses, cover, interrupted draws,
and live ranged AI. It runs in `tools/verify.ps1`. A brief native render fixture,
`tests/bandit_visual.tscn`, produces `docs/verification/bandit-encounter.png`.

All five PNGs are generated project artwork, not Pixel Frog assets.
Accepted generation prompts are in `assets/art/bandits/GENERATION_PROMPTS.txt`.
