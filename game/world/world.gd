extends Control

const DESIGN = preload("res://game/ui/design.gd")
const MAP = preload("res://game/world/world_map.gd")
var map: Control
var status: Label
var metrics: Label
var detail: VBoxContainer
var tabs: OptionButton
var origin: OptionButton
var time_buttons: Array[Button] = []
var selected_area := ""
var selected_person := ""
var current_geometry_id := 0
var reset_dialog: ConfirmationDialog
var sandbox_button: Button

func _ready() -> void:
	theme = DESIGN.theme()
	theme.default_font_size = 16
	_build()
	CampaignSession.changed.connect(_refresh)
	CampaignSession.message.connect(func(text: String): status.text = text)
	CampaignSession.busy_changed.connect(_busy_changed)
	if not CampaignSession.summary.get("initialized", false) and FileAccess.file_exists(CampaignSession.SAVE_PATH):
		CampaignSession.load_campaign()
	_refresh()
	status.text = CampaignSession.last_message

func _build() -> void:
	var background := ColorRect.new()
	background.color = Color("#101c23")
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(background)
	var margins := MarginContainer.new()
	margins.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for edge in ["left", "right", "top", "bottom"]:
		margins.add_theme_constant_override("margin_" + edge, 16)
	add_child(margins)
	var column := VBoxContainer.new()
	margins.add_child(column)
	var header := HBoxContainer.new()
	column.add_child(header)
	var heading := DESIGN.label("DARK LINEAGE  /  Living world", 24)
	heading.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(heading)
	header.add_child(DESIGN.button("Save", func(): CampaignSession.save_campaign()))
	header.add_child(DESIGN.button("Load", func(): CampaignSession.load_campaign()))
	sandbox_button = DESIGN.button("Combat sandbox", func():
		if not CampaignSession.busy:
			get_tree().change_scene_to_file("res://game/arena.tscn"))
	sandbox_button.tooltip_text = "Bring your current warband. F6 returns here. Sandbox losses do not change the campaign."
	header.add_child(sandbox_button)
	metrics = DESIGN.label("Create a fresh world to begin.", 17)
	column.add_child(metrics)
	var body := HSplitContainer.new()
	body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	body.split_offset = 790
	column.add_child(body)
	var map_column := VBoxContainer.new()
	map_column.custom_minimum_size.x = 380
	map_column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	body.add_child(map_column)
	var map_tools := HBoxContainer.new()
	map_column.add_child(map_tools)
	var layer := OptionButton.new()
	for text in ["Political realms", "Population", "Unrest", "Terrain"]:
		layer.add_item(text)
	layer.item_selected.connect(func(index: int):
		map.layer = index
		map.queue_redraw())
	map_tools.add_child(layer)
	map_tools.add_child(DESIGN.button("Fit map", func(): map.fit()))
	var hint := DESIGN.label("Wheel: zoom · Middle drag: pan · Click: inspect", 13, DESIGN.MUTED)
	hint.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	map_tools.add_child(hint)
	map = MAP.new()
	map.size_flags_vertical = Control.SIZE_EXPAND_FILL
	map.area_selected.connect(func(id: String):
		if CampaignSession.busy:
			return
		selected_area = id
		tabs.select(0)
		_render_detail())
	map_column.add_child(map)
	var right := PanelContainer.new()
	right.custom_minimum_size.x = 370
	body.add_child(right)
	var right_column := VBoxContainer.new()
	right.add_child(right_column)
	tabs = OptionButton.new()
	for text in ["Area & economy", "People & lineage", "Items & equipment", "Realms & estates", "World journal", "Warband"]:
		tabs.add_item(text)
	tabs.item_selected.connect(func(_index: int): _render_detail())
	right_column.add_child(tabs)
	var scroll := ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	right_column.add_child(scroll)
	detail = VBoxContainer.new()
	detail.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(detail)
	var footer := HBoxContainer.new()
	column.add_child(footer)
	origin = OptionButton.new()
	origin.add_item("Ember Oath · Strife")
	origin.add_item("Concord · Harmony")
	footer.add_child(origin)
	footer.add_child(DESIGN.button("New world", _request_new))
	for days in [1, 30, 365]:
		var button := DESIGN.button("+ %d day%s" % [days, "" if days == 1 else "s"], func(): CampaignSession.advance(days))
		time_buttons.append(button)
		footer.add_child(button)
	status = DESIGN.label("", 13, DESIGN.MUTED)
	status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	column.add_child(status)
	reset_dialog = ConfirmationDialog.new()
	reset_dialog.title = "Create a fresh world?"
	reset_dialog.dialog_text = "This replaces the current in-memory world. Your saved checkpoint stays available until you save again."
	reset_dialog.confirmed.connect(_new_world)
	add_child(reset_dialog)

func _request_new() -> void:
	if CampaignSession.busy:
		return
	if CampaignSession.summary.get("initialized", false):
		reset_dialog.popup_centered()
	else:
		_new_world()

