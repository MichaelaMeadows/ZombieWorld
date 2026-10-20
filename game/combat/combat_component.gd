class_name CombatComponent
extends Node
## Player and NPC intents enter the same timeline and damage policy.
signal attack_started(definition: AttackDefinition)
signal impact(target: Node2D, amount: int, blocked: bool, heavy_hit: bool)
signal died
signal phase_changed(phase: String)
signal hit_received(direction: Vector2, blocked: bool)

const INPUT_BUFFER := 0.13
const DODGE_DURATION := 0.22
const DODGE_SPEED := 480.0
const DODGE_COST := 22.0
const SPRINT_DRAIN := 20.0
const SPRINT_RECOVERY := 20.0

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
var projectile_released := false
var statuses := StatusComponent.new()
var status_rng := RandomNumberGenerator.new()
var motion_delta := 0.0
var sprint_exhausted := false

func _ready() -> void:
	actor = get_parent() as CharacterEntity
	status_rng.randomize()
	statuses.applied.connect(_on_status_applied)

func is_active() -> bool:
	return not statuses.blocks_actions() and current != null and elapsed >= current.startup and elapsed < current.startup + current.active

func can_act() -> bool:
	return enabled and not dead and hit_pause <= 0 and not statuses.blocks_actions() and current == null and dodge_left <= 0 and stagger_left <= 0

func is_blocking() -> bool:
	return can_act() and block_intent and stamina > 1

func can_sprint() -> bool:
	if stamina >= SPRINT_RECOVERY:
		sprint_exhausted = false
	return can_act() and not block_intent and not statuses.blocks_movement() and stamina > 0 and not sprint_exhausted

func spend_sprint(delta: float) -> void:
	stamina = maxf(0, stamina - SPRINT_DRAIN * delta)
	regen_delay = 0.8
	if stamina <= 0:
		sprint_exhausted = true

func dodge_progress() -> float:
	return clampf(1.0 - dodge_left / DODGE_DURATION, 0.0, 1.0)

func request_attack(definition: AttackDefinition) -> bool:
	if dead or not enabled or definition == null or statuses.blocks_actions():
		return false
	if not can_act():
		# Only buffer the final 130 ms of recovery, never chain through stagger.
		if current != null and elapsed >= current.startup + current.active and current.duration() - elapsed <= INPUT_BUFFER + 0.000001:
			queued = definition
			buffer_left = INPUT_BUFFER
		return false
	if stamina < definition.stamina_cost or block_intent:
		return false
	stamina -= definition.stamina_cost
	regen_delay = 0.65
	current = definition
	queued = null
	buffer_left = 0
	elapsed = 0.0
	swing_direction = actor.facing.normalized() if not actor.facing.is_zero_approx() else Vector2.RIGHT
	hit_ids.clear()
	last_swing_hit = false
	projectile_released = false
	_set_phase("Startup")
	attack_started.emit(definition)
	return true

func request_dodge(direction: Vector2) -> bool:
	if dead or not enabled or hit_pause > 0 or statuses.blocks_actions() or statuses.blocks_movement() or dodge_left > 0 or stagger_left > 0 or stamina < DODGE_COST:
		return false
	# Recovery is cancelable; startup and active frames commit the attack.
	if current != null and elapsed < current.startup + current.active:
		return false
	current = null
	queued = null
	buffer_left = 0
	stamina -= DODGE_COST
	regen_delay = 0.8
	dodge_left = DODGE_DURATION
	invulnerability = 0.19
	dodge_direction = direction.normalized() if direction.length_squared() > 0.01 else actor.facing
	_set_phase("Dodge")
	return true

