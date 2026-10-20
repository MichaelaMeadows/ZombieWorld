# Modular sprite contract

The runtime is `ModularSpriteRig` plus `ModularSpriteLayer`. All artwork lives
under `assets/art/modular/<family>/`. Existing original atlases remain intact.
Humanoids use separate body, armor and weapon sheets. Creatures can retain a
single complete body. Equipment compatibility requires BOTH animation family
and rig ID; proportions and grip are part of the contract.

## Atlas specification (v1)

Every layer has exactly 56 poses, on an **8-column × 7-row** transparent atlas.
Target canvas: **2048 × 1792**, cells **256 × 256**. Foot-space origin within
each cell: **(128, 220)**. Uniform output scaling is allowed; the normalized
pivot stays `(0.5, 0.859375)`. Never center each layer by its own alpha bounds.
Each consecutive group of four cells is one clip. Read left to right, top down:

0–3 idle east; 4–7 walk east; 8–11 walk south; 12–15 walk north;
16–19 run east; 20–23 run south; 24–27 run north;
28–31 attack east; 32–35 attack south; 36–39 attack north;
40–43 attack northeast; 44–47 hurt; 48–51 death;
52–55 attack southeast. Western directions mirror eastern artwork.

Attack frames: anticipation, full windup/draw, strike/release, recovered guard.
Movement frames: alternating contact and passing, with distinct running poses.
Directional idle uses recovered guard. Death ends lying down.

Generated reference atlases are rendered from the original poses into this
layout. They are layout/identity references, not final modular layers.

## Layer authoring

* `body.png`: the character in a plain close-fitting under-tunic, trousers and
  boots, with bare hands. Preserve identity, pose and anatomy. Remove the weapon,
  shield, outer armor, quiver and other replaceable equipment. Reconstruct
  anatomy that those items concealed. Never keep an old weapon under a new one.
* `armor.png`: ONLY the fitted outer clothing/armor for every reference pose,
  transparent everywhere else. Do not draw another body or face. Cut out pixels
  where hands/arms must show in front. Keep torso placement exactly registered.
* `weapon.png`: ONLY the weapon (and bow arrow during draw if appropriate),
  transparent everywhere else. Preserve grip and perspective. Leave transparent
  cutouts for occluding fingers/hands. Do not draw skin into weapon layers.
* Optional `back`/`foreground` layers handle quivers, shields and hand occlusion.
  Depths: back=-2, body=0, armor=1, weapon=2, foreground=3. Per-frame depths are
  supported. A gear layer should not contain a complete dressed character.

Initial artwork is made with built-in image generation. The user subsequently
authorized deterministic pixel/mask cleanup for exact body registration and
occlusion. Per-family cleanup scripts preserve sources and produce final
layers; Godot renders references and composites for inspection. Inspect every
result and correct misaligned or incomplete sheets. Preserve actual alpha.
Store exact prompts in `GENERATION_PROMPTS.txt` and record deterministic edits.

## Resources

Layer `.tres`: script `res://game/presentation/modular_sprite_layer.gd`; set
label, family, rig_id, slot, sheet, depth. Defaults cover the standard grid.
Optional `regions`/`pivots` allow explicit per-frame registration corrections.
Gear layers also support `frame_scales` (PackedVector2Array, 56 positive factors)
and `frame_rotations` (PackedFloat32Array, 56 radians). These transform about
the authored pivot; mirrored rotation reverses automatically. Use these only
for measured registration, never to substitute one pose for all animation frames.
Rig `.tres`: script `res://game/presentation/modular_sprite_rig.gd`; set label,
family, rig_id, body, equipment (typed Array[ModularSpriteLayer]).
Default display_scale=0.42 applies to the 2048px target width; for a differently
sized output use `0.42 * 2048 / sheet_width` so world dimensions stay consistent.
All layers in one rig MUST have matching cell dimensions unless explicitly
registered and normalized before runtime.

Families / rig IDs:
* greatsword hero: `two_handed` / `human_two_handed_v1`
* club bandit and sword warband: `one_handed` / `human_one_handed_v1`
* archer: `bow` / `human_bow_v1`
* zombies: keep their complete legacy artwork, no equipment conversion needed.

Actor `equip_visual(slot, layer)` changes one actor's equipment; null removes
the slot. `reset_visual_equipment()` restores defaults. Incompatible layers are
rejected without changing equipment. Visual selection never changes combat
damage, reach, attack clocks, or campaign inventory.

Assign `sprite_rig` on an ActorArchetype or WeaponLoadout to select a kit in an
encounter. A directly assigned actor `modular_rig` overrides both; archetype
overrides loadout. `set_visual_rig()` validates the kit and clears equipment
overrides. Unassigned actors continue using their original complete sheets.
Negative-depth equipment draws immediately behind its own body, above terrain.
Positive depths order local children; all layers keep Z=0 so actors Y-sort as
complete characters rather than globally sorting every armor layer above bodies.

