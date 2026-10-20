extends SceneTree
func _init() -> void:
	for name in ["body", "armor", "weapon", "sword", "warband_armor", "foreground"]:
		var path := "res://assets/art/modular/one_handed/%s.png" % name
		var image := Image.load_from_file(path)
		print(name, " ", image.get_size())
		image.resize(2048, 1792, Image.INTERPOLATE_LANCZOS)
		image.save_png(path)
	quit()
