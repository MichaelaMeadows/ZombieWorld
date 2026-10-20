extends Node2D
## Swept projectile: no tunneling, one impact, walls/allies stop the arrow.
var source_ref: WeakRef
var attack: AttackDefinition
var direction := Vector2.RIGHT
var remaining := 0.0
var spent := false

func configure(source: CharacterEntity, definition: AttackDefinition, aim: Vector2) -> void:
	source_ref = weakref(source)
	attack = definition.duplicate()
	direction = aim.normalized()
	remaining = attack.reach
	add_to_group("combat_projectiles")

func _physics_process(delta: float) -> void:
	if spent:
		return
	var source = source_ref.get_ref()
	if not is_instance_valid(source):
		queue_free()
		return
	var distance := minf(remaining, attack.projectile_speed * delta)
	var next := global_position + direction * distance
	var query := PhysicsRayQueryParameters2D.create(global_position, next, 3, [source.get_rid()])
	var result := get_world_2d().direct_space_state.intersect_ray(query)
	if not result.is_empty():
		global_position = result.position
		spent = true
		var target = result.collider
		if target is CharacterEntity and "combat" in target and target.team != source.team and not target.dead:
			var amount: int = target.combat.receive_hit(attack, source, direction)
			if amount >= 0:
				source.combat.last_swing_hit = true
				source.combat.impact.emit(target, amount, amount == 0, attack.heavy)
		queue_free()
		return
	global_position = next
	remaining -= distance
	if remaining <= 0:
		spent = true
		queue_free()
	queue_redraw()

func _draw() -> void:
	# The ground-plane collision stays at the feet; the shaft flies at hand height.
	draw_set_transform(Vector2(0, -30), direction.angle())
	draw_line(Vector2(-17, 0), Vector2(4, 0), Color("#372a22"), 3)
	draw_line(Vector2(-16, 0), Vector2(4, 0), Color("#c6a477"), 1)
	draw_colored_polygon(PackedVector2Array([Vector2(7,0), Vector2(0,-3), Vector2(0,3)]), Color("#dce4d9"))
	draw_line(Vector2(-16,-3), Vector2(-12,0), Color("#e4dcc7"), 2)
	draw_line(Vector2(-16,3), Vector2(-12,0), Color("#e4dcc7"), 2)
