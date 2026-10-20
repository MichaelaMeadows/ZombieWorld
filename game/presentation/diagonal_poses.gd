extends RefCounted
## Rows: human SE, human NE, zombie SE, zombie NE. Mirror for western diagonals.
const REGIONS = [
	Rect2(79,46,217,284), Rect2(357,27,273,309), Rect2(717,103,309,225), Rect2(1100,96,250,240),
	Rect2(71,381,250,264), Rect2(404,332,261,316), Rect2(711,345,327,303), Rect2(1091,387,265,256),
	Rect2(93,668,194,209), Rect2(426,650,172,229), Rect2(728,692,272,180), Rect2(1111,677,182,200),
	Rect2(100,899,187,221), Rect2(437,880,177,241), Rect2(758,889,236,229), Rect2(1131,902,170,217),
]
const FEET = [
	Vector2(110,281), Vector2(162,306), Vector2(157,222), Vector2(124.5,237),
	Vector2(105,261), Vector2(112,313), Vector2(112.5,300), Vector2(103,253),
	Vector2(73.5,206), Vector2(74.5,226), Vector2(84,177), Vector2(75,197),
	Vector2(74,218), Vector2(83.5,238), Vector2(81.5,226), Vector2(72.5,214),
]
static var atlas = preload("res://game/presentation/frame_atlas.gd").new(
	preload("res://assets/art/Diagonal_Attacks.png"), REGIONS, FEET)

static func apply(sprite: Sprite2D, frame: int, row: int, heavy: bool) -> void:
	atlas.apply(sprite, row * 4 + frame, 0.30 if row < 2 else (0.3584 if heavy else 0.32))
	sprite.set_meta("animation", "diagonal_attack")
	sprite.set_meta("pose", Vector2i(frame, row))
