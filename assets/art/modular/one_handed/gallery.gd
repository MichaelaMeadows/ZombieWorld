extends SceneTree
func _init() -> void:
	call_deferred("_capture")
func _capture() -> void:
	var variant := OS.get_cmdline_user_args()[0]
	var layer_names := ["body", "armor", "weapon"] if variant == "club" else ["body", "warband_armor", "sword"]
	var registered_layers: Array[ModularSpriteLayer] = []
	for name in layer_names:
		var layer: ModularSpriteLayer = load("res://game/content/modular/one_handed/%s.tres" % name)
		layer.sheet = ImageTexture.create_from_image(Image.load_from_file("res://assets/art/modular/one_handed/%s.png" % name))
		registered_layers.append(layer)
	var foreground: ModularSpriteLayer = load("res://game/content/modular/one_handed/foreground.tres")
	foreground.sheet = ImageTexture.create_from_image(Image.load_from_file("res://assets/art/modular/one_handed/foreground.png"))
	foreground.depth = 3
	registered_layers.append(foreground)
	var viewport := SubViewport.new()
	viewport.size = Vector2i(2048, 1792)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var background := ColorRect.new()
	background.color = Color("#25313c")
	background.size = Vector2(2048, 1792)
	background.z_index = -10
	viewport.add_child(background)
	for i in 56:
		var point := Vector2(i % 8 * 256 + 128, i / 8 * 256 + 220)
		var label := Label.new()
		label.text = str(i)
		label.position = point + Vector2(-115, 8)
		viewport.add_child(label)
		for layer in registered_layers:
			var sprite := Sprite2D.new()
			viewport.add_child(sprite)
			layer.apply(sprite, i, false)
			sprite.position = point
	await process_frame
	await RenderingServer.frame_post_draw
	viewport.get_texture().get_image().save_png("res://assets/art/modular/one_handed/%s-gallery.png" % variant)
	quit()