func _new_world() -> void:
	selected_area = ""
	selected_person = ""
	current_geometry_id = 0
	var key := "ember_heir" if origin.selected == 0 else "concord_heir"
	CampaignSession.new_world(42, key)

func _busy_changed() -> void:
	for button in time_buttons:
		button.disabled = CampaignSession.busy or not CampaignSession.summary.get("initialized", false)
	tabs.disabled = CampaignSession.busy
	sandbox_button.disabled = CampaignSession.busy
	# Avoid reads from the native object while its worker owns the simulation.
	detail.mouse_filter = Control.MOUSE_FILTER_IGNORE if CampaignSession.busy else Control.MOUSE_FILTER_PASS

func _refresh() -> void:
	if CampaignSession.busy:
		return
	var s: Dictionary = CampaignSession.summary
	_busy_changed()
	if not s.get("initialized", false):
		_render_detail()
		return
	metrics.text = "Day %s    ·    %s inhabitants    ·    %s realms    ·    %s living named people    ·    %s active camps" % [
		s.day, _number(s.population), s.realms, s.living_people, s.active_camps]
	var native: Object = CampaignSession.bridge
	var party: Dictionary = CampaignSession.warband()
	sandbox_button.text = "Combat sandbox · %d allies" % party.troops.size()
	if current_geometry_id != native.get_instance_id():
		current_geometry_id = native.get_instance_id()
		map.set_world(native.geometry(), native.areas())
	else:
		map.update_states(native.areas())
	if selected_person.is_empty():
		selected_person = s.player_id
	if selected_area.is_empty():
		selected_area = native.inspect_person(s.player_id).get("area_id", "")
	map.selected = selected_area
	map.queue_redraw()
	_render_detail()

func _clear_detail() -> void:
	for child in detail.get_children():
		detail.remove_child(child)
		child.queue_free()

func _text(value: String, heading: bool = false) -> void:
	var label := DESIGN.label(value, 20 if heading else 15, DESIGN.PAPER if heading else Color("#cbd4c9"))
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	detail.add_child(label)

func _number(value: float) -> String:
	return String.num(value, 0)

func _person_link(person_id: String, prefix: String) -> void:
	if person_id == "0" or person_id.is_empty():
		return
	var person: Dictionary = CampaignSession.bridge.inspect_person(person_id)
	if person.is_empty():
		return
	var button := DESIGN.button(prefix + person.name, func():
		if CampaignSession.busy:
			return
		selected_person = person_id
		tabs.select(1)
		_render_detail())
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	detail.add_child(button)

func _render_detail() -> void:
	if CampaignSession.busy:
		return
	_clear_detail()
	if not CampaignSession.summary.get("initialized", false):
		_text("A continent in motion", true)
		_text("Create a fresh Duvaleia world with the origin below, or load your Godot checkpoint.")
		_text("Population, families, food, markets and rulers evolve when you advance time. Inspect land cells, follow named lineages and manage your inherited equipment.")
		_text("The combat sandbox remains separate. Press F6 there to return to this world.")
		return
	match tabs.selected:
		0: _area_detail()
		1: _people_detail()
		2: _items_detail()
		3: _realms_detail()
		4: _journal_detail()
		5: _warband_detail()

func _warband_detail() -> void:
	var party: Dictionary = CampaignSession.warband()
	_text("Your warband", true)
	_text("%d / %d troops\nAll current members accompany you into the combat sandbox." % [party.troops.size(), party.capacity])
	_text("Local volunteers", true)
	for offer: Dictionary in party.offers:
		_text("%s · %d available\n%s" % [offer.culture, offer.available, offer.approval])
		var button := DESIGN.button("Recruit peasant", func(): CampaignSession.recruit(offer.culture_id))
		button.disabled = offer.available <= 0 or party.troops.size() >= party.capacity
		detail.add_child(button)
	if party.troops.is_empty():
		_text("Your warband is empty. You will enter the sandbox alone.")
	for troop: Dictionary in party.troops:
		_text(troop.name + (" · Trained" if troop.trained else ""), true)
		_text(troop.culture + " · Home area " + troop.home_area_id)
		detail.add_child(DESIGN.button("Dismiss", func(): CampaignSession.dismiss_troop(troop.id)))

func _area_detail() -> void:
	var a: Dictionary = CampaignSession.bridge.inspect_area(selected_area)
	if a.is_empty():
		_text("Select an area", true)
		return
	_text("Area " + a.id, true)
	if a.water:
		_text("Open water\nWater cells have no land population, economy or walking connections.")
		return
	_text(a.realm_name + "\n" + a.owner_name)
	_text("Population  %s\nProsperity  %.0f%%\nUnrest  %.0f%%\nDevelopment  %.0f%%\nTerrain effort  %.1f" % [
		_number(a.population), a.prosperity * 100, a.unrest * 100, a.development * 100, a.terrain_effort])
	_text("Local food & nutrition", true)
	_text("Stored food  %s person-days\nNutrition  %.0f%%\nProduced to date  %s\nConsumed to date  %s" % [
		_number(a.get("food_stored", 0)), a.get("nutrition", 1.0) * 100,
		_number(a.get("food_produced", 0)), _number(a.get("food_consumed", 0))])
	if a.has("food_price"):
		_text("Regional market", true)
		_text("Household cash  %.2f\nProducer cash  %.2f\nFood price  %.4f\nFood coverage  %.0f%%\nWage coverage  %.0f%%" % [
			a.household_cash, a.producer_cash, a.food_price, a.food_coverage * 100, a.wage_coverage * 100])
		var goods: Array = a.get("goods", [])
		if goods.size() == 3:
			_text("Materials  %.1f · Tools  %.1f · Luxuries  %.1f" % goods)
		_text("Market figures are shared by the region. Food stocks above belong to this area.")
	_text("Culture cohorts", true)
	for cohort: Dictionary in a.cohorts:
		_text("%s  ·  %s" % [cohort.culture, _number(cohort.population)])

