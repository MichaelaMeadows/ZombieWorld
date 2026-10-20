extends Node
## Deterministic test scene. Use normal scene boot so upstream autoloads exist.
const ACTOR = preload("res://game/actors/actor.tscn")
var failures := 0
var assertions := 0
var fighters: Array[ArenaActor] = []
var arena: Node2D

func _ready() -> void:
	call_deferred("_run")

func check(condition: bool, description: String) -> void:
	assertions += 1
	if not condition:
		failures += 1
		push_error("FAIL: " + description)
	else:
		print("PASS: " + description)

func actor(id: String, team: int, pos: Vector2) -> ArenaActor:
	var a: ArenaActor = ACTOR.instantiate()
	a.entity_id = id
	a.name = id
	a.team = team
	a.player_controlled = team == 0
	a.position = pos
	get_tree().root.add_child(a)
	a.set_physics_process(false)
	fighters.append(a)
	return a

func reset_pair(a: ArenaActor, b: ArenaActor) -> void:
	for f in [a, b]:
		f.dead = false
		f.combat.dead = false
		f.health_controller.hp = f.health_controller.max_hp
		f.combat.current = null
		f.combat.stamina = 100
		f.combat.invulnerability = 0
		f.combat.hit_pause = 0
		f.combat.stagger_left = 0
		f.combat.dodge_left = 0
		f.combat.block_intent = false
		f.combat.queued = null
		f.combat.armor_resistance = false
		f.combat.statuses.clear()
	a.position = Vector2(300, 300)
	b.position = Vector2(350, 300)
	a.facing = Vector2.RIGHT
	b.facing = Vector2.LEFT
	b.team = 1

func step(a: ArenaActor, duration: float, hit: bool = false) -> void:
	for i in range(ceili(duration / 0.01)):
		a.combat.tick(0.01)
		if hit:
			a.combat.resolve_hits(fighters)

