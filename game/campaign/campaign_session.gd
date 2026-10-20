extends Node
## Owns the native campaign across map/sandbox scenes. UI sees committed views only.
signal changed
signal busy_changed
signal message(text: String)

const WORLD_SCENE := "res://game/world/world.tscn"
const GEOGRAPHY := "res://game/content/world/duvaleia.geography"
const SAVE_PATH := "user://campaigns/world.json"
var bridge: Object
var busy := false
var last_message := ""
var summary: Dictionary = {"initialized": false}
var _worker: Thread

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_make_bridge()

func _make_bridge() -> void:
	if ClassDB.class_exists("CampaignBridge"):
		bridge = ClassDB.instantiate("CampaignBridge")
	else:
		notify("World library unavailable. Run tools/build_campaign.ps1 -Bootstrap.")

func notify(text: String) -> void:
	last_message = text
	message.emit(text)

func new_world(seed_value: int = 42, origin: String = "ember_heir") -> Dictionary:
	if busy:
		return {"accepted": false, "code": "busy"}
	if bridge == null:
		_make_bridge()
	if bridge == null:
		return {"accepted": false, "code": "library_missing"}
	var result: Dictionary = bridge.create_world(GEOGRAPHY, seed_value, origin)
	if result.accepted:
		summary = bridge.summary()
		notify("Fresh Duvaleia world created. Time advances when you choose.")
		changed.emit()
	else:
		notify("Could not create world: " + str(result.code))
	return result

func advance(days: int) -> bool:
	if busy or not summary.get("initialized", false):
		return false
	busy = true
	busy_changed.emit()
	notify("Simulating %d campaign days…" % days)
	_worker = Thread.new()
	var error := _worker.start(_advance_worker.bind(days))
	if error != OK:
		_worker = null
		busy = false
		busy_changed.emit()
		notify("Could not start simulation: %s" % error_string(error))
		return false
	return true

func _advance_worker(days: int) -> Dictionary:
	return bridge.advance_days(days)

func _process(_delta: float) -> void:
	if _worker != null and not _worker.is_alive():
		var result: Dictionary = _worker.wait_to_finish()
		_worker = null
		busy = false
		summary = bridge.summary()
		busy_changed.emit()
		notify("Day %d · simulation complete" % summary.day if result.accepted else "Simulation rejected: " + str(result.code))
		changed.emit()

func equip(slot: int, item_id: String) -> Dictionary:
	if busy or bridge == null:
		return {"accepted": false, "code": "busy"}
	var result: Dictionary = bridge.equip_item(slot, item_id)
	if result.accepted:
		summary = bridge.summary()
		notify("Equipment updated.")
		changed.emit()
	else:
		notify("Equipment rejected: " + str(result.code))
	return result

func warband() -> Dictionary:
	if busy or bridge == null or not summary.get("initialized", false):
		return {"troops": [], "offers": []}
	return bridge.warband()

func recruit(culture_id: String, count: int = 1) -> Dictionary:
	if busy or bridge == null:
		return {"accepted": false, "code": "busy"}
	return _party_result(bridge.recruit(culture_id, count), "Recruited %d troop(s)." % count)

func dismiss_troop(troop_id: String) -> Dictionary:
	if busy or bridge == null:
		return {"accepted": false, "code": "busy"}
	return _party_result(bridge.dismiss_troop(troop_id), "Troop dismissed.")

func _party_result(result: Dictionary, success: String) -> Dictionary:
	if result.accepted:
		summary = bridge.summary()
		notify(success)
		changed.emit()
	else:
		notify("Warband request rejected: " + str(result.code))
	return result

func _read_checkpoint(path: String) -> Dictionary:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null or file.get_length() > 64 * 1024 * 1024:
		return {}
	var parser := JSON.new()
	if parser.parse(file.get_as_text()) != OK:
		return {}
	var data: Variant = parser.data
	if not data is Dictionary or data.get("format") != "briarwatch_campaign" or data.get("version") != 1:
		return {}
	if not data.get("world") is String or not data.get("player_id") is String or not data.get("sha256") is String:
		return {}
	if data.world.sha256_text() != data.sha256:
		return {}
	var candidate: Object = ClassDB.instantiate("CampaignBridge")
	var result: Dictionary = candidate.restore_snapshot(data.world, data.player_id)
	if not result.accepted:
		return {}
	return {"bridge": candidate}

func save_campaign(path: String = SAVE_PATH) -> bool:
	if busy or bridge == null or not summary.get("initialized", false):
		return false
	var absolute := ProjectSettings.globalize_path(path)
	var error := DirAccess.make_dir_recursive_absolute(absolute.get_base_dir())
	if error != OK:
		notify("Cannot create save directory: " + error_string(error))
		return false
	var snapshot: String = bridge.snapshot_text()
	var envelope := {"format": "briarwatch_campaign", "version": 1, "player_id": summary.player_id,
		"world": snapshot, "sha256": snapshot.sha256_text()}
	var temporary := path + ".tmp"
	var file := FileAccess.open(temporary, FileAccess.WRITE)
	if file == null:
		notify("Cannot write checkpoint.")
		return false
	file.store_string(JSON.stringify(envelope))
	file.flush()
	var write_error := file.get_error()
	file.close()
	if write_error != OK or _read_checkpoint(temporary).is_empty():
		notify("Checkpoint validation failed; previous save retained.")
		return false
	if FileAccess.file_exists(path):
		if not _read_checkpoint(path).is_empty():
			if FileAccess.file_exists(path + ".bak"):
				error = DirAccess.remove_absolute(absolute + ".bak")
				if error != OK:
					notify("Could not rotate backup: " + error_string(error))
					return false
			error = DirAccess.rename_absolute(absolute, absolute + ".bak")
		else:
			# Preserve a valid previous backup when replacing a damaged primary.
			error = DirAccess.remove_absolute(absolute)
		if error != OK:
			notify("Could not rotate save: " + error_string(error))
			return false
	error = DirAccess.rename_absolute(ProjectSettings.globalize_path(temporary), absolute)
	if error != OK:
		notify("Could not install checkpoint; previous generation remains recoverable.")
		return false
	notify("World saved at day %d." % summary.day)
	return true

func load_campaign(path: String = SAVE_PATH) -> bool:
	if busy or bridge == null:
		return false
	var loaded := _read_checkpoint(path)
	var recovered := false
	if loaded.is_empty():
		loaded = _read_checkpoint(path + ".bak")
		recovered = not loaded.is_empty()
	if loaded.is_empty():
		notify("No valid checkpoint found; current world preserved.")
		return false
	bridge = loaded.bridge
	summary = bridge.summary()
	notify("Recovered previous saved generation." if recovered else "World restored at day %d." % summary.day)
	changed.emit()
	return true

func _unhandled_key_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_F6:
		get_tree().paused = false
		get_tree().change_scene_to_file(WORLD_SCENE)
		get_viewport().set_input_as_handled()

func _exit_tree() -> void:
	if _worker != null:
		_worker.wait_to_finish()
