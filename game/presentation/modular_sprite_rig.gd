class_name ModularSpriteRig
extends Resource
## Immutable library. Equipment choices and Sprite2D nodes belong to each actor.
@export var label := ""
@export var family: StringName = &"one_handed"
@export var rig_id: StringName = &"human_one_handed_v1"
@export var display_scale := 0.42
@export var body: ModularSpriteLayer
@export var equipment: Array[ModularSpriteLayer] = []
# Whole-rig registration for the player, in atlas pixels. It never moves one
# equipment layer independently or changes the actor's physics position.
@export var player_pose_offsets: PackedVector2Array = []
@export var player_walk_stride := 0.0
@export var player_run_stride := 0.0
# Four frames per clip; each row of the 8x7 atlas contains two clips.
const CLIPS = {
	"idle_e": 0, "walk_e": 4, "walk_s": 8, "walk_n": 12,
	"run_e": 16, "run_s": 20, "run_n": 24,
	"attack_e": 28, "attack_s": 32, "attack_n": 36, "attack_ne": 40,
	"hurt": 44, "death": 48, "attack_se": 52,
}

func accepts(layer: ModularSpriteLayer) -> bool:
	return (layer != null and layer.family == family and layer.rig_id == rig_id
		and layer.frame_count() == 56 and layer.validate().is_empty()
		and (body == null or body.sheet == null or layer.sheet.get_size() == body.sheet.get_size()))

func validate() -> PackedStringArray:
	var errors := PackedStringArray()
	if body == null:
		errors.append("%s: missing body" % label)
	elif not accepts(body) or body.slot != &"body":
		errors.append("%s: incompatible body" % label)
	if display_scale <= 0:
		errors.append("%s: invalid scale" % label)
	if not player_pose_offsets.is_empty() and player_pose_offsets.size() != 56:
		errors.append("%s: player pose offsets must cover 56 frames" % label)
	var slots := {}
	for layer in equipment:
		if not accepts(layer) or layer.slot == &"body":
			errors.append("%s: incompatible equipment" % label)
		elif slots.has(layer.slot):
			errors.append("%s: duplicate slot %s" % [label, layer.slot])
		else:
			slots[layer.slot] = true
	return errors

func sample(actor: Node2D) -> Dictionary:
	var aim: Vector2 = actor.combat.swing_direction if actor.combat.current != null else actor.facing
	var octant := posmod(roundi(aim.angle() / (PI * 0.25)), 8)
	var direction: String = ["e", "se", "s", "se", "e", "ne", "n", "ne"][octant]
	var clip := "idle_e"
	var frame := 0
	var animation := "idle"
	var legacy_row := 0
	var mirrored := aim.x < 0
	if actor.dead:
		clip = "death"
		frame = mini(3, int(actor.death_age / 0.85 * 4))
		animation = "death"
		legacy_row = 5
	elif actor.combat.current != null:
		clip = "attack_" + direction
		var attack: AttackDefinition = actor.combat.current
		var elapsed: float = actor.combat.elapsed
		frame = mini(1, int(elapsed / maxf(attack.startup, 0.001) * 2)) if elapsed < attack.startup else (2 if elapsed < attack.startup + attack.active else 3)
		# Let the extended two-handed strike finish its follow-through before
		# snapping back to the guard drawing. Combat timing stays independent.
		if family == &"two_handed" and elapsed >= attack.startup + attack.active and elapsed < attack.startup + attack.active + attack.recovery * 0.35:
			frame = 2
		animation = "attack"
		legacy_row = [0, 1, 1, 1, 0, 3, 2, 3][octant]
	elif actor.combat.dodge_left > 0:
		var motion: Vector2 = actor.combat.dodge_direction
		direction = "e" if absf(motion.x) >= absf(motion.y) else ("s" if motion.y > 0 else "n")
		clip = "run_" + direction
		frame = mini(3, int(actor.combat.dodge_progress() * 4))
		mirrored = motion.x < 0
		animation = "dodge"
		legacy_row = 4 + ["e", "s", "n"].find(direction)
	elif actor.hurt_age < 0.24:
		clip = "hurt"
		frame = mini(3, int(actor.hurt_age / 0.24 * 4))
		animation = "hurt"
		legacy_row = 4
	elif actor.locomotion_state != "Idle":
		var motion: Vector2 = actor.velocity
		if actor.player_controlled and not actor.locomotion_direction.is_zero_approx():
			motion = actor.locomotion_direction
		direction = "e" if absf(motion.x) >= absf(motion.y) else ("s" if motion.y > 0 else "n")
		animation = actor.locomotion_state.to_lower()
		clip = animation + "_" + direction
		frame = mini(3, int(actor.locomotion_phase * 4))
		mirrored = motion.x < 0
		legacy_row = 1 + ["e", "s", "n"].find(direction) + (3 if animation == "run" else 0)
	elif direction != "e":
		clip = "attack_" + direction
		frame = 3
	else:
		# The four generated idle drawings vary in proportions. A stable guard
		# avoids head/hand wobble while retaining the established character.
		frame = 0 if actor.player_controlled else int(actor.age * 3) % 4
	return {"clip": clip, "frame": frame, "index": CLIPS[clip] + frame,
		"mirrored": mirrored, "animation": animation, "row": legacy_row}

