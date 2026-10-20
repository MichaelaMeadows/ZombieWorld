extends Node
var failures := 0
var assertions := 0
const PROFILES = ["two_handed/hero", "one_handed/club", "one_handed/warband", "bow/archer"]

func check(ok: bool, message: String) -> void:
	assertions += 1
	if not ok:
		failures += 1
		push_error("FAIL: " + message)

func _ready() -> void:
	var seen_layers := {}
	for profile in PROFILES:
		var path: String = "res://game/content/modular/%s.tres" % profile
		check(ResourceLoader.exists(path), "%s kit saved" % profile)
		if not ResourceLoader.exists(path):
			continue
		var rig: ModularSpriteRig = load(path)
		var errors := rig.validate()
		check(errors.is_empty(), "%s kit compatible: %s" % [profile, "; ".join(errors)])
		var layers: Array[ModularSpriteLayer] = [rig.body]
		layers.append_array(rig.equipment)
		for layer in layers:
			if layer == null or seen_layers.has(layer.resource_path):
				continue
			seen_layers[layer.resource_path] = true
			check(layer.validate().is_empty(), "%s: %s" % [layer.resource_path, "; ".join(layer.validate())])
			if layer.sheet == null or not layer.validate().is_empty():
				continue
			check(layer.sheet.get_size() == Vector2(2048, 1792), "%s normalized coordinates, got %s" % [layer.label, layer.sheet.get_size()])
			var image := layer.sheet.get_image()
			if image != null and image.is_compressed():
				image.decompress()
			check(image != null and not image.is_empty() and image.detect_alpha() != Image.ALPHA_NONE, "%s real transparency" % layer.label)
			var sprite := Sprite2D.new()
			add_child(sprite)
			for index in 56:
				layer.apply(sprite, index, false)
				var texture: AtlasTexture = sprite.texture
				check(texture.region.has_area() and Rect2(Vector2.ZERO, layer.sheet.get_size()).encloses(texture.region), "%s frame %d stays in sheet" % [layer.label, index])
				var crop := image.get_region(Rect2i(texture.region))
				check(crop.get_used_rect().has_area(), "%s frame %d not empty" % [layer.label, index])
			sprite.free()
	var club: ModularSpriteRig = load("res://game/content/modular/one_handed/club.tres")
	var soldier: ModularSpriteRig = load("res://game/content/modular/one_handed/warband.tres")
	check(club.body == soldier.body, "bandit and soldier share the same base body")
	check(club.equipment != soldier.equipment, "bandit and soldier have independent equipment sets")
	print("MODULAR_ASSETS_RESULT: %d assertions, %d failures" % [assertions, failures])
	get_tree().quit(1 if failures else 0)
