extends Node
## Brief deterministic presentation fixture; forced proc is test-only.
func _ready() -> void:
	call_deferred("_capture")

func _capture() -> void:
	var arena := preload("res://game/arena.tscn").instantiate()
	arena.test_mode = true
	add_child(arena)
	for actor in arena.actors:
		actor.brain.set_physics_process(false)
		actor.intent = Vector2.ZERO
	arena.hero.position = Vector2(850, 540)
	arena.hero.facing = Vector2.RIGHT
	arena.actors[1].position = Vector2(1200, 720)
	arena.actors[2].position = Vector2(1350, 700)
	arena.actors[3].position = Vector2(945, 540)
	arena.camera.reset_smoothing()
	await get_tree().physics_frame
	if DisplayServer.get_name() != "headless":
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png("res://docs/verification/greatsword-proportions.png")
	var attack: AttackDefinition = arena.hero.heavy_attack.duplicate()
	var application: StatusApplication = attack.on_hit_effects[0].duplicate()
	application.chance = 1
	attack.on_hit_effects = [application]
	arena.hero.combat.request_attack(attack)
	await get_tree().create_timer(0.74).timeout
	if DisplayServer.get_name() != "headless":
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png("res://docs/verification/greatsword-knockdown.png")
	if not arena.actors[3].combat.statuses.has(&"knockdown"):
		push_error("Visual fixture failed to land knockdown")
		get_tree().quit(1)
		return
	print("GREATSWORD_VISUAL: attack and knockdown presented")
	get_tree().quit()
