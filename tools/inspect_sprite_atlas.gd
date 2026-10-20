extends SceneTree
## Read-only atlas inspection: finds connected sprite bounds and foot pivots.
## Run --headless --script res://tools/inspect_sprite_atlas.gd -- image_path.
func _init() -> void:
	var args := OS.get_cmdline_user_args()
	var source := Image.load_from_file(args[0])
	var opaque := BitMap.new()
	opaque.create_from_image_alpha(source, float(args[1]) if args.size() > 1 else 0.02)
	var seen := BitMap.new()
	seen.create(source.get_size())
	var sprites: Array[Dictionary] = []
	for y in source.get_height():
		for x in source.get_width():
			var start := Vector2i(x, y)
			if seen.get_bitv(start) or not opaque.get_bitv(start):
				continue
			var todo: Array[Vector2i] = [start]
			seen.set_bitv(start, true)
			var points: Array[Vector2i] = []
			var minimum := start
			var maximum := start
			while not todo.is_empty():
				var point: Vector2i = todo.pop_back()
				points.append(point)
				minimum = minimum.min(point)
				maximum = maximum.max(point)
				for dy in range(-1, 2):
					for dx in range(-1, 2):
						var neighbor := point + Vector2i(dx, dy)
						if neighbor.x < 0 or neighbor.y < 0 or neighbor.x >= source.get_width() or neighbor.y >= source.get_height():
							continue
						if not seen.get_bitv(neighbor) and opaque.get_bitv(neighbor):
							seen.set_bitv(neighbor, true)
							todo.append(neighbor)
			if points.size() < 1000:
				continue
			var foot_min := maximum.x
			var foot_max := minimum.x
			for point in points:
				if point.y > maximum.y - 20:
					foot_min = mini(foot_min, point.x)
					foot_max = maxi(foot_max, point.x)
			sprites.append({
				"rect": [minimum.x - 2, minimum.y - 2, maximum.x - minimum.x + 5, maximum.y - minimum.y + 5],
				"feet": [(foot_min + foot_max) / 2.0 - minimum.x + 2, maximum.y - minimum.y + 2],
				"pixels": points.size()
			})
	print(JSON.stringify(sprites))
	quit()
