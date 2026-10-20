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

func _ready() -> void:
	actor = get_parent()
	decision_left = float(abs(actor.entity_id.hash()) % 180) / 1000.0
	attack_wait += decision_left

func _physics_process(delta: float) -> void:
	if actor.dead:
		state = "Dead"
		actor.intent = Vector2.ZERO
		return
	attack_wait -= delta
	decision_left -= delta
	if decision_left <= 0:
		var start := Time.get_ticks_usec()
		_decide()
		decision_usec = Time.get_ticks_usec() - start
		decisions += 1
		decision_left = 0.18
	if actor.combat.can_act() and not path.is_empty():
		if actor.position.distance_to(path[0]) < 16:
			path.remove_at(0)
		if not path.is_empty():
			actor.intent = actor.position.direction_to(path[0])
	else:
		actor.intent = Vector2.ZERO
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
	var target: ArenaActor = actor.arena.hero
	if not is_instance_valid(target) or target.dead:
		state = "Guard"
		path.clear()
		return
	var offset := target.position - actor.position
	if offset.length() < 290 or target.position.x > 815:
		engaged = true
	if not engaged:
		state = "Guard"
		return
	if not actor.combat.can_act():
		state = "Recover"
		path.clear()
		return
	actor.facing = Vector2(signf(offset.x), 0) if absf(offset.x) > absf(offset.y) else Vector2(0, signf(offset.y))
	var attack: AttackDefinition = actor.heavy_attack if actor.heavy else actor.light_attack
	if offset.length() < attack.reach + 3:
		path.clear()
		state = "Attack"
		if attack_wait <= 0:
			actor.combat.request_attack(attack)
			attack_wait = attack.duration() + (0.65 if actor.heavy else 0.42)
	else:
		state = "Approach"
		path = actor.arena.find_path(actor.position, target.position)
