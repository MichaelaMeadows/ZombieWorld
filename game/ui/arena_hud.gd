extends CanvasLayer

var arena: Node2D
var hero: ArenaActor
var hp: ProgressBar
var hp_text: Label
var root: Control
var modal: Control

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	root = Control.new()
	root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.theme = ArenaDesign.theme()
	add_child(root)
	var stats := PanelContainer.new()
	stats.position = Vector2(24, 24)
	stats.custom_minimum_size = Vector2(320, 110)
	root.add_child(stats)
	var v := VBoxContainer.new()
	stats.add_child(v)
	v.add_child(ArenaDesign.label("BRIARWATCH  /  VANGUARD", 14, ArenaDesign.BRONZE))
	hp_text = ArenaDesign.label("Vitality  •  100 / 100", 18)
	v.add_child(hp_text)
	hp = ArenaDesign.bar(Color("#b96359"))
	v.add_child(hp)
	var title := VBoxContainer.new()
	title.set_anchors_and_offsets_preset(Control.PRESET_TOP_RIGHT)
	title.offset_left = -380
	title.offset_right = -24
	title.offset_top = 26
	title.offset_bottom = 100
	root.add_child(title)
	title.add_child(ArenaDesign.label("THE BRIARWATCH", 28))
	title.add_child(ArenaDesign.label("01  /  A clearing worth defending", 15, ArenaDesign.MUTED))
	var footer := PanelContainer.new()
	footer.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
	footer.offset_left = 24
	footer.offset_right = -24
	footer.offset_top = -72
	footer.offset_bottom = -20
	root.add_child(footer)
	footer.add_child(ArenaDesign.label("W A S D   Move     •     MOUSE   Face     •     ESC   Pause", 17))

func _process(_delta: float) -> void:
	hp.value = hero.health_controller.hp
	hp_text.text = "Vitality  •  %d / %d" % [hero.health_controller.hp, hero.health_controller.max_hp]

func _input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_cancel"):
		if is_instance_valid(modal):
			close_modal()
		else:
			open_pause()
		get_viewport().set_input_as_handled()

func close_modal() -> void:
	if is_instance_valid(modal):
		modal.queue_free()
	modal = null
	get_tree().paused = false

func open_pause() -> void:
	get_tree().paused = true
	modal = ColorRect.new()
	modal.color = Color(0.03, 0.07, 0.09, 0.78)
	modal.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.add_child(modal)
	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	modal.add_child(center)
	var p := PanelContainer.new()
	p.custom_minimum_size.x = 440
	center.add_child(p)
	var v := VBoxContainer.new()
	p.add_child(v)
	v.add_child(ArenaDesign.label("A MOMENT OF RESPITE", 14, ArenaDesign.BRONZE))
	v.add_child(ArenaDesign.label("The watch can wait.", 30))
	var resume := ArenaDesign.button("Resume", close_modal)
	v.add_child(resume)
	v.add_child(ArenaDesign.button("Restart clearing", func(): close_modal(); get_tree().reload_current_scene()))
	v.add_child(ArenaDesign.button("Quit", func(): get_tree().quit()))
	resume.grab_focus()
