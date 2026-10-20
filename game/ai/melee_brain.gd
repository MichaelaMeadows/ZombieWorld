extends Node
## Bounded tactical layer; one local encounter registry, staggered decisions.
var actor: ArenaActor
var decision_left := 0.0
var attack_wait := 0.5
var state := "Guard"
var path := PackedVector2Array()
var engaged := false
var previous_position := Vector2.ZERO
var stuck_time := 0.0
var decisions := 0
var decision_usec := 0
var target: ArenaActor

func _ready() -> void:
	actor = get_parent()
	decision_left = float(abs(actor.entity_id.hash()) % 180) / 1000.0
	attack_wait += decision_left
	actor.combat.phase_changed.connect(_on_combat_phase)

func _on_combat_phase(next: String) -> void:
	if next == "Ready":
		decision_left = 0 # React on recovery without increasing the idle scan rate.

func _physics_process(delta: float) -> void:
	if actor.dead:
		state = "Dead"
		actor.intent = Vector2.ZERO
		return
	var previous_wait := attack_wait
	if actor.combat.can_act():
		attack_wait = maxf(0, attack_wait - delta)
		if previous_wait > 0 and attack_wait <= 0:
			decision_left = 0
	decision_left -= delta
	if is_instance_valid(target) and not target.dead:
		actor.aim_at(target.global_position)
	if decision_left <= 0:
		var start := Time.get_ticks_usec()
		_decide()
		decision_usec = Time.get_ticks_usec() - start
		decisions += 1
		decision_left = 0.18
	actor.intent = Vector2.ZERO
	if actor.combat.can_act() and not path.is_empty():
		if actor.position.distance_to(path[0]) < 16:
			path.remove_at(0)
		if not path.is_empty():
			actor.intent = actor.position.direction_to(path[0])
	if actor.intent != Vector2.ZERO and actor.position.distance_to(previous_position) < 0.2:
		stuck_time += delta
		if stuck_time > 0.6:
			path.clear()
			decision_left = 0
			stuck_time = 0
	else:
		stuck_time = 0
	previous_position = actor.position

func _decide() -> void:
	actor.run_intent = false
	target = null
	var nearest := INF
	for candidate: ArenaActor in actor.arena.actors:
		if candidate == actor or candidate.dead or candidate.team == actor.team:
			continue
		var distance := actor.position.distance_squared_to(candidate.position)
		if distance < nearest:
			nearest = distance
			target = candidate
	if target == null:
		_follow_or_guard()
		return
	var offset := target.position - actor.position
	if offset.length() < 290 or actor.arena.hero.position.x > 815:
		engaged = true
	if not engaged:
		_follow_or_guard()
		return
	if not actor.combat.can_act():
		state = "Recover"
		path.clear()
		return
	actor.aim_at(target.global_position)
	var attack: AttackDefinition = actor.heavy_attack if actor.heavy else actor.light_attack
	if offset.length() <= attack.reach + 8:
		path.clear()
		state = "Attack"
		if attack_wait <= 0:
			if actor.combat.request_attack(attack):
				attack_wait = actor.archetype.attack_delay if actor.archetype != null else (0.30 if actor.heavy else 0.22)
	else:
		state = "Approach"
		actor.run_intent = actor.archetype != null and offset.length() > 150
		path = actor.arena.find_path(actor.position, target.position)
		# The 32px navigation grid is too coarse for hand range. Finish the
		# approach at the target's actual position when the final segment is clear.
		if offset.length() < 96:
			var ray := PhysicsRayQueryParameters2D.create(actor.global_position, target.global_position, 1)
			if actor.get_world_2d().direct_space_state.intersect_ray(ray).is_empty():
				path = PackedVector2Array([target.position])

func _follow_or_guard() -> void:
	path.clear()
	state = "Guard"
	var leader: ArenaActor = actor.arena.hero
	if actor.team != leader.team or leader.dead or not actor.combat.can_act():
		return
	var destination := leader.position + actor.follow_offset
	if actor.position.distance_to(destination) > 40:
		state = "Follow"
		path = actor.arena.find_path(actor.position, destination)
