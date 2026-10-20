class_name CombatComponent
extends Node
## Player and NPC intents enter the same timeline and damage policy.
signal attack_started(definition: AttackDefinition)
signal impact(target: Node2D, amount: int, blocked: bool, heavy_hit: bool)
signal died
signal phase_changed(phase: String)

var actor: CharacterEntity
var current: AttackDefinition
var elapsed := 0.0
var phase := "Ready"
var stamina := 100.0
var max_stamina := 100.0
var regen_delay := 0.0
var stagger_left := 0.0
var invulnerability := 0.0
var dodge_left := 0.0
var dodge_direction := Vector2.RIGHT
var knockback_velocity := Vector2.ZERO
var block_intent := false
var hit_pause := 0.0
var dead := false
var swing_direction := Vector2.RIGHT
var hit_ids: Dictionary = {}
var queued: AttackDefinition
var buffer_left := 0.0
var last_swing_hit := false
var armor_resistance := false
var enabled := true

func _ready() -> void:
	actor = get_parent() as CharacterEntity

func is_active() -> bool:
	return current != null and elapsed >= current.startup and elapsed < current.startup + current.active

func can_act() -> bool:
	return enabled and not dead and current == null and dodge_left <= 0 and stagger_left <= 0

func is_blocking() -> bool:
	return can_act() and block_intent and stamina > 1

func request_attack(definition: AttackDefinition) -> bool:
	if dead or not enabled or definition == null:
		return false
	if not can_act():
		# Only buffer the final 130 ms of recovery, never chain through stagger.
		if current != null and current.duration() - elapsed <= 0.13:
			queued = definition
			buffer_left = 0.13
		return false
	if stamina < definition.stamina_cost or block_intent:
		return false
	stamina -= definition.stamina_cost
	regen_delay = 0.65
	current = definition
	elapsed = 0.0
	swing_direction = actor.facing
	hit_ids.clear()
	last_swing_hit = false
	_set_phase("Startup")
	attack_started.emit(definition)
	return true

func request_dodge(direction: Vector2) -> bool:
	if dead or not enabled or dodge_left > 0 or stagger_left > 0 or stamina < 22:
		return false
	# Recovery is cancelable; startup and active frames commit the attack.
	if current != null and elapsed < current.startup + current.active:
		return false
	current = null
	queued = null
	stamina -= 22
	regen_delay = 0.8
	dodge_left = 0.28
	invulnerability = 0.19
	dodge_direction = direction.normalized() if direction.length_squared() > 0.01 else actor.facing
	_set_phase("Dodge")
	return true

func tick(delta: float) -> void:
	if dead:
		return
	invulnerability = maxf(0, invulnerability - delta)
	dodge_left = maxf(0, dodge_left - delta)
	stagger_left = maxf(0, stagger_left - delta)
	regen_delay = maxf(0, regen_delay - delta)
	buffer_left = maxf(0, buffer_left - delta)
	knockback_velocity = knockback_velocity.move_toward(Vector2.ZERO, 800 * delta)
	if regen_delay <= 0 and current == null and dodge_left <= 0 and not block_intent:
		stamina = minf(max_stamina, stamina + delta * 27)
	if hit_pause > 0:
		hit_pause = maxf(0, hit_pause - delta)
		return # Local actor pause only. No Engine.time_scale changes.
	if current != null:
		elapsed += delta
		if elapsed >= current.duration():
			current = null
			_set_phase("Ready")
			if queued != null and buffer_left > 0:
				var next := queued
				queued = null
				request_attack(next)
		elif is_active():
			_set_phase("Active")
		elif elapsed >= current.startup:
			_set_phase("Recovery")
	else:
		_set_phase("Stagger" if stagger_left > 0 else ("Dodge" if dodge_left > 0 else ("Block" if is_blocking() else "Ready")))

func resolve_hits(candidates: Array) -> void:
	if not is_active() or hit_pause > 0 or not enabled:
		return
	for target in candidates:
		if not is_instance_valid(target) or target == actor or target.dead or target.team == actor.team:
			continue
		if hit_ids.has(target.entity_id):
			continue
		var offset: Vector2 = target.global_position - actor.global_position
		if offset.length() > current.reach + 12:
			continue
		if offset.length() > 0.1 and swing_direction.dot(offset.normalized()) < cos(deg_to_rad(current.arc_degrees * 0.5)):
			continue
		# A wall or prop interrupts the swing; never hit through obstacles.
		var ray := PhysicsRayQueryParameters2D.create(actor.global_position, target.global_position, 1)
		if not actor.get_world_2d().direct_space_state.intersect_ray(ray).is_empty():
			continue
		hit_ids[target.entity_id] = true
		var result: int = target.combat.receive_hit(current, actor)
		if result < 0:
			continue
		last_swing_hit = true
		hit_pause = current.hit_stop
		impact.emit(target, result, result == 0, current.heavy)

func receive_hit(definition: AttackDefinition, source: CharacterEntity) -> int:
	if dead or invulnerability > 0 or not enabled or source.team == actor.team:
		return -1
	var direction: Vector2 = source.global_position.direction_to(actor.global_position)
	if is_blocking() and actor.facing.dot(-direction) > 0.35 and stamina >= definition.stamina_cost:
		stamina -= definition.stamina_cost
		regen_delay = 0.7
		invulnerability = 0.08
		knockback_velocity = direction * 60
		return 0
	block_intent = false
	actor.health_controller.change_hp(-definition.damage, source.name)
	invulnerability = 0.12
	hit_pause = definition.hit_stop
	knockback_velocity = direction * definition.knockback
	if definition.heavy or not armor_resistance:
		stagger_left = definition.stagger
		current = null
		queued = null
		_set_phase("Stagger")
	if actor.health_controller.hp <= 0:
		dead = true
		current = null
		queued = null
		_set_phase("Dead")
		died.emit()
	return definition.damage

func _set_phase(value: String) -> void:
	if phase != value:
		phase = value
		phase_changed.emit(phase)
