extends Node2D
## Authored island terrain; no procedural world generation.
const TILES = preload("res://assets/art/Tilemap_Flat.png")

func _draw() -> void:
	draw_rect(Rect2(-2000, -2000, 6000, 6000), Color("#3a7883"))
	# 24 x 16 tiles with the pack's authored shoreline edges.
	for y in range(16):
		for x in range(24):
			var tx := 0 if x == 0 else (2 if x == 23 else 1)
			var ty := 0 if y == 0 else (2 if y == 15 else 1)
			draw_texture_rect_region(TILES, Rect2(x * 64, y * 64, 64, 64), Rect2(tx * 64, ty * 64, 64, 64))
	# Sand path and sparring court, using the matching terrain palette.
	for y in range(5, 12):
		for x in range(6, 21):
			if (y < 7 or y > 9) and x < 12:
				continue
			var tx := 0 if x == (6 if y >= 7 and y <= 9 else 12) else (2 if x == 20 else 1)
			var ty := 0 if y == (7 if x < 12 else 5) else (2 if y == (9 if x < 12 else 11) else 1)
			draw_texture_rect_region(TILES, Rect2(x * 64, y * 64, 64, 64), Rect2(320 + tx * 64, ty * 64, 64, 64))
	draw_arc(Vector2(1040, 540), 150, 0, TAU, 96, Color(0.55, 0.43, 0.24, 0.22), 3)
