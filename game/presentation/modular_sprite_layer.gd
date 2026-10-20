class_name ModularSpriteLayer
extends Resource
## All layers in a rig share pose indices and a foot-space origin.
@export var label := ""
@export var family: StringName = &"one_handed"
@export var rig_id: StringName = &"human_one_handed_v1"
@export var slot: StringName = &"weapon"
@export var sheet: Texture2D
@export var columns := 8
@export var rows := 7
@export var pivot := Vector2(0.5, 0.859375)
@export var depth := 2
@export var tint := Color.WHITE
# Optional authored corrections, in texture pixels; never infer each layer's
# position from its alpha bounds (a floating sword has no feet).
@export var regions: Array[Rect2] = []
@export var pivots: Array[Vector2] = []
@export var frame_depths: PackedInt32Array = []
@export var frame_scales: PackedVector2Array = []
@export var frame_rotations: PackedFloat32Array = []
var _frames: Array[AtlasTexture] = []

func frame_count() -> int:
	return regions.size() if not regions.is_empty() else columns * rows

func validate() -> PackedStringArray:
	var errors := PackedStringArray()
	if sheet == null:
		errors.append("%s: missing sheet" % label)
		return errors
	if columns <= 0 or rows <= 0:
		errors.append("%s: invalid grid" % label)
	if not pivots.is_empty() and pivots.size() != frame_count():
		errors.append("%s: pivot count differs from frame count" % label)
	if not frame_depths.is_empty() and frame_depths.size() != frame_count():
		errors.append("%s: depth count differs from frame count" % label)
	if not frame_scales.is_empty() and frame_scales.size() != frame_count():
		errors.append("%s: scale count differs from frame count" % label)
	if not frame_rotations.is_empty() and frame_rotations.size() != frame_count():
		errors.append("%s: rotation count differs from frame count" % label)
	for factor in frame_scales:
		if factor.x <= 0 or factor.y <= 0 or not factor.is_finite():
			errors.append("%s: invalid frame scale" % label)
	var bounds := Rect2(Vector2.ZERO, sheet.get_size())
	for region in regions:
		if not region.has_area() or not bounds.encloses(region):
			errors.append("%s: frame outside texture" % label)
	return errors

func apply(sprite: Sprite2D, index: int, mirrored: bool) -> void:
	if _frames.is_empty():
		var cell := sheet.get_size() / Vector2(columns, rows)
		for i in frame_count():
			var texture := AtlasTexture.new()
			texture.atlas = sheet
			texture.region = regions[i] if not regions.is_empty() else Rect2(Vector2(i % columns, i / columns) * cell, cell)
			texture.filter_clip = true
			_frames.append(texture)
	sprite.frame = 0
	sprite.hframes = 1
	sprite.vframes = 1
	sprite.texture = _frames[index]
	sprite.flip_h = mirrored
	var anchor := pivots[index] if not pivots.is_empty() else _frames[index].get_size() * pivot
	sprite.offset = _frames[index].get_size() * 0.5 - anchor
	if mirrored:
		sprite.offset.x = -sprite.offset.x
	sprite.z_index = frame_depths[index] if not frame_depths.is_empty() else depth
	sprite.scale = frame_scales[index] if not frame_scales.is_empty() else Vector2.ONE
	sprite.rotation = (frame_rotations[index] if not frame_rotations.is_empty() else 0.0) * (-1 if mirrored else 1)
	sprite.self_modulate = tint
	sprite.set_meta("atlas_frame", index)

