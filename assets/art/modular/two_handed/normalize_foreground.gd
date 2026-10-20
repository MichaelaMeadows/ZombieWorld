extends SceneTree
func _initialize():
	for slot in ["foreground"]:
		var path = "res://assets/art/modular/two_handed/%s.png" % slot
		var img = Image.load_from_file(path)
		print(slot, " source ", img.get_size(), " alpha ", img.detect_alpha())
		img.resize(2048,1792,Image.INTERPOLATE_LANCZOS)
		img.save_png(path)
	quit()

