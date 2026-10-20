extends Node2D
## Rendered player-only inspection. Captures are opt-in and never touch saves.
const ACTOR = preload("res://game/actors/actor.tscn")
var units: Array[ArenaActor] = []
var anchors: Array[Vector2] = []
var frame := 0
var output := ""
var body_only := false
var title: Label
var travel := false
var arena: Node2D

func _ready() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--output="):
			output = arg.trim_prefix("--output=")
		body_only = body_only or arg == "--body-only"
		travel = travel or arg == "--travel"
	if travel:
		arena = preload("res://game/arena.tscn").instantiate()
		arena.test_mode = true
		add_child(arena)
		for unit: ArenaActor in arena.actors:
			unit.brain.set_physics_process(false)
			unit.brain.set_process_unhandled_input(false)
			unit.set_physics_process(false)
			unit.position = Vector2(1450, 950)
		arena.hero.position = Vector2(900, 450)
		arena.camera.position_smoothing_enabled = false
		return
	RenderingServer.set_default_clear_color(Color("#23313c"))
	title = Label.new()
	title.position = Vector2(24, 16)
	title.add_theme_font_size_override("font_size", 24)
	add_child(title)
	for row in 3:
		for column in 8:
			var unit: ArenaActor = ACTOR.instantiate()
			unit.player_controlled = true
			unit.entity_id = "preview_%d_%d" % [row, column]
			unit.loadout = preload("res://game/content/greatsword_loadout.tres")
			unit.position = Vector2(90 + column * 157, 220 + row * 205)
			unit.scale = Vector2.ONE * 1.5
			unit.facing = Vector2.RIGHT.rotated(column * PI / 4)
			add_child(unit)
			unit.set_physics_process(false)
			if body_only:
				unit.equip_visual(&"armor", null)
				unit.equip_visual(&"weapon", null)
			unit._update_sprite()
			units.append(unit)
			anchors.append(unit.position)
			var caption := Label.new()
			caption.text = ["Idle", "Walk", "Run"][row] + " / " + ["E", "SE", "S", "SW", "W", "NW", "N", "NE"][column]
			caption.position = unit.position + Vector2(-50, 25)
			add_child(caption)
	queue_redraw()

func _physics_process(delta: float) -> void:
	frame += 1
	if travel:
		_travel(delta)
		return
	var moving := frame > 18 and frame < 150
	title.text = "PLAYER LOCOMOTION  /  " + ("BASE BODY" if body_only else "EQUIPPED") + ("  /  moving" if moving else "  /  settle to idle")
	for index in units.size():
		var unit := units[index]
		var row := index / 8
		unit.intent = Vector2.RIGHT.rotated((index % 8) * PI / 4) if row > 0 and moving else Vector2.ZERO
		unit.run_intent = row == 2
		unit._physics_process(delta)
		unit.position = anchors[index] # Treadmill view; phase still uses real travel.
	if frame in [60, 155, 175] and not output.is_empty():
		_capture(frame)
	if frame >= 190:
		print("PLAYER_VISUAL: completed idle, eight-way walk/run, start and stop")
		get_tree().quit()

func _capture(at_frame: int) -> void:
	await RenderingServer.frame_post_draw
	var error := get_viewport().get_texture().get_image().save_png("%s-%03d.png" % [output, at_frame])
	print("PLAYER_CAPTURE: ", at_frame, " result=", error)

func _draw() -> void:
	for anchor in anchors:
		draw_line(anchor + Vector2(-65, 0), anchor + Vector2(65, 0), Color("#607784"), 1)

func _travel(delta: float) -> void:
	var segment := maxi(0, frame - 30) / 32
	var moving := frame > 30 and (frame - 30) % 32 < 24 and segment < 16
	var hero: ArenaActor = arena.hero
	hero.intent = Vector2.RIGHT.rotated((segment % 8) * PI / 4) if moving else Vector2.ZERO
	hero.run_intent = segment >= 8
	if moving:
		hero.aim_at(hero.position + hero.intent * 200)
	if frame == 286:
		hero.position = Vector2(900, 450)
		hero.combat.stamina = 100
	if frame >= 542:
		if frame == 542:
			hero.position = Vector2(900, 450)
			hero.combat.stamina = 100
		# Continuous walk -> run -> walk -> stop, with no input-release gap.
		hero.intent = Vector2.RIGHT if frame < 622 else Vector2.ZERO
		hero.run_intent = frame >= 558 and frame < 598
		hero.facing = Vector2.RIGHT
	hero._physics_process(delta)
	if frame in [52, 116, 308, 436, 566, 614] and not output.is_empty():
		_capture(frame)
	if frame >= 650:
		print("PLAYER_TRAVEL: completed eight-way walk/run, repeated starts/stops and direction changes in arena")
		get_tree().quit()
