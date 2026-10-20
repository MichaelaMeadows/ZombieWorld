extends SceneTree
## Layout-only rendering of existing art. No generated pixels are painted here.
const HERO = preload("res://game/presentation/greatsword_poses.gd")
const MOTION = preload("res://game/presentation/locomotion_poses.gd")
const DIAGONAL = preload("res://game/presentation/diagonal_poses.gd")
const CLIPS = ["idle_e", "walk_e", "walk_s", "walk_n", "run_e", "run_s", "run_n",
	"attack_e", "attack_s", "attack_n", "attack_ne", "hurt", "death", "attack_se"]

func _init() -> void:
	call_deferred("_build")

func _build() -> void:
	for kind in ["two_handed", "one_handed", "bow"]:
		var directory: String = "res://assets/art/modular/" + kind
		DirAccess.make_dir_recursive_absolute(directory)
		var viewport := SubViewport.new()
		viewport.size = Vector2i(2048, 1792)
		viewport.transparent_bg = true
		viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		root.add_child(viewport)
		var bandit: ActorSpriteSet
		if kind != "two_handed":
			bandit = load("res://game/content/bandit_%s_sprites.tres" % ("club" if kind == "one_handed" else "archer"))
			bandit._ensure_atlases()
		for index in 56:
			var sprite := Sprite2D.new()
			var frame := index % 4
			var clip: String = CLIPS[index / 4]
			viewport.add_child(sprite)
			sprite.position = Vector2(index % 8 * 256 + 128, index / 8 * 256 + 220)
			if kind == "two_handed":
				_hero(sprite, clip, frame)
			else:
				_bandit(sprite, bandit, clip, frame)
			sprite.scale *= 2.15
		await process_frame
		await RenderingServer.frame_post_draw
		var error := viewport.get_texture().get_image().save_png(directory + "/reference.png")
		print("REFERENCE: ", kind, " ", error)
		viewport.queue_free()
		await process_frame
	quit()

func _hero(sprite: Sprite2D, clip: String, frame: int) -> void:
	if clip.begins_with("walk") or clip.begins_with("run"):
		var direction := Vector2.RIGHT if clip.ends_with("e") else (Vector2.DOWN if clip.ends_with("s") else Vector2.UP)
		MOTION.apply(sprite, direction, clip.begins_with("run"), frame)
	elif clip in ["attack_ne", "attack_se"]:
		DIAGONAL.apply(sprite, frame, 1 if clip.ends_with("ne") else 0, false)
	else:
		var row := 0
		if clip.begins_with("attack"):
			row = {"attack_e": 2, "attack_s": 3, "attack_n": 4}[clip]
		sprite.scale = Vector2.ONE * 0.35
		HERO.apply(sprite, Vector2i(frame, row))
		if clip == "hurt":
			sprite.rotation = [-0.08, -0.16, -0.08, 0.0][frame]
		elif clip == "death":
			sprite.rotation = -PI * [0.15, 0.3, 0.45, 0.5][frame]
			sprite.position.y -= 12

func _bandit(sprite: Sprite2D, data: ActorSpriteSet, clip: String, frame: int) -> void:
	var group := CLIPS.find(clip)
	if group <= 6:
		data.motion_atlas.apply(sprite, group * 4 + frame, data.motion_scale)
	elif clip == "attack_se":
		data.diagonal_atlas.apply(sprite, frame, data.diagonal_scale)
	else:
		data.combat_atlas.apply(sprite, (group - 7) * 4 + frame, data.combat_scale)
