extends SceneTree
## Render actual layered PNGs without requiring an editor import in progress.
var INDICES = range(56)

func _init() -> void:
	call_deferred("_capture")

func _capture() -> void:
	var args := OS.get_cmdline_user_args()
	var family := args[0] if not args.is_empty() else "two_handed"
	var folder := "res://assets/art/modular/" + family + "/"
	var viewport := SubViewport.new()
	viewport.size = Vector2i(2048, 1792)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var background := ColorRect.new()
	background.z_index = -10
	background.color = Color("#25313c")
	background.size = Vector2(2048, 1792)
	viewport.add_child(background)
	var content := Node2D.new()
	viewport.add_child(content)
	var path := "res://game/content/modular/%s/%s.tres" % [family, {"two_handed": "hero", "one_handed": "club", "bow": "archer"}[family]]
	var rig: ModularSpriteRig = load(path) if ResourceLoader.exists(path) else null
	for i in INDICES.size():
		var point := Vector2(i % 8 * 256 + 128, i / 8 * 256 + 220)
		var marker := Line2D.new()
		marker.points = PackedVector2Array([point + Vector2(-100, 0), point + Vector2(100, 0)])
		marker.width = 1
		marker.default_color = Color("#536473")
		content.add_child(marker)
		var label := Label.new()
		label.text = "%s · frame %d" % [family, INDICES[i]]
		label.position = point + Vector2(-145, 20)
		label.add_theme_font_size_override("font_size", 18)
		content.add_child(label)
		var layers: Array[ModularSpriteLayer] = []
		if rig != null:
			layers.append(rig.body)
			layers.append_array(rig.equipment)
		else:
			for slot in ["body", "armor", "weapon"]:
				var image := Image.load_from_file(folder + slot + ".png") if FileAccess.file_exists(folder + slot + ".png") else null
				if image == null:
					continue
				var layer := ModularSpriteLayer.new()
				layer.sheet = ImageTexture.create_from_image(image)
				layer.slot = slot
				layer.depth = ["body", "armor", "weapon"].find(slot)
				layers.append(layer)
		for layer in layers:
			layer.sheet = ImageTexture.create_from_image(Image.load_from_file(folder + str(layer.slot) + ".png"))
			var sprite := Sprite2D.new()
			content.add_child(sprite)
			layer.apply(sprite, INDICES[i], false)
			sprite.position = point
			sprite.scale *= 1.0 * 2048.0 / layer.sheet.get_width()
	await process_frame
	await RenderingServer.frame_post_draw
	var destination := "res://assets/art/modular/bow/preview-all-%s.png" % family
	print("MODULAR_PREVIEW: ", viewport.get_texture().get_image().save_png(destination), " ", destination)
	quit()




