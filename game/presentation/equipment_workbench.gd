extends Node2D
## Interactive inspection of real layered assets, at battle size and enlarged.
const PROFILES = [
	["Greatsword hero", "two_handed/hero"],
	["Club bandit", "one_handed/club"],
	["Warband swordsman", "one_handed/warband"],
	["Bandit archer", "bow/archer"],
]
const ACTIONS = ["Idle", "Walk", "Run", "Attack", "Hurt", "Death"]
var actors: Array[ArenaActor] = []
var unit_picker: OptionButton
var action_picker: OptionButton
var facing_picker: OptionButton
var armor_toggle: CheckBox
var weapon_toggle: CheckBox
var play_toggle: CheckBox
var scrub: HSlider
var info: Label
var phase := 0.0

func _ready() -> void:
	RenderingServer.set_default_clear_color(Color("#17232d"))
	var canvas := CanvasLayer.new()
	add_child(canvas)
	var margin := MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "top", "right", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 28)
	canvas.add_child(margin)
	var layout := VBoxContainer.new()
	margin.add_child(layout)
	var title := Label.new()
	title.text = "Equipment workbench"
	title.add_theme_font_size_override("font_size", 30)
	layout.add_child(title)
	var description := Label.new()
	description.text = "One animation clock • independent body, armor and weapon layers"
	layout.add_child(description)
	var controls := HBoxContainer.new()
	controls.add_theme_constant_override("separation", 14)
	layout.add_child(controls)
	unit_picker = OptionButton.new()
	for profile in PROFILES:
		unit_picker.add_item(profile[0])
	controls.add_child(unit_picker)
	unit_picker.item_selected.connect(func(_i: int): _select())
	action_picker = OptionButton.new()
	for action in ACTIONS:
		action_picker.add_item(action)
	controls.add_child(action_picker)
	facing_picker = OptionButton.new()
	for direction in ["East", "Southeast", "South", "Southwest", "West", "Northwest", "North", "Northeast"]:
		facing_picker.add_item(direction)
	controls.add_child(facing_picker)
	armor_toggle = CheckBox.new()
	armor_toggle.text = "Armor"
	armor_toggle.button_pressed = true
	controls.add_child(armor_toggle)
	weapon_toggle = CheckBox.new()
	weapon_toggle.text = "Weapon"
	weapon_toggle.button_pressed = true
	controls.add_child(weapon_toggle)
	for toggle in [armor_toggle, weapon_toggle]:
		toggle.toggled.connect(func(_on: bool): _equipment())
	play_toggle = CheckBox.new()
	play_toggle.text = "Play"
	play_toggle.button_pressed = true
	controls.add_child(play_toggle)
	scrub = HSlider.new()
	scrub.min_value = 0
	scrub.max_value = 0.999
	scrub.step = 0.001
	layout.add_child(scrub)
	scrub.value_changed.connect(func(value: float): phase = value)
	info = Label.new()
	info.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	layout.add_child(info)
	for item in [[Vector2(270, 560), "Original"], [Vector2(650, 560), "Layered × 3"], [Vector2(1040, 560), "Battle size"]]:
		var label := Label.new()
		label.text = item[1]
		label.position = item[0] + Vector2(-50, 35)
		canvas.add_child(label)
	_select()

func _select() -> void:
	for actor in actors:
		actor.free()
	actors.clear()
	var index := unit_picker.selected
	var path := "res://game/content/modular/%s.tres" % PROFILES[index][1]
	var rig: ModularSpriteRig = load(path) if ResourceLoader.exists(path) else null
	for i in 3:
		var actor: ArenaActor = preload("res://game/actors/actor.tscn").instantiate()
		actor.player_controlled = true
		if index == 0:
			actor.loadout = preload("res://game/content/greatsword_loadout.tres")
		elif index == 1:
			actor.archetype = preload("res://game/content/bandit_club.tres")
		elif index == 3:
			actor.archetype = preload("res://game/content/bandit_archer.tres")
		else:
			actor.loadout = preload("res://game/content/sword_loadout.tres")
		actor.position = [Vector2(270, 560), Vector2(650, 560), Vector2(1040, 560)][i]
		add_child(actor)
		actor.set_physics_process(false)
		actor.scale = Vector2.ONE * (3 if i < 2 else 1)
		actor.set_visual_rig(rig if i > 0 else null)
		actor.visible = i == 0 or rig != null
		actors.append(actor)
	info.text = ("Asset kit: %s\nToggle equipment to inspect the underlying body. Scrub through every grip and facing." % rig.label) if rig != null else "This asset kit is still being authored."
	_equipment()

func _equipment() -> void:
	for actor in actors:
		if actor.modular_rig == null:
			continue
		actor.reset_visual_equipment()
		if not armor_toggle.button_pressed:
			actor.equip_visual(&"armor", null)
		if not weapon_toggle.button_pressed:
			actor.equip_visual(&"weapon", null)

func _process(delta: float) -> void:
	if play_toggle == null:
		return
	if play_toggle.button_pressed:
		phase = fmod(phase + delta * 0.65, 1.0)
		scrub.set_value_no_signal(phase)
	var action: String = ACTIONS[action_picker.selected]
	var direction := Vector2.RIGHT.rotated(facing_picker.selected * PI * 0.25)
	for actor in actors:
		actor.facing = direction
		actor.age = phase * 4.0 / 3.0
		actor.hurt_age = phase * 0.239 if action == "Hurt" else 1.0
		actor.dead = action == "Death"
		actor.death_age = phase * 1.2
		actor.locomotion_state = action if action in ["Walk", "Run"] else "Idle"
		actor.locomotion_phase = phase
		actor.velocity = direction * 200 if action in ["Walk", "Run"] else Vector2.ZERO
		actor.combat.current = actor.light_attack if action == "Attack" else null
		actor.combat.swing_direction = direction
		actor.combat.elapsed = phase * actor.light_attack.duration()
		actor._update_sprite()
		actor.queue_redraw()
