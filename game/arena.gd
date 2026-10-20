extends Node2D

const ACTOR = preload("res://game/actors/actor.tscn")
@export var player_loadout: WeaponLoadout = preload("res://game/content/greatsword_loadout.tres")
@export var enemy_archetypes: Array[ActorArchetype] = []
@export var encounter_label := "zombies"
@export var encounter_hint := "Two shamblers   •   One brute   •   Watch their hands"
@export var include_warband := true
var hero: ArenaActor
var actors: Array[ArenaActor] = []
var camera: Camera2D
var hud: CanvasLayer
var obstacles: Array[Rect2] = []
var navigation := AStarGrid2D.new()
var preferences := ArenaPreferences.load_or_create()
var audio: Dictionary = {}
var impacts := 0
var recent_events: Array[String] = []
var shake_left := 0.0
var elapsed := 0.0
var encounter_finished := false
var test_mode := false
var frame_samples: Array[float] = []

func _ready() -> void:
	_build_world()
	_build_navigation()
	_bind_controls()
	for kind in ["swing", "hit", "heavy", "block", "miss"]:
		audio[kind] = CombatAudio.make(kind)
	hero = ACTOR.instantiate()
	hero.name = "Vanguard"
	hero.player_controlled = true
	hero.loadout = player_loadout
	hero.arena = self
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
	if include_warband:
		_spawn_warband()
	for index in range(3 if enemy_archetypes.is_empty() else enemy_archetypes.size()):
		var enemy: ArenaActor = ACTOR.instantiate()
		enemy.name = "ZombieBrute" if index == 2 else "Zombie%d" % (index + 1)
		enemy.entity_id = "briarwatch_enemy_%d" % index
		enemy.team = 1
		enemy.heavy = index == 2
		if not enemy_archetypes.is_empty():
			enemy.archetype = enemy_archetypes[index]
			enemy.name = enemy.archetype.label.replace(" ", "") + str(index + 1)
			enemy.heavy = false
		enemy.arena = self
		enemy.position = [Vector2(1000, 440), Vector2(1155, 650), Vector2(1260, 485)][index % 3]
		$Actors.add_child(enemy)
		actors.append(enemy)
	hud = preload("res://game/ui/arena_hud.gd").new()
	hud.arena = self
	hud.hero = hero
	add_child(hud)

func _spawn_warband() -> void:
	var party: Dictionary = CampaignSession.warband()
	var troops: Array = party.get("troops", [])
	if troops.is_empty():
		return
	# Pick distinct reachable grid centers, nearest the player's rear first.
	# Never silently truncate the roster to a fixed set of authored positions.
	var positions: Array[Vector2] = []
	for y in range(2, 30, 2):
		for x in range(2, 46, 2):
			var cell := Vector2i(x, y)
			if navigation.is_point_solid(cell):
				continue
			var point := Vector2(x * 32 + 16, y * 32 + 16)
			if point.distance_to(hero.position) < 48 or point.x > 850:
				continue
			if not find_path(hero.position, point).is_empty():
				positions.append(point)
	positions.sort_custom(func(a: Vector2, b: Vector2):
		return a.distance_squared_to(hero.position + Vector2(-80, 0)) < b.distance_squared_to(hero.position + Vector2(-80, 0)))
	for index in troops.size():
		var troop: Dictionary = troops[index]
		var ally: ArenaActor = ACTOR.instantiate()
		ally.name = "Warband_" + troop.id
		ally.campaign_unit_id = troop.id
		ally.entity_id = "%s/troop/%s" % [party.world_id, troop.id]
		ally.human = true
		ally.team = hero.team
		ally.loadout = preload("res://game/content/sword_loadout.tres")
		ally.starting_health = 90 + (25 if troop.get("trained", false) else 0)
		ally.arena = self
		ally.position = positions[index % positions.size()] if not positions.is_empty() else hero.position + Vector2(-64, 0)
		ally.follow_offset = Vector2(-64 - (index / 5) * 48, (index % 5 - 2) * 44)
		$Actors.add_child(ally)
		actors.append(ally)
	record_event("%d warband troops joined you." % troops.size())

func _process(delta: float) -> void:
	elapsed += delta
	if frame_samples.size() < 3600:
		frame_samples.append(delta)
	shake_left = maxf(0, shake_left - delta)
	camera.offset = Vector2(sin(elapsed * 95), cos(elapsed * 83)) * shake_left * 35 if preferences.camera_shake else Vector2.ZERO
	if not encounter_finished and actors.size() > 1:
		var alive := 0
		for actor in actors:
			if actor.team == 1 and not actor.dead:
				alive += 1
		if alive == 0:
			encounter_finished = true
			record_event("Clearing secured. Return to the watchtower to recover.")