func _people_detail() -> void:
	var people: Array = CampaignSession.bridge.people()
	var picker := OptionButton.new()
	picker.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	picker.fit_to_longest_item = false
	var selected_index := 0
	for index in people.size():
		var p: Dictionary = people[index]
		picker.add_item(p.name + ("" if p.alive else " †"))
		if p.id == selected_person:
			selected_index = index
	picker.select(selected_index)
	picker.item_selected.connect(func(index: int):
		if not CampaignSession.busy:
			selected_person = people[index].id
			_render_detail())
	detail.add_child(picker)
	var person: Dictionary = CampaignSession.bridge.inspect_person(selected_person)
	if person.is_empty():
		return
	_text(person.name, true)
	_text("%s · Age %.1f years\n%s\nArea %s" % [
		"Living" if person.alive else "Deceased", person.age_days / 365.0, person.culture, person.area_id])
	for parent_id: String in person.parents:
		_person_link(parent_id, "Parent: ")
	_person_link(person.spouse_id, "Spouse: ")
	for child_id: String in person.children:
		_person_link(child_id, "Child: ")
	if person.has("family_wealth"):
		_text("Family estate  %.2f currency" % person.family_wealth)
	_text("Divine lineage", true)
	for path: Dictionary in person.affinities:
		_text("%s  ·  %.2f power\nInherited %.2f · Earned favor %.1f · Transferred %.1f" % [
			path.name, path.power, path.inherited, path.earned_favor, path.transferred_favor])
		if path.source_parent != "0":
			_person_link(path.source_parent, "Source: ")
			_text("Reserved %.2f power · %s" % [path.reserved_power, "Released" if path.released else "Awaiting release"])

func _items_detail() -> void:
	_text("Inherited equipment", true)
	var inventory: Dictionary = CampaignSession.bridge.inventory()
	var catalog: Array = CampaignSession.bridge.item_catalog()
	_text("Origin: " + str(inventory.get("origin", "")).replace("_", " ").capitalize())
	_text("These are campaign item records. Combat sandbox equipment is independent.")
	for carried: Dictionary in inventory.get("items", []):
		for item: Dictionary in catalog:
			if carried.definition != item.id:
				continue
			_text(item.name, true)
			_text("%s\nHealth +%.0f · Essence +%.0f · Damage +%.0f" % [
				["Weapon", "Armor", "Charm"][item.slot], item.health, item.essence, item.damage])
			var equipped: bool = inventory.equipment[item.slot] == carried.id
			var button := DESIGN.button("Unequip" if equipped else "Equip", func():
				CampaignSession.equip(item.slot, "0" if equipped else carried.id))
			detail.add_child(button)

func _realms_detail() -> void:
	var s: Dictionary = CampaignSession.summary
	_text("World economy", true)
	_text("Food stored  %s person-days\nTreasuries  %.1f\nFamily estates  %.1f\nHousehold cash  %.1f\nProducer cash  %.1f\nActive wars  %d" % [
		_number(s.food_stored), s.treasury, s.family_cash, s.household_cash, s.producer_cash, s.active_wars])
	_text("Currency total  %.2f\nInitial currency  %.2f\nFood traded to date  %.1f" % [s.money_total, s.initial_money, s.food_traded])
	for realm: Dictionary in CampaignSession.bridge.realms():
		if realm.retired:
			continue
		_text(realm.name, true)
		_person_link(realm.head_id, "Ruler: ")
		_text("%s people · %s areas\nTreasury %.1f · Legitimacy %.0f%%\nTaxes received %.1f\nPublic services paid %.1f\nReserve target %.1f" % [
			_number(realm.population), realm.areas, realm.get("treasury", 0), realm.get("legitimacy", 0) * 100,
			realm.get("tax_receipts", 0), realm.get("services_paid", 0), realm.get("reserve_target", 0)])

func _journal_detail() -> void:
	_text("World journal", true)
	_text("Latest 60 recorded events. Decisions use campaign days, independent of rendering.")
	for event: Dictionary in CampaignSession.bridge.recent_events(60):
		_text("Day %s  ·  %s" % [event.day, str(event.kind).replace("_", " ").capitalize()], true)
		_text(event.detail)
