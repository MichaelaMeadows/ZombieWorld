class_name ActorArchetype
extends Resource
## Authored actor stats, weapon and presentation; runtime state stays on the actor.
@export var label := "Bandit"
@export var health := 80
@export var walk_speed := 110.0
@export var run_speed := 175.0
@export var walk_stride := 144.0
@export var run_stride := 168.0
@export var weapon: WeaponLoadout
@export var sprites: ActorSpriteSet
@export var sprite_rig: ModularSpriteRig
@export var ranged := false
@export var preferred_distance := 250.0
@export var attack_delay := 0.65
