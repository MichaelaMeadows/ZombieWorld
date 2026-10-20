extends Node
var failures := 0
var assertions := 0

func check(ok: bool, label: String) -> void:
	assertions += 1
	if not ok:
		failures += 1
		push_error("FAIL: " + label)

func make_actor() -> ArenaActor:
	var actor: ArenaActor = preload("res://game/actors/actor.tscn").instantiate()
	actor.player_controlled = true
	actor.loadout = preload("res://game/content/greatsword_loadout.tres")
	add_child(actor)
	actor.set_physics_process(false)
	actor.collision_layer = 0
	actor.collision_mask = 0
	return actor

func _ready() -> void:
	call_deferred("_run")

func _run() -> void:
	var actor := make_actor()
	actor.intent = Vector2.RIGHT
	actor.run_intent = true
	for i in 60:
		actor._physics_process(1.0 / 60)
	check(is_equal_approx(actor.combat.stamina, 80), "one second sprint costs 20 stamina without concurrent regeneration")
	check(actor.locomotion_state == "Run", "funded sprint uses run animation")
	actor.combat.stamina = 0.1
	actor._physics_process(1.0 / 60)
	check(actor.combat.stamina == 0, "sprint cannot make stamina negative")
	for i in 20:
		actor._physics_process(1.0 / 60)
	check(actor.max_speed == actor.walk_speed and actor.locomotion_state == "Walk", "exhaustion returns to walking while sprint remains held")
	for i in 50:
		actor._physics_process(1.0 / 60)
	check(actor.combat.stamina > 0 and actor.combat.stamina < 20 and actor.max_speed == actor.walk_speed, "stamina recovers without repeated exhausted sprint bursts")
	for i in 30:
		actor._physics_process(1.0 / 60)
	check(actor.max_speed == actor.run_speed, "sprint resumes after recovering its reserve")
	actor.combat.stamina = 100
	actor.combat.block_intent = true
	for i in 20:
		actor._physics_process(1.0 / 60)
	check(actor.combat.stamina == 100, "block plus sprint does not spend stamina")
	actor.combat.block_intent = false
	actor.intent = Vector2.ZERO
	for i in 20:
		actor._physics_process(1.0 / 60)
	check(actor.combat.stamina == 100, "holding sprint while idle is free")
	actor.intent = Vector2.RIGHT
	actor.combat.hit_pause = 0.5
	actor._physics_process(0.1)
	check(actor.combat.stamina == 100, "hit-stop does not charge sprint")
	actor.queue_free()

	for rate in [30, 60, 120]:
		actor = make_actor()
		var start := actor.position
		var travel := 0.0
		actor.run_intent = true
		check(actor.combat.request_dodge(Vector2.RIGHT), "dodge starts")
		check(actor.combat.stamina == 78 and not actor.combat.request_dodge(Vector2.RIGHT) and actor.combat.stamina == 78, "dodge charges once and rejects repeats")
		for i in ceili(0.22 * rate):
			actor._physics_process(1.0 / rate)
			travel += actor.velocity.length() / rate
		check(actor.combat.dodge_left == 0, "dodge ends within 220 ms at %d fps" % rate)
		check(absf(travel - 105.6) < 0.02, "dodge integrates to 106 px at %d fps" % rate)
		if rate == 60:
			check(absf(actor.position.distance_to(start) - 105.6) < 0.02, "physics body travels the authored dodge distance")
		var finish := actor.position
		actor._physics_process(1.0 / rate)
		check(actor.position.is_equal_approx(finish), "dodge has no lingering dash momentum")
		check(actor.combat.stamina == 78, "holding sprint during dodge adds no cost")
		actor.queue_free()

	actor = make_actor()
	for rig in [preload("res://game/content/modular/two_handed/hero.tres"), preload("res://game/content/modular/one_handed/warband.tres"), preload("res://game/content/modular/bow/archer.tres")]:
		actor.set_visual_rig(rig)
		for direction in [Vector2.RIGHT, Vector2.LEFT, Vector2.UP, Vector2.DOWN]:
			actor.combat.stamina = 100
			actor.combat.request_dodge(direction)
			actor.combat.tick(0.11)
			actor._update_sprite()
			check(actor.body_sprite.get_meta("animation") == "dodge" and actor.body_sprite.scale.y < rig.display_scale, "directional dodge crouches the equipped rig")
			var pose := actor.body_sprite.transform
			actor._update_sprite()
			check(actor.body_sprite.transform.is_equal_approx(pose), "resampling dodge never accumulates transforms")
			for layer in actor.visual_layer_nodes.values():
				check(layer.get_parent() == actor.body_sprite, "equipment inherits dodge movement")
			actor.combat.tick(0.12)
			actor._update_sprite()
			check(actor.body_sprite.rotation == 0 and actor.body_sprite.position == Vector2.ZERO and actor.body_sprite.scale.is_equal_approx(Vector2.ONE * rig.display_scale), "dodge restores normal presentation")
	actor.combat.stamina = 21
	check(not actor.combat.request_dodge(Vector2.RIGHT) and actor.combat.stamina == 21, "insufficient stamina rejects dodge without charging")
	actor.queue_free()
	await get_tree().process_frame
	print("DODGE_STAMINA_RESULT: %d assertions, %d failures" % [assertions, failures])
	get_tree().quit(1 if failures else 0)
