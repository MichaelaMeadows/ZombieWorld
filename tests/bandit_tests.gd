extends Node
var failures := 0
var assertions := 0
var arena: Node2D
var hero: ArenaActor
var club: ArenaActor
var archer: ArenaActor

func check(ok: bool, label: String) -> void:
	assertions += 1
	print(("PASS: " if ok else "FAIL: ") + label)
	if not ok:
		failures += 1

func frames(count: int = 2) -> void:
	for i in count:
		await get_tree().physics_frame

func reset(actor: ArenaActor) -> void:
	actor.combat.current = null
	actor.combat.queued = null
	actor.combat.dead = false
	actor.combat.stagger_left = 0
	actor.combat.invulnerability = 0
	actor.combat.hit_pause = 0
	actor.combat.stamina = 100
	actor.combat.block_intent = false
	actor.combat.statuses.clear()
	actor.health_controller.hp = actor.health_controller.max_hp
	actor.dead = false
	actor.hurt_age = 1
	actor.intent = Vector2.ZERO
	actor.velocity = Vector2.ZERO
	actor.locomotion_state = "Idle"

func shoot():
	reset(archer)
	archer.aim_at(hero.global_position)
	archer.combat.request_attack(archer.light_attack)
	archer.combat.tick(archer.light_attack.startup + .01)
	archer.combat.resolve_hits(arena.actors)
	var arrow = get_tree().get_first_node_in_group("combat_projectiles")
	arrow.set_physics_process(false)
	return arrow

func _ready() -> void:
	call_deferred("_run")

