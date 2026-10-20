# Architecture

The project is an additive extension of the MIT Godot 2D Top-Down Template.
Original entities/, components/, scripts/, scenes/ and addons/ remain intact.
The only baseline repair changes the project icon UID to a resource path.

## Runtime boundary

`game/actors/actor.tscn` is the shared actor scene. `ArenaActor` subclasses upstream
`CharacterEntity`, retaining its acceleration/friction movement and facing API.
It overrides lifecycle hooks which assume the template's AnimationTree, Jumper
and fall detector. Tiny Swords uses a Sprite2D adapter instead.

`ArenaHealth` subclasses `HealthController`. It retains health clamping, events
and hooks, but disables its independent recovery timer: `CombatComponent` owns
invulnerability on the same physics clock as attacks and dodges.

PlayerInput and MeleeBrain only issue intent to the same CombatComponent.
Both call ArenaActor.aim_at for continuous target-facing; CombatComponent snapshots
that vector for each swing. Locomotion uses distance-driven playback and authored
walk/run stride lengths. FrameAtlas caches generated pose textures and anchors;
animation selection is separate from the exact direction used by hit detection.
AttackDefinition Resources provide startup, active, recovery, damage, arc, range,
stagger, knockback, stamina and local hit-stop. Each actor duplicates authored
definitions before live tuning, avoiding changes to shared immutable content.
Damage is evaluated only in active frames, once per stable entity ID per swing,
with team filtering and a world-collision ray before damage delivery.

Typed combat signals feed presentation. Combat does not access HUD controls.
WeaponLoadout resources select primary/secondary attacks and presentation.
AttackDefinition accepts probabilistic StatusApplication resources; a node-free
StatusComponent owns independent effect clocks keyed by stable IDs. Combat and
movement consume generic permissions/modifiers, while presentation consumes
optional effect tags. Knockdown, slow and root share this path. See
STATUS_EFFECTS.md for lifecycle rules and extension points.

The arena is the small encounter composition root: local actor registry,
navigation, obstacles, sounds and presentation effects. The UI reads live actors.
No scene-tree scans are performed in per-frame NPC targeting.

## Navigation and identity

The authored 1536 × 1024 island uses a 48 × 32 AStarGrid2D derived from the same
prop rectangles as physics. Obstacles are inflated by actor radius; diagonal
corner-cutting is prohibited. A small static arena does not need asynchronous
navigation mesh baking. Decisions run every 180 ms with staggered initial offsets.
Local separation is bounded to the encounter roster.

Stable authored IDs exist for the hero and three opponents. These sandbox IDs
remain independent of the campaign. Upstream DataManager uses scene paths for
non-player identity and is not connected to population saves.

## Scope and next boundary

This is the milestones 0–2 slice. Milestone 3 should extend the decision source
with allied target selection and order data, using the existing combat component.
Inventory, full menu framework, orders, ranged combat, simulation, persistence
and benchmark populations remain later milestones. No placeholder systems or
nonfunctional buttons have been added for them.

## Living world migration — 2026-10-20

The deferred simulation/persistence statement above describes the original combat
slice. The project now starts at `game/world/world.tscn`, with an independent
`CampaignSession` autoload owning the imported DarkLineage C++ simulation through
GDExtension. The upstream portable rules remain unchanged; no Unreal runtime or
battle implementation is required.

The core owns geography, populations, genealogy, items, economy, politics and
campaign time. Godot provides read-only projections and validated item commands.
A worker owns the core exclusively during time advancement; the main thread
publishes its completed result. Geometry is cached separately from dynamic views.
New Godot checkpoints preserve full native state and a previous-generation backup.

The arena reads the current campaign warband on entry and creates one human AI
ally per troop, retaining its campaign ID. Recruitment/dismissal use native
commands; the arena only reads that roster. Human presentation is independent
of player/AI control. Both sides choose hostile targets from the local roster.
Campaign item bonuses do not
modify sandbox combat, and no battle outcomes feed the campaign. Legacy save
conversion and specific DarkLineage battle scenarios are outside this migration.
See [World systems](WORLD_SYSTEMS.md) for implementation, controls and verification.

## Modular humanoid presentation

`ModularSpriteRig` selects one pose for the whole character from the existing
combat clock or distance-driven locomotion phase. `ModularSpriteLayer` supplies
the corresponding body, armor, weapon or foreground frame with a common foot
origin. Authored per-frame registration handles gear pivots, scale, rotation
and local ordering. Every layer remains in its actor's Y-sort group.

Rigs and atlas textures are shared resources; equipment overrides and render
nodes are owned by each ArenaActor. Swaps validate family, body rig ID, frame
coverage and coordinate dimensions. Archetypes and loadouts can select a rig;
actors without one retain their existing complete-body presentation. This
includes shamblers and brutes, whose anatomy does not need humanoid equipment.

`game/equipment_workbench.tscn` previews kits and equipment removal independently
of campaign saves. See [Modular sprites](MODULAR_SPRITES.md) for the asset
contract, authoring instructions, compatibility rules and verification.

Bandit actors use `ActorArchetype` resources for stats, loadouts and an
`ActorSpriteSet` animation adapter. The adapter references native atlas regions
and uses the shared movement phase, combat timeline and status presentation.
`AttackDefinition.delivery` selects melee or a swept arrow projectile; projectile
impacts enter the same `CombatComponent.receive_hit` policy as melee.
The separate `game/bandit_arena.tscn` demonstrates these reusable profiles.
See [Bandits](BANDITS.md) for assets, state mapping and tuning.
