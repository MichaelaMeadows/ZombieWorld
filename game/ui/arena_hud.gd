extends CanvasLayer

var arena: Node2D
var hero: ArenaActor
var hp: ProgressBar
var hp_text: Label
var root: Control
var modal: Control
var stamina_bar: ProgressBar
var stamina_text: Label
var combat_text: Label
var objective: Label
var debug_panel: PanelContainer
var debug_text: Label
var notification: Label
var ending: Label

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
	stamina_text = ArenaDesign.label("Stamina  •  100", 15, ArenaDesign.MUTED)
	v.add_child(stamina_text)
	stamina_bar = ArenaDesign.bar(Color("#81ad88"))
	v.add_child(stamina_bar)
	combat_text = ArenaDesign.label("SWORD  /  Ready", 14, ArenaDesign.BRONZE)
	v.add_child(combat_text)
	var title_panel := PanelContainer.new()
	title_panel.set_anchors_and_offsets_preset(Control.PRESET_TOP_RIGHT)
	title_panel.offset_left = -404
	title_panel.offset_right = -24
	title_panel.offset_top = 24
	title_panel.offset_bottom = 140
	root.add_child(title_panel)
	var title := VBoxContainer.new()
	title_panel.add_child(title)
	title.add_child(ArenaDesign.label("THE BRIARWATCH", 28))
	objective = ArenaDesign.label("01  /  Defeat the " + arena.encounter_label, 15, ArenaDesign.MUTED)
	title.add_child(objective)
	title.add_child(ArenaDesign.label(arena.encounter_hint, 14, ArenaDesign.BRONZE))
	var footer := PanelContainer.new()
	footer.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
	footer.offset_left = 24
	footer.offset_right = -24
	footer.offset_top = -72
	footer.offset_bottom = -20
	root.add_child(footer)
	footer.add_child(ArenaDesign.label("WASD Walk   SHIFT Run   LMB Cut   Q Cleave   RMB Block   SPACE Dodge   E Rest   ESC Pause   F3 Tune", 16))
	notification = ArenaDesign.label("", 16)
	notification.position = Vector2(26, 230)
	notification.add_theme_color_override("font_shadow_color", Color("#111d23"))
	notification.add_theme_constant_override("shadow_offset_x", 1)
	notification.add_theme_constant_override("shadow_offset_y", 2)
	root.add_child(notification)
	ending = ArenaDesign.label("", 28)
	ending.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	ending.offset_left = -300
	ending.offset_right = 300
	ending.offset_top = 140
	ending.offset_bottom = 185
	ending.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	ending.add_theme_color_override("font_shadow_color", Color.BLACK)
	ending.add_theme_constant_override("shadow_offset_y", 2)
	root.add_child(ending)
	_build_debug()

func _process(_delta: float) -> void:
	hp.value = hero.health_controller.hp
	hp.max_value = hero.health_controller.max_hp
	hp_text.text = "Vitality  •  %d / %d" % [hero.health_controller.hp, hero.health_controller.max_hp]
	stamina_bar.value = hero.combat.stamina
	stamina_text.text = "Stamina  •  %d" % hero.combat.stamina
	combat_text.text = "%s  /  %s" % [hero.loadout.label.to_upper() if hero.loadout != null else "SWORD", hero.combat.phase]
	var alive := 0
	var allies := 0
	for actor in arena.actors:
		if actor.team == 1 and not actor.dead:
			alive += 1
		elif actor != hero and actor.team == hero.team and not actor.dead:
			allies += 1
	objective.text = "01  /  %d %s remaining" % [alive, arena.encounter_label] if alive > 0 else "01  /  Clearing secured"
	if allies > 0:
		objective.text += "  ·  %d allies" % allies
	notification.text = "E  •  Rest at the watchtower" if hero.position.distance_to(Vector2(300, 400)) < 130 else (arena.recent_events[0] if not arena.recent_events.is_empty() else "Enter the sand court. Watch the enemy wind-up.")
	ending.text = "YOU FELL  ·  R to retry" if hero.dead else ("CLEARING SECURED  ·  R to replay" if arena.encounter_finished else "")
	if debug_panel.visible:
		var lines := PackedStringArray()
		lines.append("%d FPS  |  %.2f ms" % [Engine.get_frames_per_second(), Performance.get_monitor(Performance.TIME_PROCESS) * 1000])
		lines.append("Physics %.2f ms  |  NPCs %d  |  Hits %d" % [Performance.get_monitor(Performance.TIME_PHYSICS_PROCESS) * 1000, alive, arena.impacts])
		lines.append("Navigation: 48 × 32 grid / 180 ms decisions")
		for actor in arena.actors:
			if not actor.player_controlled:
				lines.append("%s: %s  (%d μs)" % [actor.name, actor.brain.state, actor.brain.decision_usec])
		debug_text.text = "\n".join(lines)

