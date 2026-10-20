extends SceneTree
func _init() -> void:
	var failures := 0
	for name in ["club", "warband"]:
		var rig: ModularSpriteRig = load("res://game/content/modular/one_handed/%s.tres" % name)
		var errors := rig.validate()
		print(name, ": ", errors)
		failures += errors.size()
		var layers: Array[ModularSpriteLayer] = [rig.body]
		layers.append_array(rig.equipment)
		for layer in layers:
			print("  ", layer.label, " ", layer.frame_count(), " ", layer.sheet.get_size())
	quit(failures)