The greatsword loadout selects the hero kit, the sword loadout selects the
warband kit, and the club/archer archetypes select their corresponding kits.
The club and warband share a body resource. Original full sheets remain available
for comparison and rollback; zombie shamblers/brutes retain their complete
creature artwork.

## Inspection

Open `game/equipment_workbench.tscn` in Godot and press F6. It shows original and
modular art at 3× size plus a battle-sized composite. Select a unit, action and
facing; pause/scrub and toggle armor/weapon to check each layer independently.
The workbench is separate from campaign play and does not alter saved equipment.
`tools/preview_modular_assets.gd -- <family>` renders a fixed set of poses.
`tools/measure_layer_registration.py` provides optional read-only candidate
measurements against reference artwork; its results need visual inspection.

Functional checks run in `tests/modular_sprite_tests.tscn` and tools/verify.ps1.
They verify independent per-actor swaps, compatibility rejection, frame and
mirror synchronization, registration transforms, removal/reset, hurt/death,
and status transforms. These checks do not replace visual art review.

## Delivered kits and source records

* [Greatsword hero](../game/content/modular/two_handed/hero.tres):
  [generation prompts](../assets/art/modular/two_handed/GENERATION_PROMPTS.txt),
  [cleanup notes](../assets/art/modular/two_handed/QA_NOTES.txt).
* [Club bandit](../game/content/modular/one_handed/club.tres) and
  [warband swordsman](../game/content/modular/one_handed/warband.tres):
  [generation prompts](../assets/art/modular/one_handed/GENERATION_PROMPTS.txt),
  [cleanup notes](../assets/art/modular/one_handed/QA.txt).
* [Archer](../game/content/modular/bow/archer.tres):
  [generation prompts](../assets/art/modular/bow/GENERATION_PROMPTS.txt),
  [cleanup notes](../assets/art/modular/bow/PIXEL_CLEANUP.txt).

New equipment still needs artwork for the matching 56 poses. Reuse the existing
body and animation timing, then author only the replacement equipment layer.
A different grip or stance (for example, switching from sword to bow) selects
another animation family rather than stretching equipment onto incompatible poses.

## Player locomotion polish

The greatsword player's rig now authors `player_walk_stride` (100 world pixels)
and `player_run_stride` (132). These affect presentation cadence only; speed,
sprint stamina, dodge, attacks and NPC movement retain their existing rules.
Playback uses actual displacement, including the short deceleration after input
release. Idle, blocked movement and local hit-stop retain foot phase rather than
restarting the same step. A 20% axis hysteresis prevents diagonal travel from
alternating between side and front/back poses. Combat aim remains independent.

`player_pose_offsets` optionally supplies 56 common translations in atlas pixels.
The hero's walking feet share the existing idle ground baseline; running contact
frames use that baseline and both flight frames lift ten atlas pixels. These
measurements were taken from the existing body's solid-alpha bounds. The entire
body/armor/weapon hierarchy receives the correction together. Attack, hurt, death
and idle offsets remain zero. NPCs sharing this resource ignore the corrections.

The player uses linear filtering for the downscaled painted body and inherited
equipment, plus a stable idle drawing to avoid the original four idle drawings'
proportion changes. No raster artwork was edited or generated. This improves
registration and playback, but does not add in-between drawings: locomotion still
has four poses and side/front/back art, with mirrored side views for west.

Run `tests/player_locomotion_tests.tscn` for logic and registration checks. For a
rendered comparison, run `tests/player_locomotion_visual.tscn` at 1280x720 with
optional user arguments `--output=<absolute-path-prefix>`, `--body-only`, or
`--travel`. Travel mode exercises eight-way starts/stops and turns in the actual
arena. `--write-movie <path.avi> --fixed-fps 60` records the harness. Captures and
clips for this pass are in `docs/verification/player-locomotion/`; in the
comparison movie, the original is on the left and the revised player on the right.

## Verification — 2026-10-20

Runtime suites passed with **2,206 assertions and zero failures**: framework
385, real imported assets 1,505, combat 75, movement 43, bandits 98, zombies 59,
and warband 41. Encounter and layout smoke checks also passed. Logs are
`docs/verification/modular-regression-*.log`.

Rendered inspection passed for all four workbench profiles, the bandit
encounter, and greatsword knockdown. Captures are
`docs/verification/modular-workbench-0.png` through `-3.png`,
`bandit-encounter.png`, and `greatsword-knockdown.png`. All 56 poses per kit were
reviewed in the per-family galleries. Soldier shoulder/armhole contours remain
imperfect at enlarged inspection scale; see the one-handed cleanup notes.

The Mono editor import completed, but its log reports a missing .NET SDK
10.0.4 and a safe-save error in this environment. Consequently the full
`tools/verify.ps1` wrapper is not reported as passing; the runtime suites above
were run directly after reimport. Godot also retains its existing shutdown
ObjectDB/resource warning. These did not prevent asset loading or native
rendered checks.
