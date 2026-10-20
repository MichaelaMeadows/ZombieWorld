extends RefCounted
## Measured atlas regions preserve complete blades and consistent foot pivots.
## Native AtlasTextures reference the original artwork without modifying pixels.
const SHEET = preload("res://assets/art/Hero_Greatsword.png")
const REGIONS = [
	Rect2(103,27,231,228), Rect2(439,30,226,225), Rect2(778,30,222,225), Rect2(1122,27,221,229),
	Rect2(90,267,234,224), Rect2(435,265,209,225), Rect2(758,265,227,228), Rect2(1111,268,221,223),
	Rect2(130,503,198,224), Rect2(422,496,247,228), Rect2(695,537,359,188), Rect2(1105,501,229,226),
	Rect2(98,757,209,173), Rect2(428,726,219,204), Rect2(734,757,295,173), Rect2(1103,728,216,202),
	Rect2(111,947,243,180), Rect2(413,925,224,204), Rect2(733,950,306,178), Rect2(1112,926,209,202),
]
const FEET = [
	Vector2(80.5,225), Vector2(80,222), Vector2(79,222), Vector2(79.5,226),
	Vector2(94,221), Vector2(91,222), Vector2(93.5,225), Vector2(91,220),
	Vector2(93.5,221), Vector2(98,225), Vector2(98.5,185), Vector2(90.5,223),
	Vector2(104,170), Vector2(86,201), Vector2(94,170), Vector2(87,199),
	Vector2(98.5,177), Vector2(127,201), Vector2(101.5,175), Vector2(92,199),
]
static var textures: Array[AtlasTexture] = []

static func apply(sprite: Sprite2D, pose: Vector2i) -> void:
	if textures.is_empty():
		for region in REGIONS:
			var texture := AtlasTexture.new()
			texture.atlas = SHEET
			texture.region = region
			texture.filter_clip = true
			textures.append(texture)
	var index := pose.y * 4 + pose.x
	sprite.texture = textures[index]
	sprite.offset = REGIONS[index].size * 0.5 - FEET[index]
	if sprite.flip_h:
		sprite.offset.x = -sprite.offset.x
	sprite.set_meta("pose", pose)
