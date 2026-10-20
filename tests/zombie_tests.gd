extends Node2D
## Run with tools/godot.ps1 --headless res://tests/zombie_tests.tscn.
const ACTOR = preload("res://game/actors/actor.tscn")
var failures := 0
var assertions := 0

func check(ok: bool, label: String) -> void:
	assertions += 1
	if not ok:
		failures += 1
		push_error(label)

func _ready() -> void:
	call_deferred("_run")

func _run() -> void:
	var hero: ArenaActor = ACTOR.instantiate()
	hero.player_controlled = true
	hero.entity_id = "hero"
	hero.position = Vector2(300, 300)
	add_child(hero)
	hero.set_physics_process(false)
	for brute in [false, true]:
		var zombie: ArenaActor = ACTOR.instantiate()
		zombie.entity_id = "brute" if brute else "zombie"
		zombie.team = 1
		zombie.heavy = brute
		zombie.position = Vector2(270, 300)
		add_child(zombie)
		zombie.set_physics_process(false)
		var attack := zombie.heavy_attack if brute else zombie.light_attack
		check(attack.duration() <= 0.43, "both zombie profiles use quick swipes")
		check(zombie.body_sprite.texture.resource_path.ends_with("Zombie.png"), "NPC uses zombie sheet")
		check(zombie.body_sprite.hframes == 6 and zombie.body_sprite.vframes == 5, "atlas slices all 30 poses")
		for direction in [Vector2.RIGHT, Vector2.LEFT, Vector2.DOWN, Vector2.UP]:
			hero.health_controller.hp = 100
			hero.combat.invulnerability = 0
			hero.position = zombie.position + direction * 42
			zombie.facing = direction
			zombie.combat.current = null
			zombie.combat.hit_pause = 0
			zombie.combat.stamina = 100
			zombie.combat.request_attack(attack)
			zombie.combat.resolve_hits([hero])
			check(hero.health_controller.hp == 100, "windup cannot damage")
			zombie.combat.elapsed = attack.startup + 0.01
			zombie._update_sprite()
			var row := 3 if direction == Vector2.DOWN else (4 if direction == Vector2.UP else 2)
			check(zombie.body_sprite.frame_coords.y == row, "swipe uses correct directional row")
			check(zombie.body_sprite.frame_coords.x == 3, "active damage matches hand extension")
			check(zombie.body_sprite.flip_h == (direction == Vector2.LEFT), "left swipe mirrors right")
			zombie.combat.resolve_hits([hero])
			check(hero.health_controller.hp == 100 - attack.damage, "swipe connects at 42px without perfect adjacency")
			zombie.combat.hit_pause = 0
			hero.combat.invulnerability = 0
			zombie.combat.resolve_hits([hero])
			check(hero.health_controller.hp == 100 - attack.damage, "one hit per swipe")
		hero.health_controller.hp = 100
		hero.combat.invulnerability = 0
		zombie.combat.current = null
		zombie.combat.hit_pause = 0
		zombie.combat.stamina = 100
		zombie.facing = Vector2.RIGHT
		hero.position = zombie.position + Vector2(attack.reach + 12.1, 0)
		zombie.combat.request_attack(attack)
		zombie.combat.elapsed = attack.startup + 0.01
		zombie.combat.resolve_hits([hero])
		check(hero.health_controller.hp == 100, "swipe misses immediately outside hand range")
		hero.position = zombie.position + Vector2(30, 0)
		zombie.combat.elapsed = attack.startup + attack.active + 0.01
		zombie.combat.resolve_hits([hero])
		check(hero.health_controller.hp == 100, "recovery cannot damage")
		zombie.queue_free()
	hero.queue_free()
	await get_tree().process_frame
	var arena = preload("res://game/arena.tscn").instantiate()
	arena.test_mode = true
	add_child(arena)
	arena.hero.brain.set_physics_process(false)
	arena.hero.set_physics_process(false)
	arena.hero.position = Vector2(1060, 520)
	var attacker: ArenaActor = arena.actors[1]
	attacker.position = Vector2(1000, 520)
	for other in arena.actors:
		if other != attacker and other != arena.hero:
			other.brain.set_physics_process(false)
			other.set_physics_process(false)
			other.position = Vector2(1400, 800)
	attacker.brain.engaged = true
	attacker.brain.attack_wait = 0
	var captured := false
	for frame in range(150):
		await get_tree().physics_frame
		if not captured and attacker.combat.is_active() and DisplayServer.get_name() != "headless":
			captured = true
			await RenderingServer.frame_post_draw
			get_viewport().get_texture().get_image().save_png("res://docs/verification/zombie-swipe.png")
	check(arena.hero.health_controller.hp < 100, "AI closes off-grid gap and lands a hand swipe")
	arena.queue_free()
	await get_tree().process_frame
	print("ZOMBIE_RESULT: %d assertions, %d failures" % [assertions, failures])
	get_tree().quit(1 if failures else 0)
