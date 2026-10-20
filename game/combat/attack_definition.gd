class_name AttackDefinition
extends Resource
## Single timeline drives both damage and the sprite adapter.
@export var label := "Light"
@export var startup := 0.12
@export var active := 0.10
@export var recovery := 0.22
@export var damage := 22
@export var reach := 64.0
@export var arc_degrees := 100.0
@export var stamina_cost := 14.0
@export var stagger := 0.18
@export var knockback := 135.0
@export var hit_stop := 0.045
@export var heavy := false
@export var on_hit_effects: Array[StatusApplication] = []
enum Delivery { MELEE, PROJECTILE }
@export var delivery := Delivery.MELEE
@export var projectile_speed := 500.0

func duration() -> float:
	return startup + active + recovery
