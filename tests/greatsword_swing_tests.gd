extends Node
var failures := 0
var assertions := 0

func check(ok: bool, label: String) -> void:
	assertions += 1
	if not ok:
		failures += 1
		push_error("FAIL: " + label)

func _ready() -> void:
	var hero: ArenaActor = preload("res://game/actors/actor.tscn").instantiate()
	hero.player_controlled = true
	hero.loadout = preload("res://game/content/greatsword_loadout.tres")
	add_child(hero)
	hero.set_physics_process(false)
	var target: ArenaActor = preload("res://game/actors/actor.tscn").instantiate()
	target.team = 1
	target.starting_health = 10000
	add_child(target)
	target.set_physics_process(false)
	for attack in [hero.light_attack, hero.heavy_attack]:
		check(is_equal_approx(attack.arc_degrees, 93.5 if attack == hero.light_attack else 102.0), "greatsword arc reduced by 15 percent")
		hero.combat.current = attack
		for direction in [Vector2.RIGHT, Vector2.LEFT, Vector2.UP, Vector2.DOWN, Vector2(1, 1).normalized()]:
			hero.combat.swing_direction = direction
			hero.combat.elapsed = attack.startup + attack.active * 0.95
			hero._update_sprite()
			check(absf(hero.body_sprite.rotation) > 0.15 and hero.body_sprite.scale.x > hero.modular_rig.display_scale, "equipped strike extends through the swing")
			var pose := hero.body_sprite.transform
			hero._update_sprite()
			check(hero.body_sprite.transform.is_equal_approx(pose), "swing transform does not accumulate on resample")
			for angle in [-1.0, 1.0]:
				for outside in [false, true]:
					var half_arc: float = attack.arc_degrees * 0.5 + (2.0 if outside else -2.0)
					target.position = hero.position + direction.rotated(deg_to_rad(half_arc * angle)) * 70
					target.combat.invulnerability = 0
					hero.combat.hit_pause = 0
					hero.combat.hit_ids.clear()
					var health := target.health_controller.hp
					hero.combat.resolve_hits([target])
					check((target.health_controller.hp == health) == outside, "damage respects both edges of the reduced arc")
			hero.combat.elapsed = attack.startup + attack.active + attack.recovery * 0.2
			hero._update_sprite()
			check(hero.body_sprite.get_meta("pose").x == 2 and not hero.combat.is_active(), "extended follow-through is visual only")
			hero.combat.elapsed = attack.duration() - 0.0001
			hero._update_sprite()
			check(hero.body_sprite.get_meta("pose").x == 3 and absf(hero.body_sprite.rotation) < 0.001, "recovery restores guard and neutral rotation")
		hero.combat.current = null
		hero._update_sprite()
		check(hero.body_sprite.rotation == 0, "idle clears swing transform")
	hero.queue_free()
	target.queue_free()
	print("GREATSWORD_SWING_RESULT: %d assertions, %d failures" % [assertions, failures])
	get_tree().quit(1 if failures else 0)
