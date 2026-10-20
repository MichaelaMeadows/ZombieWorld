extends Control
## Geometry is cached once. Political colors refresh only on committed campaign changes.
signal area_selected(id: String)
const SEA := Color("#142a37")
const REALM_COLORS := [Color("#6d8e77"), Color("#bc9569"), Color("#7e8ba5"),
	Color("#b37967"), Color("#939563"), Color("#a38aa3"), Color("#729d9b")]
var cells: Array = []
var states: Dictionary = {}
var map_size := Vector2.ONE
var selected := ""
var layer := 0
var zoom := 1.0
var pan := Vector2.ZERO
var drag := false
var _index: Dictionary = {}
var _buckets: Dictionary = {}
const BUCKET := 80.0

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	clip_contents = true
	resized.connect(queue_redraw)

func set_world(geometry: Dictionary, area_states: Array) -> void:
	cells = geometry.get("cells", [])
	map_size = geometry.get("size", Vector2.ONE)
	_index.clear()
	_buckets.clear()
	for cell: Dictionary in cells:
		_index[cell.id] = cell
		var bounds := Rect2(cell.polygon[0], Vector2.ZERO)
		for point: Vector2 in cell.polygon:
			bounds = bounds.expand(point)
		for y in range(floori(bounds.position.y / BUCKET), floori(bounds.end.y / BUCKET) + 1):
			for x in range(floori(bounds.position.x / BUCKET), floori(bounds.end.x / BUCKET) + 1):
				var key := Vector2i(x, y)
				if not _buckets.has(key):
					_buckets[key] = []
				_buckets[key].append(cell)
	update_states(area_states)
	fit()

func update_states(area_states: Array) -> void:
	states.clear()
	for area: Dictionary in area_states:
		states[area.id] = area
	queue_redraw()

func fit() -> void:
	zoom = 1.0
	pan = Vector2.ZERO
	queue_redraw()

func scale_factor() -> float:
	return minf((size.x - 32) / map_size.x, (size.y - 32) / map_size.y) * zoom

func map_offset() -> Vector2:
	return (size - map_size * scale_factor()) * 0.5 + pan

func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), SEA)
	if cells.is_empty():
		return
	var factor := scale_factor()
	draw_set_transform(map_offset(), 0, Vector2.ONE * factor)
	for cell: Dictionary in cells:
		var area: Dictionary = states.get(cell.id, {})
		if area.get("water", true):
			continue
		var color := Color("#646b57")
		if layer == 0 and area.get("realm_id", "0") != "0":
			color = REALM_COLORS[int(area.realm_id) % REALM_COLORS.size()]
		elif layer == 1:
			color = Color("#345c4c").lerp(Color("#d3bc86"), clampf(float(area.population) / 900.0, 0, 1))
		elif layer == 2:
			color = Color("#406e59").lerp(Color("#b46955"), float(area.unrest))
		elif layer == 3:
			color = Color("#82795b") if cell.height > 50 else Color("#647e59")
			if cell.height >= 71:
				color = Color("#a9a99a")
		draw_colored_polygon(cell.polygon, color)
		if zoom > 2:
			var outline := PackedVector2Array(cell.polygon)
			outline.append(outline[0])
			draw_polyline(outline, Color(0.08, 0.12, 0.12, 0.23), 0.6 / factor, true)
	if _index.has(selected):
		var polygon := PackedVector2Array(_index[selected].polygon)
		polygon.append(polygon[0])
		draw_polyline(polygon, Color("#f6e4ab"), 2.5 / factor, true)
	draw_set_transform(Vector2.ZERO)

func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_MIDDLE:
			drag = event.pressed
		elif event.pressed and event.button_index in [MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN]:
			var before: Vector2 = (event.position - map_offset()) / scale_factor()
			zoom = clampf(zoom * (1.2 if event.button_index == MOUSE_BUTTON_WHEEL_UP else 1.0 / 1.2), 1, 8)
			pan += event.position - (before * scale_factor() + map_offset())
			queue_redraw()
			accept_event()
		elif event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
			var point: Vector2 = (event.position - map_offset()) / scale_factor()
			var key := Vector2i(floori(point.x / BUCKET), floori(point.y / BUCKET))
			for cell: Dictionary in _buckets.get(key, []):
				if Geometry2D.is_point_in_polygon(point, cell.polygon):
					selected = cell.id
					area_selected.emit(selected)
					queue_redraw()
					break
	elif event is InputEventMouseMotion and drag:
		pan += event.relative
		queue_redraw()
