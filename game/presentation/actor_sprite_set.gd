class_name ActorSpriteSet
extends Resource
## Complete NPC animation library. Western views mirror the eastern artwork.
@export var motion_sheet: Texture2D
@export var combat_sheet: Texture2D
@export var motion_regions: Array[Rect2] = []
@export var motion_pivots: Array[Vector2] = []
@export var combat_regions: Array[Rect2] = []
@export var combat_pivots: Array[Vector2] = []
@export var motion_scale := 0.35
@export var combat_scale := 0.35
@export var diagonal_sheet: Texture2D
@export var diagonal_regions: Array[Rect2] = []
@export var diagonal_pivots: Array[Vector2] = []
@export var diagonal_scale := 0.35
var motion_atlas: RefCounted
var combat_atlas: RefCounted
var diagonal_atlas: RefCounted

func _ensure_atlases() -> void:
	if motion_atlas == null:
		var atlas = preload("res://game/presentation/frame_atlas.gd")
		motion_atlas = atlas.new(motion_sheet, motion_regions, motion_pivots)
		combat_atlas = atlas.new(combat_sheet, combat_regions, combat_pivots)
		if diagonal_sheet != null:
			diagonal_atlas = atlas.new(diagonal_sheet, diagonal_regions, diagonal_pivots)

func present(actor: Node2D) -> void:
	_ensure_atlases()
	var sprite: Sprite2D = actor.body_sprite
	sprite.position = Vector2.ZERO
	var aim: Vector2 = actor.combat.swing_direction if actor.combat.current != null else actor.facing
	var octant := posmod(roundi(aim.angle() / (PI * 0.25)), 8)
	var attack_row: int = [0, 1, 1, 1, 0, 3, 2, 3][octant]
	sprite.flip_h = aim.x < 0
	var frame := 0
	var row := 0
	var fighting := false
	var animation := "idle"
	if actor.dead:
		fighting = true
		row = 5
		frame = mini(3, int(actor.death_age / 0.85 * 4))
		animation = "death"
	elif actor.combat.current != null:
		fighting = true
		row = attack_row
		var attack: AttackDefinition = actor.combat.current
		var elapsed: float = actor.combat.elapsed
		frame = mini(1, int(elapsed / attack.startup * 2)) if elapsed < attack.startup else (2 if elapsed < attack.startup + attack.active else 3)
		animation = "attack"
	elif actor.combat.dodge_left > 0:
		var direction: Vector2 = actor.combat.dodge_direction
		sprite.flip_h = direction.x < 0
		var view := 0 if absf(direction.x) >= absf(direction.y) else (1 if direction.y > 0 else 2)
		row = 4 + view
		frame = mini(3, int(actor.combat.dodge_progress() * 4))
		animation = "dodge"
	elif actor.hurt_age < 0.24:
		fighting = true
		row = 4
		frame = mini(3, int(actor.hurt_age / 0.24 * 4))
		animation = "hurt"
	elif actor.locomotion_state != "Idle":
		var direction: Vector2 = actor.velocity
		sprite.flip_h = direction.x < 0
		var view := 0 if absf(direction.x) >= absf(direction.y) else (1 if direction.y > 0 else 2)
		row = 1 + view + (3 if actor.locomotion_state == "Run" else 0)
		frame = mini(3, int(actor.locomotion_phase * 4))
		animation = actor.locomotion_state.to_lower()
	elif octant != 0 and octant != 4:
		fighting = true
		row = attack_row
		frame = 3 # Recovered directional guard, weapon still in hand.
	else:
		frame = int(actor.age * 3) % 4
	if fighting and animation != "hurt" and animation != "death" and octant in [1, 3] and diagonal_atlas != null:
		diagonal_atlas.apply(sprite, frame, diagonal_scale)
	elif fighting:
		combat_atlas.apply(sprite, row * 4 + frame, combat_scale)
	else:
		motion_atlas.apply(sprite, row * 4 + frame, motion_scale)
	sprite.set_meta("animation", animation)
	sprite.set_meta("pose", Vector2i(frame, row))
