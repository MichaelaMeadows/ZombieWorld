extends RefCounted
## Shared atlas playback with immutable textures and mirrored foot pivots.
var textures: Array[AtlasTexture] = []
var feet: Array

func _init(sheet: Texture2D, regions: Array, anchors: Array) -> void:
	feet = anchors
	for region in regions:
		var texture := AtlasTexture.new()
		texture.atlas = sheet
		texture.region = region
		texture.filter_clip = true
		textures.append(texture)

func apply(sprite: Sprite2D, index: int, scale: float) -> void:
	sprite.frame = 0
	sprite.hframes = 1
	sprite.vframes = 1
	sprite.texture = textures[index]
	sprite.scale = Vector2.ONE * scale
	sprite.offset = textures[index].get_size() * 0.5 - feet[index]
	if sprite.flip_h:
		sprite.offset.x = -sprite.offset.x
	sprite.set_meta("atlas_frame", index)
