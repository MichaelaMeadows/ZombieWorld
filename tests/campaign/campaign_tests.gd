extends Node
var failures := 0
var assertions := 0
const CHECKPOINT := "user://campaign-test/world.json"

func check(value: bool, description: String) -> void:
	assertions += 1
	if not value:
		failures += 1
		push_error("FAIL: " + description)
	else:
		print("PASS: " + description)

func _ready() -> void:
	call_deferred("_run")

func _run() -> void:
	check(ClassDB.class_exists("CampaignBridge"), "native extension loaded")
	if CampaignSession.bridge == null:
		get_tree().quit(1)
		return
	var start := Time.get_ticks_msec()
	var created: Dictionary = CampaignSession.new_world(42, "concord_heir")
	check(created.accepted, "fresh imported world generated")
	if not created.accepted:
		print(created)
		get_tree().quit(1)
		return
	var native: Object = CampaignSession.bridge
	var s: Dictionary = native.summary()
	check(s.areas == 6069 and s.land_areas == 4175 and s.water_areas == 1894, "full Duvaleia geography retained")
	check(s.realms == 6 and s.population > 0, "six fresh populated empires")
	check(s.evolution_enabled and s.economy_enabled and s.politics_enabled and s.bandits_enabled, "coupled simulation policies enabled")
	var person: Dictionary = native.inspect_person(s.player_id)
	check(person.parents.size() == 1 and person.affinities.size() == 7, "player lineage and seven affinities")
	check(native.inspect_person(person.parents[0]).alive == false, "origin ancestry is recorded")
	check(native.item_catalog().size() == 3 and native.inventory().items.size() == 3, "three source item definitions and owned items")
	var before: String = native.snapshot_text()
	check(not native.equip_item(0, "99999999999999999999").accepted, "unowned item rejected")
	check(not native.equip_item(0, "-1").accepted and not native.equip_item(0, "1junk").accepted, "malformed IDs rejected without coercion")
	check(native.inspect_person("+1").is_empty(), "noncanonical IDs rejected")
	check(native.snapshot_text() == before, "failed commands preserve state")
	check(CampaignSession.equip(0, "0").accepted, "unequip uses campaign command")
	check(native.inventory().equipment[0] == "0", "unequip updates authoritative inventory")
	check(CampaignSession.equip(0, "1").accepted, "equip owned sword")
	check(not native.equip_item(1, "1").accepted, "wrong-slot item rejected")
	var initial: String = native.snapshot_text()
	var reference: Object = ClassDB.instantiate("CampaignBridge")
	check(reference.restore_snapshot(initial, s.player_id).accepted, "checkpoint reconstructs independent native owner")
	check(not native.advance_days(0).accepted and not native.advance_days(366).accepted, "interactive advance bounds enforced")
	check(CampaignSession.advance(30), "asynchronous campaign advance starts")
	check(not CampaignSession.advance(1) and not CampaignSession.equip(0, "0").accepted, "worker excludes concurrent mutations")
	check(not CampaignSession.save_campaign(CHECKPOINT), "save does not read worker-owned state")
	while CampaignSession.busy:
		await get_tree().process_frame
	for day in range(30):
		var tick: Dictionary = reference.advance_days(1)
		if not tick.accepted:
			check(false, "reference daily step " + str(day))
			break
	check(native.snapshot_text() == reference.snapshot_text(), "batched advance exactly matches daily simulation")
	s = native.summary()
	check(s.day == 30 and s.wages > 0 and s.taxes > 0, "economy advances with campaign clock")
	check(absf(s.money_total - s.initial_money) < 0.01, "currency accounting conserved")
	var advanced_person: Dictionary = native.inspect_person(s.player_id)
	check(advanced_person.affinities == person.affinities, "protected player earns no passive NPC favor")
	check(native.recent_events(5).size() <= 5, "event query bounded")
	check(CampaignSession.save_campaign(CHECKPOINT), "fresh Godot checkpoint saved and validated")
	var saved_state: String = native.snapshot_text()
	check(CampaignSession.advance(1), "advance after checkpoint")
	while CampaignSession.busy:
		await get_tree().process_frame
	check(CampaignSession.load_campaign(CHECKPOINT), "fresh Godot checkpoint loaded")
	check(CampaignSession.bridge.snapshot_text() == saved_state, "new save round trip preserves exact campaign")
	check(CampaignSession.advance(1), "advance for second save generation")
	while CampaignSession.busy:
		await get_tree().process_frame
	check(CampaignSession.save_campaign(CHECKPOINT), "second generation rotates valid backup")
	var broken := FileAccess.open(CHECKPOINT, FileAccess.WRITE)
	broken.store_string("{broken")
	broken.close()
	check(CampaignSession.load_campaign(CHECKPOINT), "corrupt primary recovers backup")
	check(CampaignSession.bridge.snapshot_text() == saved_state, "backup restores prior generation exactly")
	var good: String = CampaignSession.bridge.snapshot_text()
	check(not CampaignSession.bridge.restore_snapshot("bad", s.player_id).accepted, "corrupt native payload rejected")
	check(CampaignSession.bridge.snapshot_text() == good, "corrupt load leaves live world intact")
	check(not CampaignSession.bridge.create_world(CampaignSession.GEOGRAPHY, -1, "ember_heir").accepted, "invalid generation seed rejected")
	check(CampaignSession.bridge.snapshot_text() == good, "failed new world preserves previous campaign")
	var scene: Control = preload("res://game/world/world.tscn").instantiate()
	add_child(scene)
	await get_tree().process_frame
	for index in range(6):
		scene.tabs.select(index)
		scene._render_detail()
		await get_tree().process_frame
		check(scene.detail.get_child_count() > 0, "working inspector tab %d" % index)
	scene.tabs.select(0)
	scene._render_detail()
	await get_tree().process_frame
	check(scene.map.cells.size() == 6069, "map uses authoritative cell polygons")
	check(scene.map.get_global_rect().end.x <= get_viewport().get_visible_rect().size.x, "map within viewport")
	# A rendered invocation captures the real Godot view; headless runs skip rendering.
	if DisplayServer.get_name() != "headless":
		await RenderingServer.frame_post_draw
		var image := get_viewport().get_texture().get_image()
		var capture := "res://docs/verification/campaign-world.png"
		check(image.save_png(capture) == OK, "rendered world capture saved")
	print("CAMPAIGN_RESULT: %d assertions, %d failures, %d ms" % [assertions, failures, Time.get_ticks_msec() - start])
	get_tree().quit(1 if failures else 0)
