extends SceneTree
func _initialize() -> void:
	for part in ["body", "armor", "weapon"]:
		var path: String = "res://assets/art/modular/bow/" + part + ".png"
		var image := Image.load_from_file(path)
		image.resize(2048, 1792, Image.INTERPOLATE_LANCZOS)
		image.save_png(path)
	quit()
