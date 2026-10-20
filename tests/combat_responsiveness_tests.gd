extends Node
## Shared rules exercised against player, ally, zombie, brute, club and archer.
const ACTOR = preload("res://game/actors/actor.tscn")
var assertions := 0
var failures := 0

func check(ok: bool, label: String) -> void:
	assertions += 1
	if not ok:
		failures += 1
		push_error("FAIL: " + label)
	else:
		print("PASS: " + label)

func make_actor(id: String, team: int, kind: String) -> ArenaActor:
	var unit: ArenaActor = ACTOR.instantiate()
	unit.entity_id = id
	unit.name = id
	unit.team = team
	unit.player_controlled = kind == "player"
	unit.human = kind == "ally"
	unit.heavy = kind == "brute"
	unit.starting_health = 1000
	if kind == "club" or kind == "archer":
		unit.archetype = load("res://game/content/bandit_%s.tres" % kind)
	add_child(unit)
	unit.set_physics_process(false)
	return unit

func reset(unit: ArenaActor) -> void:
	unit.dead = false
	unit.combat.dead = false
	unit.health_controller.hp = unit.health_controller.max_hp
	unit.combat.current = null
	unit.combat.queued = null
	unit.combat.buffer_left = 0
	unit.combat.stamina = 100
	unit.combat.hit_pause = 0
	unit.combat.invulnerability = 0
	unit.combat.dodge_left = 0
	unit.combat.stagger_left = 0
	unit.combat.knockback_velocity = Vector2.ZERO
	unit.combat.block_intent = false
	unit.combat.statuses.clear()
	unit.combat.tick(0.001)
	unit.facing = Vector2.RIGHT
	unit.velocity = Vector2.ZERO
	unit.movement_velocity = Vector2.ZERO
	unit.intent = Vector2.ZERO
	unit.hurt_age = 1
	unit.flash_left = 0

func _ready() -> void:
	call_deferred("_run")

func _run() -> void:
	var source := make_actor("source", 2, "player")
	source.position = Vector2(200, 300)
	for kind in ["player", "ally", "zombie", "brute", "club", "archer"]:
		var unit := make_actor(kind, 0 if kind in ["player", "ally"] else 1, kind)
		var friend := make_actor(kind + "_friend", unit.team, "ally")
		unit.position = Vector2(300, 300)
		source.position = Vector2(340, 300)
		friend.position = Vector2(340, 310)
		await get_tree().physics_frame
		await get_tree().physics_frame
		reset(unit)
		var attack: AttackDefinition = unit.light_attack.duplicate()
		attack.delivery = AttackDefinition.Delivery.MELEE
		attack.damage = 1
		attack.reach = 70
		attack.on_hit_effects = []
		reset(source)
		check(unit.combat.request_attack(attack), kind + " accepts shared attack intent")
		unit.combat.tick(attack.startup * 0.5)
		unit.combat.resolve_hits([source, friend])
		check(source.health_controller.hp == 1000, kind + " startup does not hit")
		unit.combat.tick(attack.startup * 0.5 + 0.001)
		unit.combat.resolve_hits([source, friend])
		check(source.health_controller.hp == 999 and friend.health_controller.hp == 1000, kind + " hits hostile but not friend")
		unit.combat.tick(attack.hit_stop + 0.001)
		source.combat.invulnerability = 0
		unit.combat.resolve_hits([source, friend])
		check(source.health_controller.hp == 999, kind + " stable ID prevents duplicate hit after immunity clears")
		unit.combat.tick(attack.duration())
		check(unit.combat.can_act() and unit.combat.phase == "Ready", kind + " returns to ready after authored recovery")

		reset(unit)
		unit.combat.request_attack(attack)
		unit.combat.tick(attack.duration() - CombatComponent.INPUT_BUFFER)
		unit.aim_at(unit.position + Vector2.UP * 100)
		check(unit.combat.swing_direction == Vector2.RIGHT and unit.facing == Vector2.UP, kind + " recovery aims next action without turning old swing")
		check(not unit.combat.request_attack(attack) and unit.combat.queued == attack, kind + " buffers at exact recovery boundary")
		unit.combat.hit_pause = 0.045
		unit.combat.tick(0.04)
		check(is_equal_approx(unit.combat.buffer_left, CombatComponent.INPUT_BUFFER), kind + " hit-stop preserves queued intent")
		unit.combat.tick(0.025)
		for i in 6:
			unit.combat.tick(0.02)
		check(unit.combat.current == attack and unit.combat.elapsed < 0.02 and unit.combat.swing_direction == Vector2.UP, kind + " boundary-crossing frame consumes buffer once with new aim")
		check(unit.combat.queued == null and is_equal_approx(unit.combat.stamina, 100 - attack.stamina_cost * 2), kind + " queued attack charges stamina exactly once")

		reset(unit)
		var impact := AttackDefinition.new()
		impact.heavy = true
		impact.damage = 1
		impact.hit_stop = 0.045
		impact.stagger = 0.18
		impact.knockback = 135
		unit.combat.request_attack(attack)
		unit.combat.tick(attack.duration() - 0.10)
		unit.combat.request_attack(attack)
		unit.combat.receive_hit(impact, source, Vector2.LEFT)
		check(unit.combat.current == null and unit.combat.queued == null, kind + " stagger cancels committed and queued attacks")
		unit.combat.tick(0.03)
		check(is_equal_approx(unit.combat.stagger_left, 0.18) and unit.combat.knockback_velocity == Vector2.LEFT * 135, kind + " reaction survives local hit-stop")
		check(not unit.combat.can_act() and not unit.combat.request_dodge(Vector2.UP), kind + " cannot act through impact pause")
		unit._update_sprite()
		check(unit.body_sprite.rotation > 0 and unit.flash_left > 0, kind + " directional recoil and hit flash share damage signal")
		unit.combat.tick(0.025)
		check(is_equal_approx(unit.combat.stagger_left, 0.17) and is_equal_approx(unit.combat.motion_delta, 0.01), kind + " partial hit-stop frame uses only unpaused time")
		var start := unit.position
		unit._physics_process(1.0 / 60)
		check(unit.position.x < start.x - 1 and absf(unit.position.y - start.y) < 0.01, kind + " recoil moves away from impact without input")
		unit.combat.tick(0.3)
		check(unit.combat.can_act() and unit.combat.queued == null, kind + " finishes stagger without ghost attack")
		reset(unit)
		unit.facing = Vector2.RIGHT
		unit.combat.block_intent = true
		unit.combat.receive_hit(impact, source, Vector2.LEFT)
		check(unit.health_controller.hp == 1000 and unit.flash_left > 0 and unit.hurt_age == 1, kind + " block has feedback without damage animation")
		reset(unit)
		unit.health_controller.hp = 900
		unit.health_controller.change_hp(1, "test_heal")
		check(unit.flash_left == 0 and unit.hurt_age == 1, kind + " healing does not trigger a hit reaction")
		unit.queue_free()
		friend.queue_free()
		await get_tree().process_frame

	reset(source)
	var short := AttackDefinition.new()
	short.recovery = 0.01
	source.combat.request_attack(short)
	source.combat.tick(short.startup + 0.01)
	source.combat.request_attack(short)
	check(source.combat.queued == null, "short recovery never permits buffering during active frames")
	source.combat.tick(short.active)
	check(source.combat.current == null, "short attack still reaches ready")
	reset(source)
	source.combat.request_attack(short)
	source.combat.hit_pause = 0.01
	source.combat.tick(0.016)
	check(is_equal_approx(source.combat.elapsed, 0.006), "attacker timeline preserves sub-frame hit-stop remainder")
	check(is_equal_approx(Engine.time_scale, 1), "shared impact never changes global time scale")
	for rate in [30, 60, 120]:
		reset(source)
		var timed := AttackDefinition.new()
		source.combat.request_attack(timed)
		var total := 0.0
		var active_seen := false
		for frame in ceili(timed.duration() * rate) + 1:
			source.combat.tick(1.0 / rate)
			total += 1.0 / rate
			active_seen = active_seen or source.combat.is_active()
			if total < timed.startup:
				check(not source.combat.is_active(), "no premature active phase at %d Hz" % rate)
		check(active_seen and source.combat.can_act(), "complete valid timeline at %d Hz" % rate)
	source.queue_free()
	await get_tree().process_frame
	await _test_ai()
	print("RESPONSIVENESS_RESULT: %d assertions, %d failures" % [assertions, failures])
	get_tree().quit(1 if failures else 0)

