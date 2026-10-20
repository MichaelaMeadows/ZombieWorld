extends Node2D

const ACTOR = preload("res://game/actors/actor.tscn")
var hero: ArenaActor
var actors: Array[ArenaActor] = []
var camera: Camera2D
var hud: CanvasLayer
var obstacles: Array[Rect2] = []

func _ready() -> void:
	_build_world()
	hero = ACTOR.instantiate()
	hero.player_controlled = true
	hero.position = Vector2(600, 540)
	$Actors.add_child(hero)
	actors.append(hero)
	camera = Camera2D.new()
	camera.position_smoothing_enabled = true
	camera.position_smoothing_speed = 7.0
	camera.limit_left = 0
	camera.limit_top = 0
	camera.limit_right = 1536
	camera.limit_bottom = 1024
	hero.add_child(camera)
	hud = preload("res://game/ui/arena_hud.gd").new()
	hud.arena = self
	hud.hero = hero
	add_child(hud)

func _build_world() -> void:
	var terrain := Node2D.new()
	terrain.set_script(preload("res://game/presentation/arena_ground.gd"))
	add_child(terrain)
	move_child(terrain, 0)
	# Shoreline collision and reusable prop collision are authored together.
	_block(Rect2(0, 0, 1536, 60))
	_block(Rect2(0, 964, 1536, 60))
	_block(Rect2(0, 0, 60, 1024))
	_block(Rect2(1476, 0, 60, 1024))
	_prop("Tower_Blue.png", Vector2(300, 355), Vector2(0.9, 0.9), Vector2(90, 75))
	for p in [Vector2(160, 250), Vector2(445, 225), Vector2(650, 240), Vector2(850, 170), Vector2(1250, 200), Vector2(1390, 400), Vector2(240, 780), Vector2(500, 850), Vector2(900, 875), Vector2(1290, 845)]:
		_prop("Tree.png", p, Vector2(0.8, 0.8), Vector2(34, 28), true)
	for p in [Vector2(725, 420), Vector2(790, 725)]:
		_prop("05.png", p, Vector2.ONE, Vector2(42, 32))
	var rng := RandomNumberGenerator.new()
	rng.seed = 2701
	for i in range(75):
		var p := Vector2(rng.randf_range(100, 1430), rng.randf_range(120, 900))
		if p.x > 370 and p.y > 320 and p.y < 770:
			continue
		_prop("%02d.png" % [rng.randi_range(8, 18)], p, Vector2.ONE, Vector2.ZERO)

func _prop(file: String, pos: Vector2, art_scale: Vector2, solid: Vector2, tree: bool = false) -> void:
	var prop := Node2D.new()
	prop.position = pos
	$Actors.add_child(prop)
	var sprite := Sprite2D.new()
	sprite.texture = load("res://assets/art/" + file)
	if tree:
		sprite.hframes = 4
		sprite.vframes = 3
	sprite.scale = art_scale
	sprite.position.y = -sprite.texture.get_height() / sprite.vframes * art_scale.y * 0.32
	prop.add_child(sprite)
	if solid != Vector2.ZERO:
		_block(Rect2(pos - solid / 2, solid))

func _block(rect: Rect2) -> void:
	obstacles.append(rect)
	var body := StaticBody2D.new()
	body.position = rect.get_center()
	var shape := CollisionShape2D.new()
	var box := RectangleShape2D.new()
	box.size = rect.size
	shape.shape = box
	body.add_child(shape)
	add_child(body)
