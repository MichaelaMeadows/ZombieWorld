extends Node
## Brief in-engine evidence, with no native UI automation.
func _ready() -> void:
	call_deferred("_capture")

func snapshot(name: String) -> void:
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png("res://docs/verification/" + name + ".png")

func _capture() -> void:
	var arena := preload("res://game/arena.tscn").instantiate()
	arena.test_mode = true
	add_child(arena)
	for actor in arena.actors:
		actor.brain.set_physics_process(false)
		if actor != arena.hero:
			actor.set_physics_process(false)
	var hero: ArenaActor = arena.hero
	hero.position = Vector2(850, 540)
	hero.intent = Vector2.RIGHT
	hero.facing = Vector2.RIGHT
	arena.camera.reset_smoothing()
	await get_tree().create_timer(0.40).timeout
	await snapshot("player-walk")
	hero.run_intent = true
	await get_tree().create_timer(0.45).timeout
	await snapshot("player-run")
	hero.set_physics_process(false)
	hero.combat.request_dodge(Vector2.RIGHT)
	hero.combat.tick(CombatComponent.DODGE_DURATION * 0.5)
	hero._update_sprite()
	await snapshot("player-dodge")
	hero.combat.tick(CombatComponent.DODGE_DURATION)
	hero.intent = Vector2.ZERO
	hero.velocity = Vector2.ZERO
	hero.position = Vector2(850, 540)
	var enemy: ArenaActor = arena.actors[1]
	enemy.position = Vector2(920, 610)
	for actor in [hero, enemy]:
		actor.combat.current = actor.light_attack
		actor.combat.elapsed = actor.light_attack.startup + 0.01
		actor.combat.swing_direction = Vector2(1,1).normalized() if actor == hero else Vector2(-1,-1).normalized()
		actor._update_sprite()
		actor.queue_redraw()
	arena.camera.reset_smoothing()
	await snapshot("diagonal-attacks")
	print("MOVEMENT_VISUAL: walk, run, dodge and diagonal strikes captured")
	get_tree().quit()