func _test_ai() -> void:
	var arena: Node2D = preload("res://game/bandit_arena.tscn").instantiate()
	arena.test_mode = true
	add_child(arena)
	arena.set_process(false)
	for actor: ArenaActor in arena.actors:
		actor.set_physics_process(false)
		actor.brain.set_physics_process(false)
		actor.position = Vector2(1400, 900)
	var hero: ArenaActor = arena.hero
	hero.position = Vector2(900, 540)
	for unit: ArenaActor in [arena.actors[1], arena.actors[3]]:
		reset(unit)
		unit.position = Vector2(950 if not unit.archetype.ranged else 1100, 540)
		await get_tree().physics_frame
		await get_tree().physics_frame
		unit.brain.engaged = true
		unit.brain.attack_wait = 0
		unit.brain._decide()
		check(unit.combat.current != null, unit.name + " begins authored AI attack")
		var wait: float = unit.brain.attack_wait
		unit.brain._physics_process(0.1)
		check(is_equal_approx(unit.brain.attack_wait, wait), unit.name + " tactical delay does not drain during committed attack")
		unit.brain.decision_left = 0.18
		unit.combat.tick(unit.light_attack.duration() + 0.001)
		check(unit.brain.decision_left == 0, unit.name + " recovery wakes AI immediately")
		unit.brain.attack_wait = 0.005
		unit.brain.decision_left = 0.18
		unit.brain._physics_process(0.016)
		check(unit.combat.current != null, unit.name + " attacks as cooldown ends without waiting for next scan")
		reset(unit)
		unit.brain.path = PackedVector2Array([unit.position])
		unit.brain.decision_left = 0.18
		unit.brain.attack_wait = 1
		unit.intent = Vector2.RIGHT
		unit.brain._physics_process(0.016)
		check(unit.intent == Vector2.ZERO, unit.name + " stops stale movement at end of path")
		var decisions: int = unit.brain.decisions
		for frame in 5:
			unit.brain._physics_process(0.016)
		check(unit.brain.decisions == decisions, unit.name + " idle decisions remain throttled")
		unit.position = Vector2(1400, 900)
	arena.queue_free()
	await get_tree().process_frame
