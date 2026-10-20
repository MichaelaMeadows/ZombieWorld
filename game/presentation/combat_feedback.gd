extends Node2D
## Transient impact visuals. Audio is cached, deterministic synthesized PCM.
var age := 0.0
var direction := Vector2.RIGHT
var heavy := false
var blocked := false
var amount := 0
var show_number := true
var intensity := 1.0

func _process(delta: float) -> void:
	age += delta
	if age > 0.55:
		queue_free()
	else:
		queue_redraw()

func _draw() -> void:
	var alpha := maxf(0, 1 - age / 0.55)
	var tint := Color("#8fcad6") if blocked else Color("#fff0bb")
	tint.a = alpha * intensity
	var length := (35 if heavy else 22) * age * 5
	for i in range(9):
		var ray := direction.rotated((i - 4) * 0.43)
		var p := ray * length
		draw_line(p, p + ray * 6 * alpha, tint, 2)
	draw_arc(Vector2.ZERO, 6 + age * 45, 0, TAU, 24, Color(tint, alpha * 0.5), 1.5)
	if show_number:
		var text := "BLOCK" if blocked else str(amount)
		var pos := Vector2(-10, -30 - age * 38)
		var font_size := 18 if not heavy else 24
		draw_string_outline(ThemeDB.fallback_font, pos, text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, 4, Color("#19252b"))
		draw_string(ThemeDB.fallback_font, pos, text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, Color("#f4e9d4") if not blocked else Color("#97d1da"))
