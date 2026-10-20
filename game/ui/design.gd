class_name ArenaDesign
extends RefCounted

const INK := Color("#19252b")
const PAPER := Color("#eee3c6")
const BRONZE := Color("#b69a62")
const MUTED := Color("#a7b9b0")

static func panel(color: Color = INK, border: Color = BRONZE) -> StyleBoxFlat:
	var s := StyleBoxFlat.new()
	s.bg_color = color
	s.border_color = border
	s.set_border_width_all(1)
	s.set_corner_radius_all(5)
	s.content_margin_left = 20
	s.content_margin_right = 20
	s.content_margin_top = 14
	s.content_margin_bottom = 14
	return s

static func theme() -> Theme:
	var t := Theme.new()
	t.default_font_size = 18
	t.set_color("font_color", "Label", PAPER)
	t.set_color("font_color", "Button", PAPER)
	t.set_color("font_hover_color", "Button", Color.WHITE)
	t.set_stylebox("panel", "PanelContainer", panel())
	t.set_stylebox("panel", "Panel", panel())
	t.set_stylebox("normal", "Button", panel(Color("#2b3b40")))
	t.set_stylebox("hover", "Button", panel(Color("#42534e"), PAPER))
	t.set_stylebox("pressed", "Button", panel(Color("#131f25")))
	t.set_stylebox("focus", "Button", panel(Color(0, 0, 0, 0), PAPER))
	t.set_stylebox("disabled", "Button", panel(Color("#202b30"), Color("#53615d")))
	t.set_constant("separation", "VBoxContainer", 12)
	t.set_constant("separation", "HBoxContainer", 12)
	return t

static func label(text: String, size: int = 18, color: Color = PAPER) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return l

static func button(text: String, callback: Callable) -> Button:
	var b := Button.new()
	b.text = text
	b.custom_minimum_size.y = 44
	b.pressed.connect(callback)
	return b

static func bar(color: Color) -> ProgressBar:
	var b := ProgressBar.new()
	b.show_percentage = false
	b.custom_minimum_size = Vector2(265, 10)
	var background := panel(Color("#0f1a20"), Color("#3b4c4b"))
	background.content_margin_top = 0
	background.content_margin_bottom = 0
	b.add_theme_stylebox_override("background", background)
	var fill := panel(color, color)
	fill.content_margin_top = 0
	fill.content_margin_bottom = 0
	b.add_theme_stylebox_override("fill", fill)
	b.value = 100
	return b
