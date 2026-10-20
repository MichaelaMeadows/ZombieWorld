extends Node
const ARENA = preload("res://game/arena.tscn")
var assertions := 0
var failures := 0

func check(value: bool, description: String) -> void:
	assertions += 1
	if not value:
		failures += 1
		push_error("FAIL: " + description)
	else:
		print("PASS: " + description)

func _ready() -> void:
	call_deferred("_run")

func make_arena() -> Node2D:
	var arena: Node2D = ARENA.instantiate()
	arena.test_mode = true
	add_child(arena)
	arena.set_process(false)
	for actor: ArenaActor in arena.actors:
		actor.set_physics_process(false)
		actor.brain.set_physics_process(false)
	return arena

func allies_in(arena: Node2D) -> Array[ArenaActor]:
	var allies: Array[ArenaActor] = []
	for actor: ArenaActor in arena.actors:
		if actor != arena.hero and actor.team == arena.hero.team:
			allies.append(actor)
	return allies

func _run() -> void:
	check(CampaignSession.new_world(42, "concord_heir").accepted, "fresh campaign")
	var party: Dictionary = CampaignSession.warband()
	check(party.troops.is_empty(), "fresh warband starts empty")
	var empty := make_arena()
	check(empty.actors.size() == 4 and allies_in(empty).is_empty(), "empty party spawns hero plus original enemies")
	empty.queue_free()
	await get_tree().process_frame
	var culture := ""
	for offer: Dictionary in party.offers:
		if offer.available >= 3:
			culture = offer.culture_id
			break
	check(not culture.is_empty(), "capital has volunteers for fixture")
	if culture.is_empty():
		get_tree().quit(1)
		return
	var initial_population: int = CampaignSession.summary.population
	check(CampaignSession.recruit(culture, 3).accepted, "recruit three through native command")
	check(CampaignSession.summary.population == initial_population - 3, "recruits leave civilian population")
	party = CampaignSession.warband()
	check(party.troops.size() == 3, "only owned campaign troops returned")
	check(not CampaignSession.recruit(culture, -1).accepted, "invalid recruitment rejected")
	var before: String = CampaignSession.bridge.snapshot_text()
	check(CampaignSession.bridge.restore_snapshot(before, CampaignSession.summary.player_id).accepted, "roster checkpoint restores")
	check(CampaignSession.warband().troops == party.troops, "stable troop identities persist")
	var arena := make_arena()
	var allies := allies_in(arena)
	check(allies.size() == 3 and arena.actors.size() == 7, "all three troops accompany hero")
	var seen := {}
	for ally: ArenaActor in allies:
		check(not ally.player_controlled and ally.human and ally.team == arena.hero.team, "human AI ally uses player team")
		check(ally.campaign_unit_id in party.troops.map(func(t: Dictionary): return t.id), "spawn binds campaign troop ID")
		check(not seen.has(ally.entity_id), "unique combat identity")
		seen[ally.entity_id] = true
		check(ally.modular_rig != null and ally.modular_rig.family == &"one_handed" and ally.modular_rig.body.sheet.resource_path.contains("/one_handed/"), "ally uses the shared human equipment rig")
		check(ally.health_controller.max_hp == 90, "untrained peasant health initialized once")
		var clear := true
		for obstacle: Rect2 in arena.obstacles:
			if obstacle.grow(18).has_point(ally.position):
				clear = false
		check(clear and ally.position.distance_to(arena.hero.position) >= 48, "ally spawns clear of props and hero")
	check(allies[0].position.distance_to(allies[1].position) >= 48, "allies have distinct spaced spawn positions")
	await get_tree().physics_frame
	await get_tree().process_frame
	if DisplayServer.get_name() != "headless":
		await RenderingServer.frame_post_draw
		check(get_viewport().get_texture().get_image().save_png("res://docs/verification/warband-sandbox.png") == OK, "rendered warband captured")
	var ally: ArenaActor = allies[0]
	var friend: ArenaActor = allies[1]
	var enemy: ArenaActor = arena.actors.filter(func(a: ArenaActor): return a.team == 1)[0]
	ally.position = Vector2(650, 540)
	friend.position = Vector2(670, 540)
	enemy.position = Vector2(690, 540)
	arena.hero.position = Vector2(600, 650)
	ally.brain.attack_wait = 0
	ally.brain._decide()
	check(ally.brain.target == enemy, "ally chooses hostile rather than hero or nearby ally")
	check(ally.combat.current != null, "ally requests shared sword attack")
	ally.combat.elapsed = ally.light_attack.startup + 0.01
	ally.combat.swing_direction = Vector2.RIGHT
	var friend_hp: int = friend.health_controller.hp
	var enemy_hp: int = enemy.health_controller.hp
	ally.combat.resolve_hits(arena.actors)
	check(enemy.health_controller.hp < enemy_hp, "ally attack damages enemy")
	check(friend.health_controller.hp == friend_hp, "same-team troop ignores friendly sword arc")
	enemy.brain._decide()
	check(enemy.brain.target == friend, "enemy targets nearest living opponent including allies")
	ally.combat.current = null
	ally.combat.hit_pause = 0
	for actor: ArenaActor in arena.actors:
		if actor.team == 1:
			actor.dead = true
	arena.hero.position = Vector2(800, 650)
	ally.brain._decide()
	check(ally.brain.state == "Follow" and not ally.brain.path.is_empty(), "ally follows hero when no hostiles remain")
	ally.health_controller.hp = 0
	ally._die()
	check(CampaignSession.bridge.snapshot_text() == before, "sandbox fighting and losses do not mutate campaign")
	arena.queue_free()
	await get_tree().process_frame
	var replay := make_arena()
	check(allies_in(replay).size() == 3, "re-entering rebuilds full current party without duplicates")
	replay.queue_free()
	await get_tree().process_frame
	check(CampaignSession.dismiss_troop(party.troops[0].id).accepted, "dismiss validates ownership")
	var reduced := make_arena()
	check(allies_in(reduced).size() == 2, "next entry reflects latest roster")
	reduced.queue_free()
	await get_tree().process_frame
	for troop: Dictionary in CampaignSession.warband().troops:
		CampaignSession.dismiss_troop(troop.id)
	var solo := make_arena()
	check(allies_in(solo).is_empty(), "empty roster again deploys solo")
	solo.queue_free()
	await get_tree().process_frame
	print("WARBAND_RESULT: %d assertions, %d failures" % [assertions, failures])
	get_tree().quit(1 if failures else 0)