func _input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_cancel"):
		if is_instance_valid(modal):
			close_modal()
		else:
			open_pause()
		get_viewport().set_input_as_handled()
	elif event.is_action_pressed("aw_debug") and not is_instance_valid(modal):
		debug_panel.visible = not debug_panel.visible
		if not debug_panel.visible:
			root.get_viewport().gui_release_focus()
		get_viewport().set_input_as_handled()

func close_modal() -> void:
	if is_instance_valid(modal):
		modal.hide()
		modal.queue_free()
	modal = null
	get_tree().paused = false
	hero.intent = Vector2.ZERO
	hero.combat.block_intent = false
	get_viewport().gui_release_focus()

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
	v.add_child(ArenaDesign.button("Settings", open_settings))
	v.add_child(ArenaDesign.button("Quit", func(): get_tree().quit()))
	resume.grab_focus()

func open_settings() -> void:
	modal.hide()
	modal.queue_free()
	modal = ColorRect.new()
	modal.color = Color(0.03, 0.07, 0.09, 0.85)
	modal.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.add_child(modal)
	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	modal.add_child(center)
	var panel := PanelContainer.new()
	panel.custom_minimum_size.x = 460
	center.add_child(panel)
	var v := VBoxContainer.new()
	panel.add_child(v)
	v.add_child(ArenaDesign.label("SETTINGS", 28))
	_slider(v, "Sound volume", 0, 1, arena.preferences.volume, 0.05, func(value: float): arena.preferences.volume = value)
	_slider(v, "Impact intensity", 0, 1, arena.preferences.effects, 0.05, func(value: float): arena.preferences.effects = value)
	for setting in ["camera_shake", "damage_numbers"]:
		var check := CheckButton.new()
		check.text = setting.replace("_", " ").capitalize()
		check.button_pressed = arena.preferences.get(setting)
		check.toggled.connect(func(value: bool): arena.preferences.set(setting, value))
		v.add_child(check)
	var save := ArenaDesign.button("Save & return", func():
		var error: Error = arena.preferences.save()
		arena.record_event("Settings saved." if error == OK else "Could not save settings (%d)." % error)
		close_modal()
	)
	v.add_child(save)
	save.grab_focus()

func _slider(parent: VBoxContainer, title: String, minimum: float, maximum: float, value: float, step: float, changed: Callable) -> void:
	var label := ArenaDesign.label("%s   %.2f" % [title, value], 15)
	parent.add_child(label)
	var slider := HSlider.new()
	slider.min_value = minimum
	slider.max_value = maximum
	slider.step = step
	slider.value = value
	slider.custom_minimum_size = Vector2(280, 18)
	slider.value_changed.connect(func(next: float):
		label.text = "%s   %.2f" % [title, next]
		changed.call(next)
	)
	parent.add_child(slider)

func _build_debug() -> void:
	debug_panel = PanelContainer.new()
	debug_panel.position = Vector2(840, 170)
	debug_panel.custom_minimum_size.x = 416
	debug_panel.visible = false
	root.add_child(debug_panel)
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", 4)
	debug_panel.add_child(v)
	v.add_child(ArenaDesign.label("COMBAT LAB  /  F3 to close", 16, ArenaDesign.BRONZE))
	debug_text = ArenaDesign.label("", 13)
	v.add_child(debug_text)
	_slider(v, "Primary startup", 0.06, 1.0, hero.light_attack.startup, 0.01, func(value: float): hero.light_attack.startup = value)
	_slider(v, "Primary recovery", 0.1, 1.0, hero.light_attack.recovery, 0.01, func(value: float): hero.light_attack.recovery = value)
	_slider(v, "Secondary startup", 0.25, 1.2, hero.heavy_attack.startup, 0.01, func(value: float): hero.heavy_attack.startup = value)
	_slider(v, "Weapon reach", 40, 140, hero.light_attack.reach, 1, func(value: float): hero.light_attack.reach = value)
	v.add_child(ArenaDesign.button("Return focus to combat", func(): get_viewport().gui_release_focus()))