func tick(delta: float) -> void:
	motion_delta = 0
	if dead:
		return
	statuses.tick(delta) # Physics time, independent of local hit-stop; paused with actor.
	invulnerability = maxf(0, invulnerability - delta)
	regen_delay = maxf(0, regen_delay - delta)
	if regen_delay <= 0 and current == null and dodge_left <= 0 and not block_intent:
		stamina = minf(max_stamina, stamina + delta * 27)
	# Preserve the reaction during local hit-stop and use only the unpaused
	# portion of a boundary frame. Status durations/immunity still use world time.
	var paused := minf(hit_pause, delta)
	hit_pause = maxf(0, hit_pause - delta)
	var action_delta := delta - paused
	motion_delta = action_delta
	if action_delta <= 0:
		return
	dodge_left = maxf(0, dodge_left - action_delta)
	stagger_left = maxf(0, stagger_left - action_delta)
	knockback_velocity = knockback_velocity.move_toward(Vector2.ZERO, 800 * action_delta)
	if current != null:
		var remaining := current.duration() - elapsed
		elapsed += action_delta
		if elapsed >= current.duration():
			current = null
			_set_phase("Ready")
			var next := queued if queued != null and buffer_left + 0.000001 >= remaining else null
			queued = null
			buffer_left = 0
			if next != null:
				request_attack(next)
		elif is_active():
			_set_phase("Active")
		elif elapsed >= current.startup:
			_set_phase("Recovery")
	else:
		_set_phase(statuses.action_label() if statuses.blocks_actions() else ("Stagger" if stagger_left > 0 else ("Dodge" if dodge_left > 0 else ("Block" if is_blocking() else "Ready"))))
	if queued != null:
		buffer_left = maxf(0, buffer_left - action_delta)
		if buffer_left <= 0:
			queued = null

func resolve_hits(candidates: Array) -> void:
	if not is_active() or hit_pause > 0 or not enabled:
		return
	if current.delivery == AttackDefinition.Delivery.PROJECTILE:
		if not projectile_released:
			projectile_released = true
			var arrow = preload("res://game/combat/arrow_projectile.gd").new()
			arrow.configure(actor, current, swing_direction)
			actor.get_parent().add_child(arrow)
			arrow.global_position = actor.global_position
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

func receive_hit(definition: AttackDefinition, source: CharacterEntity, incoming_direction := Vector2.ZERO) -> int:
	if dead or invulnerability > 0 or not enabled or source.team == actor.team:
		return -1
	var direction: Vector2 = incoming_direction.normalized() if not incoming_direction.is_zero_approx() else source.global_position.direction_to(actor.global_position)
	if is_blocking() and actor.facing.dot(-direction) > 0.35 and stamina >= definition.stamina_cost:
		stamina -= definition.stamina_cost
		regen_delay = 0.7
		invulnerability = 0.08
		knockback_velocity = direction * 60
		hit_pause = maxf(hit_pause, definition.hit_stop * 0.5)
		hit_received.emit(direction, true)
		return 0
	block_intent = false
	actor.health_controller.change_hp(-definition.damage, source.name)
	invulnerability = 0.12
	hit_pause = definition.hit_stop
	knockback_velocity = direction * definition.knockback
	hit_received.emit(direction, false)
	if definition.heavy or not armor_resistance:
		stagger_left = definition.stagger
		dodge_left = 0
		current = null
		queued = null
		buffer_left = 0
		_set_phase("Stagger")
	if actor.health_controller.hp <= 0:
		dead = true
		current = null
		queued = null
		buffer_left = 0
		statuses.clear()
		_set_phase("Dead")
		died.emit()
	else:
		for application in definition.on_hit_effects:
			if application != null and application.effect != null and application.rolls_success(source.combat.status_rng):
				statuses.apply(application.effect, source.entity_id)
	return definition.damage

func _on_status_applied(effect: StatusEffectDefinition, _source_id: String) -> void:
	if effect.interrupts_action:
		current = null
		queued = null
		buffer_left = 0
		dodge_left = 0
		block_intent = false
	if effect.blocks_actions:
		_set_phase(effect.label)

func _set_phase(value: String) -> void:
	if phase != value:
		phase = value
		phase_changed.emit(phase)
