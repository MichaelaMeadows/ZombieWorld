# Greatsword variant and reusable status effects

F5 now equips the greatsword. Open game/sword_arena.tscn with F6 to compare the
original sword loadout against the current encounter. Both use the same actors
and combat component. WeaponLoadout resources select the name, primary/secondary
attacks and two-handed presentation.

Greatsword primary (LMB): 38 damage, 102px reach, 360ms startup, 180ms active,
460ms recovery, 23 stamina. Secondary (Q): 60 damage, 114px reach, 650ms startup,
200ms active, 650ms recovery, 36 stamina. Both have a 30% knockdown chance per
accepted damaging hit on a surviving hostile. One roll per target per swing;
misses, blocked, friendly, invulnerable and lethal hits do not apply effects.

Knockdown interrupts attacks and buffered input, blocks attacks/block/dodge and
voluntary movement, and overrides light-stagger armor resistance. Enemies remain
vulnerable to follow-up damage. They lie prone then rise during the last 200ms,
regaining control at 1.0 seconds. Reapplication refreshes a full second without
stacking duplicate effects. Status duration advances on the actor physics clock,
including local hit-stop, and stops while the game is paused. Death clears effects.

## Extension points

- StatusEffectDefinition: stable ID, duration, action/movement permissions,
  interruption, movement multiplier, presentation tag and refresh policy.
- StatusApplication: definition plus independent probability; attach multiple
  applications to AttackDefinition.on_hit_effects.
- StatusComponent: runtime records keyed by effect ID, source entity IDs and
  remaining duration; typed applied/expired signals. It contains no scene nodes,
  UI calls, damage code or knockdown-specific gameplay checks.
- CombatComponent consumes effect permissions and interruptions. ArenaActor
  consumes movement modifiers and the optional `prone` presentation tag.
- Seedable RNG lives on the attacking combat component. Authored resources are
  never changed by proc rolls or runtime timers.

To add chill, author a definition with ID `chill`, duration 2 and movement
multiplier 0.5; attach a StatusApplication to an attack. No new combat branches.
Root can block movement while permitting attacks. Distinct movement modifiers
multiply; same-ID reapplication refreshes instead of stacking. Tests exercise
both examples. Damage-over-time, resistance/immunity tables and serialization
are future extensions, not claimed implemented.

Presentation uses Hero_Greatsword.png, a generated adult male character with
natural proportions, practical clothing and restrained fantasy styling.
The sword, hands and arms are painted together in every pose. There is no
separate orbiting weapon. Four wide columns contain idle/walk or attack phase frames;
the five rows contain idle, walk, right swing, down swing and up swing.
Left poses mirror right poses. Attack frames follow the same startup, active
and recovery clock as hit resolution. Art changes do not alter attack balance.
Measured AtlasTexture regions retain whole blades despite uneven generated
spacing. Per-pose foot anchors, mirrored with facing, prevent drifting between
frames; the standing body is approximately 70 world pixels tall.
See assets/art/Hero_Greatsword.prompt.txt for the generation and spacing prompts.

Verification: 75 combat/UI/status assertions, including seeded 10,000-trial
probability (3,028 successes), exact recovery boundaries, refreshed timers,
independent slow expiry, root semantics, blocked/dodged/friendly hits and death.
The short greatsword_visual scene forces one proc solely for presentation evidence.
