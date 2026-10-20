extends Node
var failures := 0
var assertions := 0
var arena: Node2D

func check(ok: bool, label: String) -> void:
	assertions += 1
	if not ok:
		failures += 1
		push_error("FAIL: " + label)
	else:
		print("PASS: " + label)

func frames(count: int) -> void:
	for i in count:
		await get_tree().physics_frame

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	call_deferred("_run")

func reset_combat(actor: ArenaActor) -> void:
	actor.combat.current = null
	actor.combat.stagger_left = 0
	actor.combat.hit_pause = 0
	actor.combat.invulnerability = 0
	actor.combat.stamina = 100
	actor.combat.block_intent = false
	actor.combat.statuses.clear()
	actor.health_controller.max_hp = 100
	actor.health_controller.hp = 100

func _run() -> void:
	arena = preload("res://game/arena.tscn").instantiate()
	arena.test_mode = true
	add_child(arena)
	var hero: ArenaActor = arena.hero
	var enemy: ArenaActor = arena.actors[1]
	for actor in arena.actors:
		if actor != hero:
			actor.brain.set_physics_process(false)
			actor.set_physics_process(false)
			actor.position = Vector2(1400, 850)
	hero.position = Vector2(850, 540)
	await frames(3)
	Input.action_press("ui_right")
	var start := hero.position
	await frames(24)
	check(is_equal_approx(hero.velocity.length(), hero.walk_speed), "WASD reaches walking speed")
	check(hero.locomotion_state == "Walk" and hero.body_sprite.get_meta("animation") == "walk", "walking selects the dedicated cycle")
	var traveled := hero.position.distance_to(start)
	check(absf(hero.locomotion_phase - traveled / hero.walk_stride_length) < 0.025, "walking cadence follows distance traveled")
	check(hero.walk_stride_length / hero.walk_speed > 0.55 and hero.walk_stride_length / hero.walk_speed < 0.75, "player walk cadence matches the shorter painted stride")
	var walk_clip: String = hero.body_sprite.get_meta("clip")
	Input.action_press("aw_run")
	await frames(24)
	check(is_equal_approx(hero.velocity.length(), hero.run_speed), "Shift reaches running speed")
	check(hero.locomotion_state == "Run" and hero.body_sprite.get_meta("animation") == "run", "running selects its own poses")
	Input.action_press("ui_down")
	await frames(12)
	check(absf(hero.velocity.length() - hero.run_speed) < 0.01, "diagonal running has no speed bonus")
	Input.action_release("ui_right")
	await frames(12)
	check(walk_clip == "walk_e" and hero.body_sprite.get_meta("clip") == "run_s", "vertical movement uses front/back poses")
	Input.action_release("ui_down")
	Input.action_release("aw_run")
	await frames(12)
	check(hero.velocity.length() < 0.01 and hero.locomotion_state == "Idle", "stopping ends foot cycling")
	var stopped_phase := hero.locomotion_phase
	await frames(6)
	check(is_equal_approx(hero.locomotion_phase, stopped_phase), "idle retains the last foot phase without cycling")
	Input.action_press("ui_right")
	Input.action_press("aw_run")
	Input.action_press("aw_block")
	await frames(12)
	check(hero.locomotion_state != "Run" and hero.velocity.length() < hero.walk_speed, "block overrides run speed and poses")
	Input.action_release("aw_block")
	var root := StatusEffectDefinition.new()
	root.id = &"test_root"
	root.blocks_movement = true
	root.duration = 5
	hero.combat.statuses.apply(root, "test")
	start = hero.position
	await frames(12)
	check(hero.position.is_equal_approx(start) and hero.locomotion_state == "Idle", "root stops motion and foot cycling")
	hero.combat.statuses.clear()
	Input.action_release("ui_right")
	Input.action_release("aw_run")
	await frames(3)
	start = hero.position
	var phase := hero.locomotion_phase
	get_tree().paused = true
	await frames(3)
	check(hero.position == start and hero.locomotion_phase == phase, "pause freezes movement and animation")
	get_tree().paused = false
	hero.brain.set_physics_process(false)
	hero.set_physics_process(false)
	hero.velocity = Vector2.ZERO
	hero.position = Vector2(850, 540)
	var attack: AttackDefinition = hero.light_attack.duplicate()
	attack.arc_degrees = 20
	attack.damage = 1
	attack.on_hit_effects = []
	for direction in [Vector2(1,1).normalized(), Vector2(-1,1).normalized(), Vector2(-1,-1).normalized(), Vector2(1,-1).normalized()]:
		reset_combat(hero)
		reset_combat(enemy)
		enemy.position = hero.position + direction * 65
		hero.aim_at(enemy.global_position)
		check(hero.facing.is_equal_approx(direction), "player retains diagonal aim")
		hero.combat.request_attack(attack)
		hero.aim_at(hero.position - direction * 65)
		check(hero.combat.swing_direction.is_equal_approx(direction), "swing direction locks through the attack")
		hero.combat.elapsed = attack.startup + 0.01
		hero._update_sprite()
		check(hero.body_sprite.get_meta("clip") == ("attack_se" if direction.y > 0 else "attack_ne"), "player displays a diagonal strike")
		hero.combat.resolve_hits([enemy])
		check(enemy.health_controller.hp == 99, "narrow player swing hits on each diagonal")
		reset_combat(hero)
		reset_combat(enemy)
		enemy.position = hero.position - direction * 40
		enemy.brain.engaged = true
		enemy.brain.attack_wait = 0
		enemy.brain._decide()
		check(enemy.combat.current != null and enemy.combat.swing_direction.is_equal_approx(direction), "NPC decision aims at the target on each diagonal")
		enemy.combat.elapsed = enemy.combat.current.startup + 0.01
		enemy._update_sprite()
		check(enemy.body_sprite.get_meta("animation") == "diagonal_attack", "NPC displays a diagonal swipe")
		enemy.combat.resolve_hits([hero])
		check(hero.health_controller.hp == 88, "NPC diagonal swipe connects")
	reset_combat(hero)
	reset_combat(enemy)
	hero.aim_at(hero.position + Vector2(73, 41))
	check(hero.facing.is_equal_approx(Vector2(73,41).normalized()), "aim remains continuous between the eight visual directions")
	var saved_facing := hero.facing
	hero.aim_at(hero.position)
	check(hero.facing == saved_facing, "coincident aim preserves the last valid direction")
	enemy.combat.request_attack(enemy.light_attack)
	enemy.combat.swing_direction = Vector2.RIGHT
	enemy._update_sprite()
	check(enemy.body_sprite.hframes == 6 and enemy.body_sprite.vframes == 5, "NPC returns from diagonal atlas to cardinal sheet")
	arena.queue_free()
	await get_tree().process_frame
	print("MOVEMENT_RESULT: %d assertions, %d failures" % [assertions, failures])
	get_tree().quit(1 if failures else 0)
