extends Node
var failures := 0
var assertions := 0

func check(ok: bool, message: String) -> void:
	assertions += 1
	if not ok:
		failures += 1
		push_error("FAIL: " + message)

func layer(slot: StringName, color: Color) -> ModularSpriteLayer:
	var result := ModularSpriteLayer.new()
	result.label = str(slot)
	result.slot = slot
	result.depth = 0 if slot == &"body" else 2
	# Test fixture pixels, not production art.
	var image := Image.create(80, 70, false, Image.FORMAT_RGBA8)
	image.fill(color)
	result.sheet = ImageTexture.create_from_image(image)
	return result

func _ready() -> void:
	var rig := ModularSpriteRig.new()
	rig.body = layer(&"body", Color.WHITE)
	var weapon := layer(&"weapon", Color.RED)
	var armor := layer(&"armor", Color.BLUE)
	rig.equipment.assign([armor, weapon])
	check(rig.validate().is_empty(), "valid compatible layered rig")
	var first: ArenaActor = preload("res://game/actors/actor.tscn").instantiate()
	first.player_controlled = true
	first.loadout = preload("res://game/content/greatsword_loadout.tres").duplicate()
	first.loadout.sprite_rig = rig
	add_child(first)
	first.set_physics_process(false)
	check(first.modular_rig == rig and first.visual_layer_nodes.size() == 2, "loadout selects initial layered presentation")
	check(first.set_visual_rig(rig), "install rig")
	var second: ArenaActor = preload("res://game/actors/actor.tscn").instantiate()
	second.human = true
	add_child(second)
	second.set_physics_process(false)
	second.set_visual_rig(rig)
	check(first.body_sprite.texture.atlas == rig.body.sheet, "body sheet is separate")
	check(first.visual_layer_nodes.size() == 2, "equipment slots created")
	var replacement := layer(&"weapon", Color.GREEN)
	check(first.equip_visual(&"weapon", replacement), "swap compatible weapon")
	check(first.visual_layer_nodes[&"weapon"].texture.atlas == replacement.sheet, "replacement displayed")
	check(second.visual_layer_nodes[&"weapon"].texture.atlas == weapon.sheet, "shared rig does not leak equipment changes")
	check(rig.equipment[1] == weapon, "shared defaults remain immutable")
	check(first.equip_visual(&"armor", null), "armor can be removed")
	check(not first.visual_layer_nodes[&"armor"].visible, "removed armor disappears")
	check(first.body_sprite.texture.atlas == rig.body.sheet, "removal retains underlying body")
	var incompatible := layer(&"weapon", Color.RED)
	incompatible.family = &"bow"
	check(not first.equip_visual(&"weapon", incompatible), "wrong grip rejected")
	incompatible.family = rig.family
	incompatible.rig_id = &"different_proportions"
	check(not first.equip_visual(&"weapon", incompatible), "wrong body registration rejected")
	incompatible.rig_id = rig.rig_id
	incompatible.sheet = ImageTexture.create_from_image(Image.create(160, 140, false, Image.FORMAT_RGBA8))
	check(not first.equip_visual(&"weapon", incompatible), "different coordinate scale rejected")
	check(not first.equip_visual(&"body", rig.body), "body cannot be replaced as equipment")
	check(not first.equip_visual(&"armor", weapon), "wrong slot rejected")
	check(first.visual_layer_nodes[&"weapon"].texture.atlas == replacement.sheet, "rejection preserves equipped weapon")
	replacement.frame_scales.resize(56)
	replacement.frame_scales.fill(Vector2(0.8, 0.9))
	replacement.frame_rotations.resize(56)
	replacement.frame_rotations.fill(0.2)
	replacement.pivots.resize(56)
	replacement.pivots.fill(Vector2(3, 7))
	first.facing = Vector2.LEFT
	first._update_sprite()
	check(first.visual_layer_nodes[&"weapon"].scale == Vector2(0.8, 0.9), "per-frame gear scale applied")
	check(is_equal_approx(first.visual_layer_nodes[&"weapon"].rotation, -0.2), "gear registration rotation mirrors")
	check(first.visual_layer_nodes[&"weapon"].offset == Vector2(-2, -2), "authored grip pivot mirrors")
	first.reset_visual_equipment()
	check(first.visual_layer_nodes[&"armor"].visible, "reset restores armor")
	check(first.visual_layer_nodes[&"weapon"].texture.atlas == weapon.sheet, "reset restores weapon")
	first.combat.current = first.light_attack
	first.combat.elapsed = first.light_attack.startup + 0.001
	var attack_clock := first.combat.elapsed
	var attack_damage := first.light_attack.damage
	first.equip_visual(&"weapon", replacement)
	check(first.combat.current == first.light_attack and first.combat.elapsed == attack_clock and first.light_attack.damage == attack_damage, "visual swap preserves committed attack and damage")
	check(first.body_sprite.get_meta("atlas_frame") % 4 == 2 and first.visual_layer_nodes[&"weapon"].get_meta("atlas_frame") % 4 == 2, "mid-swing swap keeps body and weapon on strike")
	first.reset_visual_equipment()
	var expected := ["attack_e", "attack_se", "attack_s", "attack_se", "attack_e", "attack_ne", "attack_n", "attack_ne"]
	for octant in 8:
		first.combat.current = first.light_attack
		first.combat.swing_direction = Vector2.RIGHT.rotated(octant * PI / 4)
		for phase in [0.0, first.light_attack.startup * 0.75, first.light_attack.startup + 0.001, first.light_attack.startup + first.light_attack.active + 0.001]:
			first.combat.elapsed = phase
			first._update_sprite()
			check(first.body_sprite.get_meta("clip") == expected[octant], "direction selects authored attack")
			for slot in first.visual_layer_nodes:
				var sprite: Sprite2D = first.visual_layer_nodes[slot]
				check(sprite.get_meta("atlas_frame") == first.body_sprite.get_meta("atlas_frame"), "layers share exact attack frame")
				check(sprite.flip_h == first.body_sprite.flip_h and sprite.offset == first.body_sprite.offset, "layers mirror around common anchor")
				check(sprite.use_parent_material, "equipment shares damage flash")
				check(sprite.z_index == first.body_sprite.z_index, "equipment stays inside actor Y-sort order")
	first.combat.current = null
	for direction in [Vector2.RIGHT, Vector2.DOWN, Vector2.UP, Vector2.LEFT]:
		first.velocity = direction * 100
		for state in ["Walk", "Run"]:
			first.locomotion_state = state
			for frame in 4:
				first.locomotion_phase = frame / 4.0
				first._update_sprite()
				check(first.body_sprite.get_meta("animation") == state.to_lower(), "locomotion selects walk/run")
				check(first.body_sprite.get_meta("atlas_frame") % 4 == frame, "locomotion retains distance-driven phase")
	first.dead = true
	first.death_age = 10
	first._update_sprite()
	check(first.body_sprite.get_meta("atlas_frame") == 51, "death holds last frame")
	check(first.visual_layer_nodes[&"weapon"].get_meta("atlas_frame") == 51, "gear follows death")
	first.dead = false
	first.locomotion_state = "Idle"
	first.hurt_age = 0.1
	first._update_sprite()
	check(first.body_sprite.get_meta("clip") == "hurt", "hurt clip")
	first.flash_left = 0.1
	first.combat.statuses.apply(preload("res://game/content/knockdown.tres"), "test")
	first._update_sprite()
	check(absf(first.body_sprite.rotation) > 0, "status transform wraps entire layered character")
	var prone_rotation := first.body_sprite.rotation
	first.equip_visual(&"weapon", replacement)
	check(is_equal_approx(first.body_sprite.rotation, prone_rotation), "equipment swap preserves active knockdown transform")
	first.reset_visual_equipment()
	check(is_equal_approx(first.body_sprite.rotation, prone_rotation), "equipment reset preserves active knockdown transform")
	first.set_visual_rig(null)
	for slot in first.visual_layer_nodes:
		check(not first.visual_layer_nodes[slot].visible, "disabling modular rig hides equipment")
	first.queue_free()
	second.queue_free()
	await get_tree().process_frame
	print("MODULAR_RESULT: %d assertions, %d failures" % [assertions, failures])
	get_tree().quit(1 if failures else 0)