func _run() -> void:
	var a := actor("test_hero", 0, Vector2(300, 300))
	var b := actor("test_enemy", 1, Vector2(350, 300))
	await get_tree().physics_frame
	await get_tree().physics_frame
	reset_pair(a, b)
	var light := a.light_attack
	check(a.combat.request_attack(light), "light attack accepted from ready")
	check(is_equal_approx(a.combat.stamina, 86), "attack consumes authored stamina")
	check(not a.combat.request_attack(light), "cooldown rejects overlapping attack")
	step(a, 0.10, true)
	check(b.health_controller.hp == b.health_controller.max_hp, "startup cannot damage")
	step(a, 0.04, true)
	check(a.combat.is_active(), "hitbox activates on timeline")
	check(b.health_controller.hp == b.health_controller.max_hp - 22, "active arc applies authored damage")
	step(a, 0.12, true)
	check(b.health_controller.hp == b.health_controller.max_hp - 22, "same swing cannot hit twice")
	step(a, 0.12)
	check(not a.combat.is_active(), "hitbox deactivates in recovery")
	step(a, 0.3)
	check(a.combat.can_act(), "recovery returns to ready")
	check(a.combat.request_attack(light), "next attack accepted after recovery")

	reset_pair(a, b)
	b.team = 0
	a.combat.request_attack(light)
	step(a, 0.18, true)
	check(b.health_controller.hp == b.health_controller.max_hp, "friendly fire excluded")
	reset_pair(a, b)
	b.position = Vector2(250, 300)
	a.combat.request_attack(light)
	step(a, 0.18, true)
	check(b.health_controller.hp == b.health_controller.max_hp, "targets behind swing are excluded")
	reset_pair(a, b)
	b.position = Vector2(400, 300)
	a.combat.request_attack(light)
	step(a, 0.18, true)
	check(b.health_controller.hp == b.health_controller.max_hp, "out-of-range targets excluded")

	reset_pair(a, b)
	b.combat.request_dodge(Vector2.UP)
	check(is_equal_approx(b.combat.stamina, 78), "dodge costs stamina")
	check(b.combat.receive_hit(light, a) == -1, "dodge invulnerability rejects damage")
	step(b, 0.20)
	check(b.combat.receive_hit(light, a) == 22, "dodge immunity ends after 190 ms")
	reset_pair(a, b)
	b.combat.block_intent = true
	check(b.combat.receive_hit(light, a) == 0, "frontal block absorbs damage")
	check(b.health_controller.hp == b.health_controller.max_hp and b.combat.stamina == 86, "block drains stamina rather than health")
	reset_pair(a, b)
	b.combat.block_intent = true
	b.facing = Vector2.RIGHT
	check(b.combat.receive_hit(light, a) == 22, "block cannot protect the back")
	reset_pair(a, b)
	b.combat.stamina = 0
	check(not b.combat.request_attack(light) and not b.combat.request_dodge(Vector2.UP), "exhaustion prevents attacks and dodges")

	reset_pair(a, b)
	b.combat.request_attack(b.heavy_attack)
	b.combat.receive_hit(light, a)
	check(b.combat.current == null and b.combat.stagger_left > 0, "damage interrupts unarmored startup")
	reset_pair(a, b)
	b.combat.armor_resistance = true
	b.combat.request_attack(b.heavy_attack)
	b.combat.receive_hit(light, a)
	check(b.combat.current != null, "warden resists light stagger")
	step(b, 0.13)
	b.combat.receive_hit(a.heavy_attack, a)
	check(b.combat.current == null, "heavy hit interrupts armored startup")
	reset_pair(a, b)
	b.health_controller.hp = 10
	b.combat.receive_hit(light, a)
	check(b.dead and b.health_controller.hp == 0 and b.combat.dead, "lethal hit clamps health and enters death")
	check(not b.combat.request_attack(light), "dead actors cannot attack")
	reset_pair(a, b)
	a.combat.request_attack(light)
	step(a, 0.35)
	a.combat.request_attack(light)
	step(a, 0.10)
	check(a.combat.current != null and a.combat.elapsed < 0.1, "late input buffer starts next attack")
	reset_pair(a, b)
	a.combat.request_attack(light)
	check(not a.combat.request_dodge(Vector2.UP), "startup cannot be dodge-canceled")
	step(a, 0.25)
	check(a.combat.request_dodge(Vector2.UP), "recovery can be dodge-canceled")
	reset_pair(a, b)
	a.combat.hit_pause = 0.075
	b.combat.request_attack(light)
	step(b, 0.13)
	check(b.combat.is_active(), "localized hit-stop does not freeze another actor")

	reset_pair(a, b)
	var wall := StaticBody2D.new()
	wall.position = Vector2(325, 300)
	var collision := CollisionShape2D.new()
	var shape := RectangleShape2D.new()
	shape.size = Vector2(8, 100)
	collision.shape = shape
	wall.add_child(collision)
	get_tree().root.add_child(wall)
	await get_tree().physics_frame
	await get_tree().physics_frame
	a.combat.request_attack(light)
	step(a, 0.18, true)
	check(b.health_controller.hp == b.health_controller.max_hp, "solid obstacles block melee damage")
	wall.queue_free()
	await _test_statuses(a, b)
	for f in fighters:
		f.queue_free()
	fighters.clear()
	await get_tree().process_frame
	arena = preload("res://game/arena.tscn").instantiate()
	arena.test_mode = true
	get_tree().root.add_child(arena)
	await get_tree().physics_frame
	await get_tree().physics_frame
	check(arena.actors.size() == 4, "arena has one player and three autonomous melee opponents")
	check(arena.hero.loadout.two_handed and arena.hero.light_attack.reach == 102, "default arena equips greatsword")
	arena.hero.combat.current = arena.hero.light_attack
	arena.hero.combat.swing_direction = Vector2.UP
	arena.hero._update_sprite()
	check(arena.hero.body_sprite.get_meta("clip") == "attack_n", "upward greatsword swing uses the layered back-view animation")
	arena.hero.combat.current = null
	check(arena.find_path(Vector2(600, 540), Vector2(1000, 440)).size() > 0, "navigation routes through arena")
	check(arena.find_path(Vector2(-100, -100), Vector2(1000, 440)).is_empty(), "navigation rejects invalid endpoints safely")
	arena.hero.position = Vector2(900, 540)
	for enemy in arena.actors:
		if not enemy.player_controlled:
			enemy.brain._decide()
			check(enemy.brain.state == "Approach" or enemy.brain.state == "Attack", "enemy acquires nearby hostile: " + enemy.name)
	arena.hero.dead = true
	for enemy in arena.actors:
		if not enemy.player_controlled:
			enemy.brain._decide()
			check(enemy.brain.state == "Guard", "enemy recovers when target dies: " + enemy.name)
	arena.hero.dead = false
	arena.hud.open_pause()
	check(get_tree().paused and is_instance_valid(arena.hud.modal), "pause modal pauses gameplay")
	var resume := find_button(arena.hud.modal, "Resume")
	check(resume != null, "pause has a connected resume action")
	resume.pressed.emit()
	check(not get_tree().paused and arena.hud.modal == null, "resume button restores gameplay")
	arena.hud.open_pause()
	arena.hud.open_settings()
	check(get_tree().paused, "settings retains modal pause")
	arena.hud.close_modal()
	await get_tree().process_frame
	await get_tree().process_frame
	arena.hero.health_controller.hp = 47
	arena.hud._process(0)
	check(arena.hud.hp.value == 47, "HUD reflects live health")
	arena.hero.combat.current = null
	arena.hero.combat.stamina = 100
	var input := InputEventAction.new()
	input.action = "aw_light"
	input.pressed = true
	Input.parse_input_event(input)
	await get_tree().process_frame
	check(arena.hero.combat.current != null, "actual unhandled input reaches shared combat")
	var release: InputEventAction = input.duplicate()
	release.pressed = false
	Input.parse_input_event(release)
	arena.hero.combat.current = null
	arena.hud.open_pause()
	var before: float = arena.hero.combat.stamina
	input = InputEventAction.new()
	input.action = "aw_dodge"
	input.pressed = true
	Input.parse_input_event(input)
	await get_tree().process_frame
	check(arena.hero.combat.stamina == before and arena.hero.combat.dodge_left == 0, "modal suppresses gameplay input")
	release = input.duplicate()
	release.pressed = false
	Input.parse_input_event(release)
	arena.hud.close_modal()
	var prefs := ArenaPreferences.new()
	prefs.volume = 0.35
	prefs.camera_shake = false
	var path := "user://briarwatch_test_preferences.tres"
	check(ResourceSaver.save(prefs, path) == OK, "preferences serialize")
	var loaded := ResourceLoader.load(path, "", ResourceLoader.CACHE_MODE_REPLACE) as ArenaPreferences
	check(loaded.volume == 0.35 and not loaded.camera_shake, "preferences round trip")
	DirAccess.remove_absolute(path)
	arena.queue_free()
	await get_tree().process_frame
	print("RESULT: %d assertions, %d failures" % [assertions, failures])
	get_tree().quit(1 if failures else 0)

