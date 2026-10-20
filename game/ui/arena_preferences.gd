class_name ArenaPreferences
extends Resource
## Separate namespace from the preserved upstream demo preferences.
@export var volume := 0.65
@export var effects := 1.0
@export var camera_shake := true
@export var damage_numbers := true
const PATH := "user://briarwatch_preferences.tres"

func save() -> Error:
	return ResourceSaver.save(self, PATH)

static func load_or_create() -> ArenaPreferences:
	if FileAccess.file_exists(PATH):
		var saved := ResourceLoader.load(PATH, "", ResourceLoader.CACHE_MODE_REPLACE)
		if saved is ArenaPreferences:
			return saved
	return ArenaPreferences.new()
