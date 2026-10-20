extends SceneTree
func _initialize():
	var result = Image.create(2048,1792,false,Image.FORMAT_RGBA8)
	result.fill(Color("#607080"))
	for slot in ["body","armor","weapon"]:
		var img=Image.load_from_file("res://assets/art/modular/two_handed/%s.png" % slot)
		result.blend_rect(img,Rect2i(0,0,2048,1792),Vector2i.ZERO)
	result.save_png("res://assets/art/modular/two_handed/composite.png")
	quit()
