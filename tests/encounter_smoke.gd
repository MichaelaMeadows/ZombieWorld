extends Node
## Bounded, reproducible combat replay and optional rendered evidence.
## Does not claim subjective combat-feel validation.
var arena: Node2D
var seconds := 0.0
var attack_wait := 0.0
var captured := false
var debug_captured := false
var samples: Array[float] = []
var output := "res://docs/verification"
var render := false
var physics_frames := 0
var initial_position := Vector2.ZERO
var victory := false
var max_ai_usec := 0
var actions := 0

func _ready() -> void:
	render = DisplayServer.get_name() != "headless"
	arena = preload("res://game/arena.tscn").instantiate()
	arena.test_mode = true
	add_child(arena)
	arena.hero.brain.set_physics_process(false)
	arena.hero.position = Vector2(870, 510)
	initial_position = arena.hero.position
	arena.camera.reset_smoothing()

func _physics_process(delta: float) -> void:
	physics_frames += 1
	seconds += delta
	attack_wait -= delta
	var hero: ArenaActor = arena.hero
	var target: ArenaActor
	var distance := INF
	for enemy in arena.actors:
		if enemy.player_controlled:
			continue
		max_ai_usec = maxi(max_ai_usec, enemy.brain.decision_usec)
		if not enemy.dead and hero.position.distance_to(enemy.position) < distance:
			target = enemy
			distance = hero.position.distance_to(enemy.position)
	if target != null and not hero.dead:
		var direction := hero.position.direction_to(target.position)
		if hero.combat.current == null:
			hero.facing = Vector2(signf(direction.x), 0) if absf(direction.x) > absf(direction.y) else Vector2(0, signf(direction.y))
		hero.intent = direction if distance > 61 else Vector2.ZERO
		if target.combat.current != null and target.combat.elapsed > target.combat.current.startup * 0.7 and distance < 95:
			if hero.combat.request_dodge(-direction):
				actions += 1
		elif distance < 72 and attack_wait <= 0:
			if hero.combat.request_attack(hero.heavy_attack if target.heavy else hero.light_attack):
				actions += 1
				attack_wait = 0.5
	else:
		hero.intent = Vector2.ZERO
		victory = not hero.dead
	if seconds > 1.2 and not captured:
		captured = true
		_capture("combat")
	if seconds > 5.0 and not debug_captured:
		debug_captured = true
		arena.hud.debug_panel.visible = true
		_capture("debug")
	if seconds >= 12:
		set_physics_process(false)
		_finish()

func _process(delta: float) -> void:
	samples.append(delta * 1000)

func _capture(label: String) -> void:
	if not render:
		return
	await RenderingServer.frame_post_draw
	var size := get_viewport().get_visible_rect().size
	get_viewport().get_texture().get_image().save_png("%s/%s-%d.png" % [output, label, size.x])

func _finish() -> void:
	samples.sort()
	var total := 0.0
	for sample in samples:
		total += sample
	var report := {
		"rendered": render,
		"physics_frames": physics_frames,
		"actions": actions,
		"impacts": arena.impacts,
		"hero_health": arena.hero.health_controller.hp,
		"victory": victory,
		"mean_frame_ms": total / maxi(1, samples.size()),
		"p95_frame_ms": samples[mini(samples.size() - 1, int(samples.size() * 0.95))],
		"max_ai_decision_us": max_ai_usec,
		"actor_count": arena.actors.size(),
		"renderer": RenderingServer.get_video_adapter_name()
	}
	print("ENCOUNTER_REPORT: ", JSON.stringify(report))
	var file := FileAccess.open(output + ("/encounter-rendered.json" if render else "/encounter-headless.json"), FileAccess.WRITE)
	file.store_string(JSON.stringify(report, "\t"))
	file.close()
	var success: bool = arena.impacts > 0 and actions > 0 and initial_position.distance_to(arena.hero.position) > 5
	arena.queue_free()
	await get_tree().process_frame
	get_tree().quit(0 if success else 1)