func present(actor: Node2D) -> void:
	var state := sample(actor)
	var sprite: Sprite2D = actor.body_sprite
	body.apply(sprite, state.index, state.mirrored)
	sprite.position = Vector2.ZERO
	if actor.player_controlled and not player_pose_offsets.is_empty():
		sprite.position = player_pose_offsets[state.index] * display_scale
		if state.mirrored:
			sprite.position.x *= -1
	sprite.scale = Vector2.ONE * display_scale
	sprite.set_meta("animation", state.animation)
	sprite.set_meta("pose", Vector2i(state.frame, state.row))
	sprite.set_meta("clip", state.clip)
	var visible_slots := {}
	for layer in equipment:
		if not actor.visual_equipment.has(layer.slot):
			_present_layer(actor, layer, state, visible_slots)
	for slot in actor.visual_equipment:
		var layer: ModularSpriteLayer = actor.visual_equipment[slot]
		if layer != null:
			_present_layer(actor, layer, state, visible_slots)
	for slot in actor.visual_layer_nodes:
		actor.visual_layer_nodes[slot].visible = visible_slots.has(slot)
	# Keep every part at the actor's Z so Y-sort treats it as one character.
	# Local child order, rather than global Z, orders armor and foreground.
	var ordered: Array = actor.visual_layer_nodes.values()
	ordered.sort_custom(func(a: Sprite2D, b: Sprite2D):
		return a.get_meta("layer_depth") < b.get_meta("layer_depth"))
	for i in ordered.size():
		sprite.move_child(ordered[i], i)

func _present_layer(actor: Node2D, layer: ModularSpriteLayer, state: Dictionary, visible_slots: Dictionary) -> void:
	if not actor.visual_layer_nodes.has(layer.slot):
		var sprite := Sprite2D.new()
		sprite.name = "Equipment_" + str(layer.slot)
		sprite.use_parent_material = true
		actor.body_sprite.add_child(sprite)
		actor.visual_layer_nodes[layer.slot] = sprite
	var sprite: Sprite2D = actor.visual_layer_nodes[layer.slot]
	sprite.visible = true
	layer.apply(sprite, state.index, state.mirrored)
	# A negative absolute Z would bury the weapon under the arena ground.
	# Draw back equipment immediately behind its own body instead.
	sprite.set_meta("layer_depth", sprite.z_index)
	sprite.show_behind_parent = sprite.z_index < 0
	sprite.z_index = 0
	visible_slots[layer.slot] = true

