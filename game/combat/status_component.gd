class_name StatusComponent
extends RefCounted
signal applied(effect: StatusEffectDefinition, source_id: String)
signal expired(effect_id: StringName)

# Stable effect ID -> runtime record. No node references in effect records.
var active: Dictionary = {}

func apply(effect: StatusEffectDefinition, source_id: String) -> void:
	if effect == null or effect.id == &"" or effect.duration <= 0:
		return
	if active.has(effect.id) and not effect.refresh_on_reapply:
		return
	active[effect.id] = {"definition": effect, "remaining": effect.duration, "source_id": source_id}
	applied.emit(effect, source_id)

func tick(delta: float) -> void:
	for id in active.keys():
		active[id].remaining = maxf(0, active[id].remaining - delta)
		if active[id].remaining <= 0.000001:
			active.erase(id)
			expired.emit(id)

func clear() -> void:
	var ids := active.keys()
	active.clear()
	for id in ids:
		expired.emit(id)

func has(id: StringName) -> bool:
	return active.has(id)

func blocks_actions() -> bool:
	for record in active.values():
		if record.definition.blocks_actions:
			return true
	return false

func blocks_movement() -> bool:
	for record in active.values():
		if record.definition.blocks_movement:
			return true
	return false

func movement_scale() -> float:
	var scale := 1.0
	for record in active.values():
		scale *= record.definition.movement_multiplier
	return clampf(scale, 0, 3)

func presentation_progress(tag: StringName) -> float:
	for record in active.values():
		if record.definition.presentation == tag:
			return 1.0 - record.remaining / record.definition.duration
	return -1.0

func action_label() -> String:
	for record in active.values():
		if record.definition.blocks_actions:
			return record.definition.label
	return ""
