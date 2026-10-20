extends Node
## One short native render to catch bad frame selection, scale and anchors.
func _ready() -> void:
	call_deferred("_capture")

func _capture() -> void:
	var arena := preload("res://game/bandit_arena.tscn").instantiate()
	arena.test_mode = true
	add_child(arena)
	for actor in arena.actors:
		actor.brain.set_physics_process(false)
		actor.set_physics_process(false)
	arena.hero.position = Vector2(940,540)
	var club: ArenaActor = arena.actors[1]
	var running: ArenaActor = arena.actors[2]
	var archer: ArenaActor = arena.actors[3]
	club.position = Vector2(1020,590)
	club.facing = Vector2(-1,-1).normalized()
	club.combat.request_attack(club.light_attack)
	club.combat.elapsed = club.light_attack.startup + .01
	running.position = Vector2(1070,720)
	running.velocity = Vector2.LEFT * 175
	running.locomotion_state = "Run"
	running.locomotion_phase = .25
	archer.position = Vector2(1170,430)
	archer.facing = archer.position.direction_to(arena.hero.position)
	archer.combat.request_attack(archer.light_attack)
	archer.combat.tick(archer.light_attack.startup + .01)
	archer.combat.resolve_hits(arena.actors)
	var arrow = get_tree().get_first_node_in_group("combat_projectiles")
	arrow.set_physics_process(false)
	arrow.position += arrow.direction * 90
	for actor in arena.actors:
		actor._update_sprite()
		actor.queue_redraw()
	arena.camera.reset_smoothing()
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png("res://docs/verification/bandit-encounter.png")
	print("BANDIT_VISUAL: captured")
	get_tree().quit()
