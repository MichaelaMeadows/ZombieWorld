extends RefCounted
## Distinct contact/passing cycles. Shared ground baselines retain run suspension.
const SIDE_REGIONS = [
	Rect2(48,80,387,379), Rect2(574,79,271,380), Rect2(937,83,389,373), Rect2(1469,79,290,380),
	Rect2(57,497,397,346), Rect2(590,497,293,298), Rect2(909,504,452,329), Rect2(1452,501,305,312),
]
const SIDE_PIVOTS = [
	Vector2(156,376), Vector2(64,377), Vector2(158,373), Vector2(66,377),
	Vector2(188,343), Vector2(75,343), Vector2(231,336), Vector2(108,339),
]
static var side = preload("res://game/presentation/frame_atlas.gd").new(
	preload("res://assets/art/Hero_Locomotion.png"), SIDE_REGIONS, SIDE_PIVOTS)
const VERTICAL_REGIONS = [
	Rect2(118,32,193,262), Rect2(465,32,190,261), Rect2(810,25,185,268), Rect2(1138,32,187,259),
	Rect2(117,311,184,270), Rect2(467,305,186,261), Rect2(807,301,190,281), Rect2(1137,304,183,278),
	Rect2(113,585,210,264), Rect2(461,580,202,225), Rect2(806,577,192,275), Rect2(1130,579,195,240),
	Rect2(111,851,209,264), Rect2(462,845,211,245), Rect2(806,839,191,276), Rect2(1134,850,192,234),
]
const VERTICAL_PIVOTS = [
	Vector2(67,259), Vector2(70,259), Vector2(70,266), Vector2(65,259),
	Vector2(68,268), Vector2(68,274), Vector2(73,278), Vector2(66,275),
	Vector2(72,264), Vector2(74,269), Vector2(74,272), Vector2(73,270),
	Vector2(74,261), Vector2(73,267), Vector2(74,273), Vector2(69,262),
]
static var vertical = preload("res://game/presentation/frame_atlas.gd").new(
	preload("res://assets/art/Hero_Locomotion_Vertical.png"), VERTICAL_REGIONS, VERTICAL_PIVOTS)

static func apply(sprite: Sprite2D, direction: Vector2, running: bool, frame: int) -> void:
	if absf(direction.x) >= absf(direction.y):
		sprite.flip_h = direction.x < 0
		side.apply(sprite, frame + (4 if running else 0), 0.205)
	else:
		sprite.flip_h = false
		var row := (0 if direction.y > 0 else 1) + (2 if running else 0)
		vertical.apply(sprite, frame + row * 4, 0.28)
	sprite.set_meta("animation", "run" if running else "walk")
