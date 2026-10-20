extends Node
const ACTOR = preload("res://game/actors/actor.tscn")
var failures := 0
var assertions := 0

func check(ok: bool, description: String) -> void:
	assertions += 1
	if not ok:
		failures += 1
		push_error("FAIL: " + description)

func make_actor(player: bool) -> ArenaActor:
	var actor: ArenaActor = ACTOR.instantiate()
	actor.player_controlled = player
	actor.human = true
	actor.loadout = preload("res://game/content/greatsword_loadout.tres")
	add_child(actor)
	actor.set_physics_process(false)
	return actor

func step(actor: ArenaActor, direction: Vector2, running: bool, count: int) -> void:
	actor.intent = direction
	actor.run_intent = running
	for frame in count:
		actor._physics_process(1.0 / 60)

func _ready() -> void:
	call_deferred("_run")

func _run() -> void:
	var hero := make_actor(true)
	var npc := make_actor(false)
	npc.position = Vector2(1000, 1000)
	check(hero.modular_rig != null and hero.modular_rig.validate().is_empty(), "player rig and full pose registration validate")
	check(hero.walk_stride_length == 100 and hero.run_stride_length == 132, "player cadence is calibrated without changing movement speeds")
	check(npc.walk_stride_length == 144 and npc.run_stride_length == 168, "NPC cadence is unchanged")
	check(hero.walk_speed == 155 and hero.run_speed == 245, "walk and run speeds are unchanged")
	check(hero.body_sprite.texture_filter == CanvasItem.TEXTURE_FILTER_LINEAR and npc.body_sprite.texture_filter == CanvasItem.TEXTURE_FILTER_PARENT_NODE, "filter polish is player-only")
	for mode in [false, true]:
		for octant in 8:
			hero.position = Vector2(400, 400)
			hero.velocity = Vector2.ZERO
			hero.movement_velocity = Vector2.ZERO
			hero.locomotion_direction = Vector2.ZERO
			hero.combat.stamina = 100
			hero.combat.sprint_exhausted = false
			var direction := Vector2.RIGHT.rotated(octant * PI / 4)
			hero.facing = -direction
			var phase := hero.locomotion_phase
			step(hero, direction, mode, 18)
			check(hero.locomotion_state == ("Run" if mode else "Walk"), "eight-way travel chooses correct movement mode")
			check(hero.locomotion_direction.dot(direction) >= 0.70, "movement pose faces actual travel even when aim is opposite")
			check(hero.facing.is_equal_approx(-direction), "visual locomotion never overwrites combat aim")
			check(hero.locomotion_phase != phase, "actual travel advances gait")
			var index: int = hero.body_sprite.get_meta("atlas_frame")
			for layer: Sprite2D in hero.visual_layer_nodes.values():
				check(layer.get_parent() == hero.body_sprite and layer.get_meta("atlas_frame") == index and layer.flip_h == hero.body_sprite.flip_h, "equipment follows exact body pose and direction")
			step(hero, Vector2.ZERO, false, 1)
			check(hero.locomotion_state != "Idle" and hero.velocity.length() > 1, "release retains stepping during deceleration")
			step(hero, Vector2.ZERO, false, 16)
			check(hero.locomotion_state == "Idle" and hero.velocity.is_zero_approx(), "deceleration settles cleanly to idle")
			phase = hero.locomotion_phase
			var stopped_pose: int = hero.body_sprite.get_meta("atlas_frame")
			step(hero, Vector2.ZERO, false, 30)
			check(is_equal_approx(hero.locomotion_phase, phase) and hero.body_sprite.get_meta("atlas_frame") == stopped_pose, "idle holds stable anatomy and foot phase")
	# Axis changes need a meaningful turn, not tiny movement around the diagonal.
	hero.position = Vector2(400, 400)
	step(hero, Vector2.RIGHT, false, 12)
	for direction in [Vector2(1, 1.02), Vector2(1, 0.98), Vector2(1, 1.03)]:
		step(hero, direction.normalized(), false, 12)
		check(hero.locomotion_direction == Vector2.RIGHT, "near-diagonal travel does not flicker side/front")
	step(hero, Vector2.DOWN, false, 12)
	check(hero.locomotion_direction == Vector2.DOWN, "deliberate turn changes facing")
	step(hero, Vector2.UP, false, 12)
	check(hero.locomotion_direction == Vector2.UP, "reversal faces actual reversed movement")
	hero.combat.stamina = 100
	hero.combat.sprint_exhausted = false
	var prior_phase := hero.locomotion_phase
	var prior_position := hero.position
	step(hero, Vector2.UP, true, 1)
	var expected_phase := fmod(prior_phase + hero.position.distance_to(prior_position) / hero.run_stride_length, 1)
	check(hero.locomotion_state == "Run" and is_equal_approx(hero.locomotion_phase, expected_phase), "walk-to-run changes cadence without restarting the gait")
	step(hero, Vector2.UP, true, 12)
	step(hero, Vector2.UP, false, 6)
	check(hero.locomotion_state == "Walk" and hero.locomotion_phase > 0, "run-to-walk retains phase through deceleration")
	var phase := hero.locomotion_phase
	hero.combat.hit_pause = 0.1
	step(hero, Vector2.UP, false, 3)
	check(is_equal_approx(hero.locomotion_phase, phase) and hero.locomotion_state == "Walk", "local hit-stop preserves locomotion phase and contact pose")
	hero.combat.hit_pause = 0
	step(hero, Vector2.ZERO, false, 12)
	hero.combat.knockback_velocity = Vector2.RIGHT * 80
	step(hero, Vector2.ZERO, false, 1)
	check(hero.locomotion_state == "Idle", "forced recoil does not become a walking cycle")
	hero.combat.knockback_velocity = Vector2.ZERO
	var wall := StaticBody2D.new()
	wall.position = Vector2(600, 400)
	var collision := CollisionShape2D.new()
	var shape := RectangleShape2D.new()
	shape.size = Vector2(20, 200)
	collision.shape = shape
	wall.add_child(collision)
	add_child(wall)
	hero.position = Vector2(560, 400)
	await get_tree().physics_frame
	await get_tree().physics_frame
	step(hero, Vector2.RIGHT, false, 30)
	phase = hero.locomotion_phase
	step(hero, Vector2.RIGHT, false, 20)
	check(hero.locomotion_state == "Idle" and is_equal_approx(hero.locomotion_phase, phase), "pushing into a wall never cycles planted feet")
	# Compare source-foot measurements with whole-rig offsets; no pixels changed.
	var bottoms := [226,223,221,223,230,226,229,227,227,215,228,228,225,204,222,216,228,198,228,203,224,208,225,209]
	for index in range(4, 28):
		var grounded := index < 16 or index % 2 == 0
		var expected := 221 if grounded else 211
		check(is_equal_approx(bottoms[index - 4] + hero.modular_rig.player_pose_offsets[index].y, expected), "registered contact/flight height is consistent across both steps")
	hero.locomotion_state = "Walk"
	hero.locomotion_phase = 0.25
	hero.locomotion_direction = Vector2.DOWN
	hero._update_sprite()
	var equipped_position := hero.body_sprite.position
	hero.equip_visual(&"armor", null)
	hero.equip_visual(&"weapon", null)
	check(hero.body_sprite.position == equipped_position and not hero.visual_layer_nodes[&"armor"].visible and not hero.visual_layer_nodes[&"weapon"].visible, "base body and equipped body retain the same registration")
	npc.locomotion_state = "Walk"
	npc.locomotion_phase = 0.25
	npc.velocity = Vector2.DOWN
	npc._update_sprite()
	check(npc.body_sprite.position == Vector2.ZERO, "player registration does not alter NPC presentation")
	hero.queue_free()
	npc.queue_free()
	wall.queue_free()
	await get_tree().process_frame
	print("PLAYER_LOCOMOTION_RESULT: %d assertions, %d failures" % [assertions, failures])
	get_tree().quit(1 if failures else 0)