func _bind_controls() -> void:
	var bindings := {"aw_light": MOUSE_BUTTON_LEFT, "aw_block": MOUSE_BUTTON_RIGHT, "aw_heavy": KEY_Q, "aw_dodge": KEY_SPACE, "aw_run": KEY_SHIFT, "aw_debug": KEY_F3, "aw_reset": KEY_R, "aw_interact": KEY_E}
	for action in bindings:
		if InputMap.has_action(action):
			continue
		InputMap.add_action(action)
		var event: InputEvent
		if action == "aw_light" or action == "aw_block":
			event = InputEventMouseButton.new()
			event.button_index = bindings[action]
		else:
			event = InputEventKey.new()
			event.physical_keycode = bindings[action]
		InputMap.action_add_event(action, event)

func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("aw_interact") and hero.position.distance_to(Vector2(300, 400)) < 130 and not hero.dead:
		hero.health_controller.change_hp(100, "Watchtower")
		hero.combat.stamina = 100
		record_event("Rested at the watchtower.")
	elif event.is_action_pressed("aw_reset") and (hero.dead or encounter_finished):
		get_tree().reload_current_scene()

func _build_navigation() -> void:
	navigation.region = Rect2i(0, 0, 48, 32)
	navigation.cell_size = Vector2(32, 32)
	navigation.offset = Vector2(16, 16)
	navigation.diagonal_mode = AStarGrid2D.DIAGONAL_MODE_ONLY_IF_NO_OBSTACLES
	navigation.update()
	for y in range(32):
		for x in range(48):
			var p := Vector2(x * 32 + 16, y * 32 + 16)
			for obstacle in obstacles:
				if obstacle.grow(18).has_point(p):
					navigation.set_point_solid(Vector2i(x, y))
					break

func find_path(from: Vector2, to: Vector2) -> PackedVector2Array:
	var start := Vector2i(floori(from.x / 32), floori(from.y / 32))
	var end := Vector2i(floori(to.x / 32), floori(to.y / 32))
	if not navigation.is_in_boundsv(start) or not navigation.is_in_boundsv(end):
		return PackedVector2Array()
	# A body can stand beside a conservatively inflated navigation cell.
	if navigation.is_point_solid(end):
		var closest := end
		var distance := INF
		for y in range(-2, 3):
			for x in range(-2, 3):
				var candidate := end + Vector2i(x, y)
				if navigation.is_in_boundsv(candidate) and not navigation.is_point_solid(candidate):
					var d := Vector2(candidate - start).length_squared()
					if d < distance:
						closest = candidate
						distance = d
		end = closest
	if navigation.is_point_solid(start) or navigation.is_point_solid(end):
		return PackedVector2Array()
	var path := navigation.get_point_path(start, end, true)
	if not path.is_empty():
		path.remove_at(0)
	return path

func separation(subject: ArenaActor) -> Vector2:
	var result := Vector2.ZERO
	# Local encounter roster only; no whole-world tree queries.
	for other in actors:
		if other == subject or other.dead:
			continue
		var offset := subject.position - other.position
		var length := offset.length()
		if length < 37 and length > 0.01:
			result += offset / length * (1 - length / 37)
	return result

func on_impact(target: Node2D, amount: int, blocked: bool, heavy_hit: bool, source: ArenaActor) -> void:
	impacts += 1
	var effect := preload("res://game/presentation/combat_feedback.gd").new()
	effect.position = target.global_position + Vector2(0, -15)
	effect.direction = source.position.direction_to(target.position)
	effect.amount = amount
	effect.blocked = blocked
	effect.heavy = heavy_hit
	effect.show_number = preferences.damage_numbers
	effect.intensity = preferences.effects
	effect.z_index = 10
	add_child(effect)
	play_sound("block" if blocked else ("heavy" if heavy_hit else "hit"), target.position)
	if heavy_hit and not blocked:
		shake_left = 0.16 * preferences.effects
	record_event("%s → %s  %s" % [source.name, target.name, "blocked" if blocked else str(amount)])

func play_sound(kind: String, pos: Vector2) -> void:
	if test_mode or not audio.has(kind):
		return
	var sound := AudioStreamPlayer2D.new()
	sound.stream = audio[kind]
	sound.position = pos
	sound.volume_db = linear_to_db(maxf(0.001, preferences.volume)) - 9
	sound.max_distance = 1500
	add_child(sound)
	sound.finished.connect(sound.queue_free)
	sound.play()

func record_event(text: String) -> void:
	recent_events.push_front(text)
	if recent_events.size() > 6:
		recent_events.pop_back()

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
