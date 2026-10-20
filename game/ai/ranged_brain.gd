extends "res://game/ai/melee_brain.gd"
## Shares staggered decisions, navigation and hostile selection with melee AI.
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
	var distance := actor.position.distance_to(target.position)
	if distance < actor.light_attack.reach or actor.arena.hero.position.x > 815:
		engaged = true
	if not engaged:
		_follow_or_guard()
		return
	if not actor.combat.can_act():
		state = "Recover"
		path.clear()
		return
	actor.aim_at(target.global_position)
	var query := PhysicsRayQueryParameters2D.create(actor.global_position, target.global_position, 3, [actor.get_rid()])
	var sight := actor.get_world_2d().direct_space_state.intersect_ray(query)
	var clear_shot: bool = sight.is_empty() or sight.collider == target
	var preferred := actor.archetype.preferred_distance
	if distance < preferred * 0.6:
		state = "Retreat"
		path.clear()
		var away := target.position.direction_to(actor.position)
		for turn in [0.0, PI * 0.25, -PI * 0.25, PI * 0.5, -PI * 0.5]:
			var destination := actor.position + away.rotated(turn) * 100
			var route: PackedVector2Array = actor.arena.find_path(actor.position, destination)
			if not route.is_empty() and route[-1].distance_to(target.position) > distance + 32:
				path = route
				actor.run_intent = true
				break
		# Cornered archers still fight instead of becoming permanently passive.
		if not path.is_empty():
			return
	if distance <= actor.light_attack.reach * 0.82 and clear_shot:
		path.clear()
		state = "Aim"
		if attack_wait <= 0 and actor.combat.request_attack(actor.light_attack):
			attack_wait = actor.archetype.attack_delay
	else:
		state = "Reposition"
		path = actor.arena.find_path(actor.position, target.position)
		actor.run_intent = distance > actor.light_attack.reach
