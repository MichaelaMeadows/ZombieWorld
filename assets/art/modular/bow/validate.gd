extends SceneTree
func _initialize() -> void:
	var rig: ModularSpriteRig = load("res://game/content/modular/bow/archer.tres")
	var errors := rig.validate()
	for layer in [rig.body] + rig.equipment:
		var image := Image.load_from_file("res://assets/art/modular/bow/" + str(layer.slot) + ".png")
		assert(image.get_size() == Vector2i(2048, 1792))
		assert(image.detect_alpha() != Image.ALPHA_NONE)
		assert(layer.frame_count() == 56)
	assert(errors.is_empty(), str(errors))
	print("ARCHER_VALID: body/armor/weapon RGBA2048x1792, 56 frames; rig validation passed.")
	quit()