func _run() -> void:
	arena = preload("res://game/bandit_arena.tscn").instantiate()
	arena.test_mode = true
	add_child(arena)
	hero = arena.hero
	club = arena.actors[1]
	archer = arena.actors[3]
	for actor: ArenaActor in arena.actors:
		actor.brain.set_physics_process(false)
		actor.set_physics_process(false)
		actor.position = Vector2(1400, 900)
	check(arena.actors.size() == 4, "isolated bandit encounter has player, two clubs and an archer")
	check(club.health_controller.max_hp == 80 and archer.health_controller.max_hp == 65, "archetype health applies")
	check(club.light_attack.delivery == AttackDefinition.Delivery.MELEE and archer.light_attack.delivery == AttackDefinition.Delivery.PROJECTILE, "weapons select melee and ranged delivery")
	for actor in [club, archer]:
		var sprites: ActorSpriteSet = actor.archetype.sprites
		check(sprites.motion_regions.size() == 28 and sprites.combat_regions.size() == 24 and sprites.diagonal_regions.size() == 4, actor.name + " has 56 authored poses")
		for bundle in [[sprites.motion_sheet, sprites.motion_regions], [sprites.combat_sheet, sprites.combat_regions], [sprites.diagonal_sheet, sprites.diagonal_regions]]:
			var bounds := Rect2(Vector2.ZERO, bundle[0].get_size())
			var valid := true
			for region: Rect2 in bundle[1]:
				valid = valid and bounds.encloses(region) and region.has_area()
			check(valid, actor.name + " atlas regions stay inside texture")
		for direction in [Vector2.RIGHT, Vector2.DOWN, Vector2.UP]:
			for state_name in ["Walk", "Run"]:
				actor.velocity = direction * 100
				actor.locomotion_state = state_name
				for frame in 4:
					actor.locomotion_phase = frame / 4.0
					actor._update_sprite()
					check(actor.body_sprite.get_meta("animation") == state_name.to_lower() and actor.body_sprite.get_meta("pose").x == frame, actor.name + " movement pose " + state_name + str(direction) + str(frame))
		actor.locomotion_state = "Idle"
		for octant in 8:
			reset(actor)
			actor.facing = Vector2.RIGHT.rotated(octant * PI / 4)
			actor.combat.request_attack(actor.light_attack)
			actor.combat.elapsed = actor.light_attack.startup + .01
			actor._update_sprite()
			check(actor.body_sprite.get_meta("animation") == "attack" and actor.body_sprite.get_meta("pose").x == 2, actor.name + " directional strike " + str(octant))
		reset(actor)
		actor.hurt_age = .1
		actor._update_sprite()
		check(actor.body_sprite.get_meta("animation") == "hurt", actor.name + " hurt poses")
		actor.dead = true
		actor.death_age = 2
		actor._update_sprite()
		check(actor.body_sprite.get_meta("animation") == "death" and actor.body_sprite.get_meta("pose") == Vector2i(3,5), actor.name + " death holds final pose")
		reset(actor)

	hero.position = Vector2(1000, 540)
	club.position = Vector2(940, 540)
	club.facing = Vector2.RIGHT
	await frames()
	club.combat.request_attack(club.light_attack)
	club.combat.tick(club.light_attack.startup + .01)
	club.combat.resolve_hits(arena.actors)
	check(hero.health_controller.hp == 82, "club deals its authored melee damage")
	club.position = Vector2(1400, 900)
	archer.position = Vector2(800, 540)
	reset(hero)
	await frames()
	var arrow = shoot()
	check(hero.health_controller.hp == 100, "bow release does not apply instant damage")
	archer.combat.resolve_hits(arena.actors)
	check(get_tree().get_nodes_in_group("combat_projectiles").size() == 1, "one arrow per release")
	arrow._physics_process(.1)
	check(is_equal_approx(arrow.position.x, 850) and hero.health_controller.hp == 100, "arrow travels at configured speed")
	arrow._physics_process(.4)
	check(hero.health_controller.hp == 86, "swept arrow hits target once")
	arrow._physics_process(.4)
	check(hero.health_controller.hp == 86, "spent arrow cannot hit again")
	await frames()

	reset(hero)
	hero.facing = Vector2.LEFT
	hero.combat.block_intent = true
	arrow = shoot()
	# Shooter movement after release must not change which side is blockable.
	archer.position = Vector2(1200, 540)
	await frames()
	arrow._physics_process(.5)
	check(hero.health_controller.hp == 100 and hero.combat.stamina < 100, "frontal block uses arrow flight direction")
	await frames()
	archer.position = Vector2(800,540)
	reset(hero)
	hero.combat.invulnerability = .2
	await frames()
	arrow = shoot()
	arrow._physics_process(.5)
	check(hero.health_controller.hp == 100, "dodge immunity rejects arrow damage")
	await frames()

	reset(hero)
	club.position = Vector2(900,540)
	await frames()
	arrow = shoot()
	arrow._physics_process(.5)
	check(hero.health_controller.hp == 100 and club.health_controller.hp == 80 and arrow.spent, "ally blocks arrow without friendly damage")
	await frames()
	club.position = Vector2(1400,900)
	var wall := StaticBody2D.new()
	var shape := CollisionShape2D.new()
	var box := RectangleShape2D.new()
	box.size = Vector2(2,60)
	shape.shape = box
	wall.add_child(shape)
	wall.position = Vector2(810,540)
	arena.add_child(wall)
	await frames()
	arrow = shoot()
	arrow._physics_process(.8)
	check(hero.health_controller.hp == 100 and arrow.spent and arrow.position.x < 812, "swept arrow cannot tunnel through a thin wall beside shooter")
	await frames()
	wall.queue_free()
	await frames()
	hero.position = Vector2(1300,540)
	await frames()
	arrow = shoot()
	arrow._physics_process(2)
	check(hero.health_controller.hp == 100 and arrow.spent and is_equal_approx(arrow.remaining, 0), "arrow expires at maximum reach")
	await frames()

	reset(archer)
	archer.combat.request_attack(archer.light_attack)
	archer.combat.statuses.apply(preload("res://game/content/knockdown.tres"), "test")
	archer.combat.tick(.7)
	archer.combat.resolve_hits(arena.actors)
	check(archer.combat.current == null and get_tree().get_nodes_in_group("combat_projectiles").is_empty(), "knockdown interrupts bow draw before release")
	archer.combat.tick(.3)
	check(archer.combat.can_act(), "archer recovers after one second")
	hero.position = Vector2(880,540)
	archer.brain.engaged = true
	await frames()
	archer.brain._decide()
	check(archer.brain.state == "Retreat" and not archer.brain.path.is_empty(), "archer retreats from close melee threat")
	hero.position = Vector2(1000,620)
	await frames()
	archer.brain.attack_wait = 0
	archer.brain._decide()
	check(archer.combat.current != null and archer.combat.swing_direction.x > .5 and archer.combat.swing_direction.y > .2, "archer commits a diagonal aimed shot")
	reset(archer)
	club.position = Vector2(900,580)
	await frames()
	archer.brain.attack_wait = 0
	archer.brain._decide()
	check(archer.combat.current == null and archer.brain.state == "Reposition", "archer repositions when an ally blocks the shot")

	club.position = Vector2(1400,900)
	reset(hero)
	reset(archer)
	archer.brain.attack_wait = 0
	archer.brain.set_physics_process(true)
	archer.set_physics_process(true)
	await frames(180)
	check(hero.health_controller.hp < 100, "live archer AI winds up, releases and lands an arrow")
	check(archer.position.distance_to(hero.position) > 150, "live archer maintains a ranged position")
	archer.brain.set_physics_process(false)
	archer.set_physics_process(false)
	for projectile in get_tree().get_nodes_in_group("combat_projectiles"):
		projectile.queue_free()
	await frames()
	var temporary: ArenaActor = preload("res://game/actors/bandit_archer.tscn").instantiate()
	add_child(temporary)
	temporary.set_physics_process(false)
	var orphan = preload("res://game/combat/arrow_projectile.gd").new()
	orphan.configure(temporary, temporary.light_attack, Vector2.RIGHT)
	add_child(orphan)
	orphan.set_physics_process(false)
	temporary.queue_free()
	await frames()
	orphan._physics_process(.1)
	check(orphan.is_queued_for_deletion(), "projectile safely expires when its source is freed")

	arena.queue_free()
	await get_tree().process_frame
	print("BANDIT_RESULT: %d assertions, %d failures" % [assertions, failures])
	get_tree().quit(1 if failures else 0)
