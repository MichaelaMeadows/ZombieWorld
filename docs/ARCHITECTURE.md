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
AttackDefinition Resources provide startup, active, recovery, damage, arc, range,
stagger, knockback, stamina and local hit-stop. Each actor duplicates authored
definitions before live tuning, avoiding changes to shared immutable content.
Damage is evaluated only in active frames, once per stable entity ID per swing,
with team filtering and a world-collision ray before damage delivery.

Typed combat signals feed presentation. Combat does not access HUD controls.
The arena is the small encounter composition root: local actor registry,
navigation, obstacles, sounds and presentation effects. The UI reads live actors.
No scene-tree scans are performed in per-frame NPC targeting.

## Navigation and identity

The authored 1536 × 1024 island uses a 48 × 32 AStarGrid2D derived from the same
prop rectangles as physics. Obstacles are inflated by actor radius; diagonal
corner-cutting is prohibited. A small static arena does not need asynchronous
navigation mesh baking. Decisions run every 180 ms with staggered initial offsets.
Local separation is bounded to the encounter roster.

Stable authored IDs exist for the hero and three opponents. Persistent world
records and a versioned save adapter are deliberately deferred. Upstream
DataManager uses scene paths for non-player identity and must not be directly
connected to future population saves.

## Scope and next boundary

This is the milestones 0–2 slice. Milestone 3 should extend the decision source
with allied target selection and order data, using the existing combat component.
Inventory, full menu framework, orders, ranged combat, simulation, persistence
and benchmark populations remain later milestones. No placeholder systems or
nonfunctional buttons have been added for them.