func _test_statuses(a: ArenaActor, b: ArenaActor) -> void:
	var sweep: AttackDefinition = preload("res://game/content/greatsword_cut.tres")
	var cleave: AttackDefinition = preload("res://game/content/greatsword_cleave.tres")
	check(sweep.damage > a.light_attack.damage and sweep.reach > a.light_attack.reach and sweep.duration() > a.light_attack.duration(), "greatsword trades speed for reach and damage")
	check(cleave.damage > a.heavy_attack.damage and cleave.duration() > a.heavy_attack.duration(), "greatsword secondary is heavier and slower")
	check(sweep.on_hit_effects[0].chance == 0.3 and cleave.on_hit_effects[0].chance == 0.3, "both greatsword attacks author 30 percent knockdown")
	var rng := RandomNumberGenerator.new()
	rng.seed = 82026
	var procs := 0
	for i in range(10000):
		if sweep.on_hit_effects[0].rolls_success(rng):
			procs += 1
	check(procs > 2800 and procs < 3200, "seeded probability distribution is near 30 percent (%d/10000)" % procs)
	var forced: AttackDefinition = sweep.duplicate()
	var application: StatusApplication = sweep.on_hit_effects[0].duplicate()
	application.chance = 1
	forced.on_hit_effects = [application]
	reset_pair(a, b)
	b.combat.armor_resistance = true
	b.combat.request_attack(b.heavy_attack)
	b.combat.receive_hit(forced, a)
	check(b.combat.statuses.has(&"knockdown") and b.combat.current == null, "knockdown interrupts even armored enemies")
	check(not b.combat.can_act() and not b.combat.request_dodge(Vector2.UP) and not b.combat.request_attack(a.light_attack), "knocked-down actor cannot attack or dodge")
	check(b.combat.statuses.blocks_movement(), "knockdown disables voluntary movement")
	b.combat.block_intent = true
	check(not b.combat.is_blocking(), "knockdown disables block")
	step(b, 0.79)
	b._update_sprite()
	check(absf(b.body_sprite.rotation) > 1.4, "knockdown presents a prone body")
	step(b, 0.20)
	check(b.combat.statuses.has(&"knockdown"), "enemy remains disabled through 990 ms")
	step(b, 0.01)
	b._update_sprite()
	check(not b.combat.statuses.has(&"knockdown") and b.combat.can_act() and b.body_sprite.rotation == 0, "enemy stands and can act at one second, including hit-stop")
	reset_pair(a, b)
	b.combat.statuses.apply(application.effect, a.entity_id)
	step(b, 0.7)
	b.combat.statuses.apply(application.effect, a.entity_id)
	check(b.combat.statuses.active.size() == 1, "reapplication refreshes without duplicate stacks")
	step(b, 0.99)
	check(b.combat.statuses.has(&"knockdown"), "refreshed effect lasts one second from reapplication")
	step(b, 0.01)
	check(not b.combat.statuses.has(&"knockdown"), "refreshed effect expires on time")
	reset_pair(a, b)
	b.combat.block_intent = true
	b.combat.receive_hit(forced, a)
	check(not b.combat.statuses.has(&"knockdown"), "blocked hits do not apply knockdown")
	reset_pair(a, b)
	b.combat.invulnerability = 0.19
	b.combat.receive_hit(forced, a)
	check(not b.combat.statuses.has(&"knockdown"), "invulnerable targets do not receive status")
	reset_pair(a, b)
	b.team = 0
	b.combat.receive_hit(forced, a)
	check(b.combat.statuses.active.is_empty(), "friendly hits do not apply statuses")
	reset_pair(a, b)
	application.chance = 0
	b.combat.receive_hit(forced, a)
	check(b.combat.statuses.active.is_empty(), "failed proc leaves target without status")
	application.chance = 1
	reset_pair(a, b)
	b.combat.statuses.apply(application.effect, a.entity_id)
	b.health_controller.hp = 1
	b.combat.receive_hit(forced, a)
	check(b.combat.dead and b.combat.statuses.active.is_empty(), "death clears statuses and does not reapply knockdown")
	reset_pair(a, b)
	var slow := StatusEffectDefinition.new()
	slow.id = &"chill"
	slow.label = "Chilled"
	slow.duration = 2.0
	slow.movement_multiplier = 0.5
	b.combat.statuses.apply(slow, a.entity_id)
	b.combat.statuses.apply(application.effect, a.entity_id)
	step(b, 1.0)
	check(b.combat.can_act() and b.combat.statuses.movement_scale() == 0.5, "independent slow survives knockdown without blocking actions")
	step(b, 1.0)
	check(b.combat.statuses.active.is_empty() and b.combat.statuses.movement_scale() == 1, "slow expires and restores movement")
	var root_effect := StatusEffectDefinition.new()
	root_effect.id = &"root"
	root_effect.blocks_movement = true
	b.combat.statuses.apply(root_effect, a.entity_id)
	check(b.combat.can_act() and not b.combat.request_dodge(Vector2.UP), "generic root permits attacks but prevents dodge movement")
	reset_pair(a, b)
	b.combat.request_dodge(Vector2.UP)
	b.combat.statuses.apply(root_effect, a.entity_id)
	var rooted_position := b.position
	b._physics_process(0.016)
	check(b.position.is_equal_approx(rooted_position), "root stops movement during an existing dodge")
	reset_pair(a, b)
	b.combat.statuses.apply(application.effect, a.entity_id)
	var remaining: float = b.combat.statuses.active[&"knockdown"].remaining
	b.set_physics_process(true)
	get_tree().paused = true
	await get_tree().process_frame
	await get_tree().process_frame
	check(b.combat.statuses.active[&"knockdown"].remaining == remaining, "status clock does not advance while gameplay is paused")
	b.set_physics_process(false)
	get_tree().paused = false
	reset_pair(a, b)
	b.position = Vector2(395, 300)
	await get_tree().physics_frame
	await get_tree().physics_frame
	a.combat.request_attack(forced)
	step(a, 0.4, true)
	check(b.health_controller.hp == b.health_controller.max_hp - 38 and b.combat.statuses.has(&"knockdown"), "extended-range swing delivers damage and status through real hit resolver")

func find_button(node: Node, text: String) -> Button:
	if node is Button and node.text == text:
		return node
	for child in node.get_children():
		var found := find_button(child, text)
		if found != null:
			return found
	return null
