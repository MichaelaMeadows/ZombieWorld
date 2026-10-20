extends Node
## Sub-second layout verification; no Windows UI automation.
var arena: Node2D
var errors := 0

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	arena = preload("res://game/arena.tscn").instantiate()
	arena.test_mode = true
	get_tree().root.add_child.call_deferred(arena)
	call_deferred("_run")

func _run() -> void:
	await get_tree().process_frame
	await get_tree().process_frame
	arena.hud.debug_panel.visible = true
	await get_tree().process_frame
	await get_tree().process_frame
	_validate(arena.hud.root)
	await _capture("layout-debug")
	arena.hud.debug_panel.visible = false
	arena.hud.open_pause()
	await get_tree().process_frame
	await get_tree().process_frame
	_validate(arena.hud.root)
	await _capture("layout-pause")
	arena.hud.open_settings()
	await get_tree().process_frame
	await get_tree().process_frame
	_validate(arena.hud.root)
	await _capture("layout-settings")
	arena.hud.close_modal()
	arena.queue_free()
	await get_tree().process_frame
	print("LAYOUT_RESULT: %d errors" % errors)
	get_tree().quit(1 if errors else 0)

func _validate(node: Node) -> void:
	if node is Control and node.is_visible_in_tree():
		var rect: Rect2 = node.get_global_rect()
		var bounds := get_viewport().get_visible_rect().grow(2)
		if not bounds.encloses(rect):
			errors += 1
			push_error("Control outside viewport: %s %s" % [node.name, rect])
		if node is Label and node.size.x + 2 < node.get_minimum_size().x:
			errors += 1
			push_error("Label clipped: " + node.text)
	for child in node.get_children():
		_validate(child)

func _capture(label: String) -> void:
	if DisplayServer.get_name() == "headless":
		return
	await RenderingServer.frame_post_draw
	var picture := get_viewport().get_texture().get_image()
	picture.save_png("res://docs/verification/%s-%d.png" % [label, picture.get_width()])
