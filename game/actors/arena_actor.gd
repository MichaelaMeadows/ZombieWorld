class_name ArenaActor
extends CharacterEntity
## Shared unit adapter. Uses upstream facing and HealthController APIs.
## Deliberately replaces lifecycle hooks tied to the upstream AnimationTree.

@export var entity_id := "hero"
@export var player_controlled := false
@export var human := false
@export var starting_health := 0
@export var team := 0
@export var heavy := false
@export var loadout: WeaponLoadout
@export var archetype: ActorArchetype
@export var modular_rig: ModularSpriteRig
var visual_equipment: Dictionary = {}
var visual_layer_nodes: Dictionary = {}
@export var walk_speed := 155.0
@export var run_speed := 245.0
@export var walk_stride_length := 144.0
@export var run_stride_length := 168.0
var intent := Vector2.ZERO
var run_intent := false
var locomotion_phase := 0.0
var locomotion_state := "Idle"
var locomotion_direction := Vector2.ZERO
var age := 0.0
var dead := false
var body_sprite: Sprite2D
var combat: CombatComponent
var arena: Node2D
var light_attack: AttackDefinition = preload("res://game/content/light.tres")
var heavy_attack: AttackDefinition = preload("res://game/content/heavy.tres")
var brain: Node
var campaign_unit_id := ""
var follow_offset := Vector2(-64, 0)
var flash_left := 0.0
var death_age := 0.0
var hurt_age := 1.0
var reaction_direction := Vector2.ZERO
var movement_velocity := Vector2.ZERO
var flash_material: ShaderMaterial
# Alpha bounds from the 229px atlas cells; align each pose's feet to the body.
const ZOMBIE_FEET = [227, 226, 227, 227, 227, 227, 224, 221, 221, 224, 221, 224, 220, 220, 220, 220, 220, 220, 203, 203, 203, 204, 203, 203, 194, 194, 194, 194, 194, 194]
const GREATSWORD_POSES = preload("res://game/presentation/greatsword_poses.gd")
const LOCOMOTION_POSES = preload("res://game/presentation/locomotion_poses.gd")
const DIAGONAL_POSES = preload("res://game/presentation/diagonal_poses.gd")

func is_human() -> bool:
	return player_controlled or human

func _ready() -> void:
	if not player_controlled and entity_id == "hero":
		entity_id = "actor_%d" % get_instance_id()
	if archetype != null:
		human = true
		if starting_health <= 0:
			starting_health = archetype.health
		walk_speed = archetype.walk_speed
		run_speed = archetype.run_speed
		walk_stride_length = archetype.walk_stride
		run_stride_length = archetype.run_stride
		loadout = archetype.weapon
	if modular_rig == null:
		if archetype != null and archetype.sprite_rig != null:
			modular_rig = archetype.sprite_rig
		elif loadout != null:
			modular_rig = loadout.sprite_rig
	if player_controlled and modular_rig != null:
		if modular_rig.player_walk_stride > 0:
			walk_stride_length = modular_rig.player_walk_stride
		if modular_rig.player_run_stride > 0:
			run_stride_length = modular_rig.player_run_stride
	update_facing_with_movement = false
	max_speed = walk_speed if player_controlled or archetype != null else (105.0 if heavy else 155.0)
	friction = 1900.0
	health_controller = $Health
	body_sprite = $Sprite
	if player_controlled and modular_rig != null:
		# Painted, downscaled art benefits from filtered edges as it moves.
		# Equipment inherits the same filter from its body parent.
		body_sprite.texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR
	health_controller.owner = self
	health_controller.max_hp = starting_health if starting_health > 0 else (100 if player_controlled else (115 if heavy else 66))
	health_controller.hp = health_controller.max_hp
	if is_human():
		body_sprite.texture = preload("res://assets/art/Warrior_Blue.png")
	else:
		body_sprite.texture = preload("res://assets/art/Zombie.png")
		body_sprite.vframes = 5
		body_sprite.scale = Vector2(0.32, 0.32)
		body_sprite.position.y = 0
		body_sprite.offset.y = 114.5 - ZOMBIE_FEET[0]
	if heavy:
		body_sprite.scale *= 1.12
	light_attack = light_attack.duplicate()
	heavy_attack = heavy_attack.duplicate()
	if not is_human():
		light_attack = preload("res://game/content/zombie_swipe.tres").duplicate()
		heavy_attack = preload("res://game/content/zombie_brute_swipe.tres").duplicate()
	if loadout != null:
		light_attack = loadout.primary.duplicate()
		heavy_attack = loadout.secondary.duplicate()
		if loadout.two_handed:
			body_sprite.hframes = 1
			body_sprite.vframes = 1
			body_sprite.scale = Vector2(0.35, 0.35)
			body_sprite.position = Vector2.ZERO
			GREATSWORD_POSES.apply(body_sprite, Vector2i.ZERO)
	flash_material = ShaderMaterial.new()
	flash_material.shader = preload("res://game/presentation/actor_flash.gdshader")
	body_sprite.material = flash_material
	combat = CombatComponent.new()
	combat.armor_resistance = heavy
	add_child(combat)
	combat.died.connect(_die)
	combat.attack_started.connect(func(_attack: AttackDefinition): arena.play_sound("swing", global_position) if arena != null else null)
	combat.phase_changed.connect(func(next: String):
		if next == "Recovery" and not combat.last_swing_hit and arena != null:
			arena.play_sound("miss", global_position)
	)
	combat.hit_received.connect(func(direction: Vector2, blocked: bool):
		flash_left = 0.08 if blocked else 0.13
		reaction_direction = direction
		if not blocked:
			hurt_age = 0.0
		flash_material.set_shader_parameter("flash_color", Color("#8fcad6") if blocked else Color.WHITE)
		flash_material.set_shader_parameter("power", 0.85)
	)
	if archetype != null and archetype.sprites != null:
		body_sprite.position = Vector2.ZERO
		archetype.sprites.present(self)
	if modular_rig != null:
		var requested_rig := modular_rig
		modular_rig = null
		set_visual_rig(requested_rig)
	if arena != null:
		combat.impact.connect(arena.on_impact.bind(self))
		var controller = preload("res://game/actors/player_input.gd") if player_controlled else preload("res://game/ai/melee_brain.gd")
		if not player_controlled and archetype != null and archetype.ranged:
			controller = preload("res://game/ai/ranged_brain.gd")
		brain = controller.new()
		add_child(brain)

func _physics_process(delta: float) -> void:
	if dead:
		death_age += delta
		if modular_rig != null:
			body_sprite.rotation = 0
			modular_rig.present(self)
		elif archetype != null and archetype.sprites != null:
			body_sprite.rotation = 0
			archetype.sprites.present(self)
		else:
			body_sprite.rotation = lerpf(body_sprite.rotation, -PI * 0.5, delta * 8)
		body_sprite.modulate = Color(0.42, 0.43, 0.41, maxf(0.2, 1 - death_age * 0.5))
		queue_redraw()
		return
	var dodge_before := combat.dodge_left
	combat.tick(delta)
	var sprinting := run_intent and intent.length_squared() > 0.01 and combat.can_sprint() and dodge_before <= 0
	if player_controlled or archetype != null:
		max_speed = run_speed if sprinting else walk_speed
	flash_left = maxf(0, flash_left - delta)
	flash_material.set_shader_parameter("power", 0.85 if flash_left > 0 else 0.0)
	if combat.motion_delta > 0:
		var action_delta := combat.motion_delta
		age += action_delta
		hurt_age += action_delta
		var motion := intent
		if arena != null:
			motion += arena.separation(self) * 0.5
		speed_multiplier = 0.38 if combat.is_blocking() else 1.0
		if combat.current != null:
			speed_multiplier = 0.18 if combat.elapsed < combat.current.startup + combat.current.active else 0.65
		if combat.stagger_left > 0:
			motion = Vector2.ZERO
			movement_velocity = Vector2.ZERO
		speed_multiplier *= combat.statuses.movement_scale()
		if combat.statuses.blocks_movement():
			motion = Vector2.ZERO
			movement_velocity = Vector2.ZERO
		# Keep voluntary velocity separate from the impulse so movement friction
		# cannot erase recoil, and use physics time rather than render delta.
		movement_velocity = movement_velocity.move_toward(motion.normalized() * max_speed * speed_multiplier, friction * action_delta)
		if dodge_before > 0 and not combat.statuses.blocks_movement():
			# Include the final partial dodge frame without carrying dash momentum
			# into walking. Travel stays tied to the authored 220 ms duration.
			var dodge_fraction := minf(dodge_before, action_delta) / action_delta
			movement_velocity = motion.normalized() * max_speed * speed_multiplier * (1.0 - dodge_fraction) + combat.dodge_direction * CombatComponent.DODGE_SPEED * dodge_fraction
		velocity = (movement_velocity + combat.knockback_velocity) * (action_delta / delta)
	else:
		velocity = Vector2.ZERO
		movement_velocity = Vector2.ZERO
	var previous_position := position
	move_and_slide()
	for index in get_slide_collision_count():
		movement_velocity = movement_velocity.slide(get_slide_collision(index).get_normal())
	var distance_moved := position.distance_to(previous_position)
	if sprinting and combat.motion_delta > 0 and distance_moved > 0.01 and (player_controlled or archetype != null):
		combat.spend_sprint(combat.motion_delta)
	if dodge_before > 0 and combat.dodge_left <= 0:
		movement_velocity = intent.normalized() * max_speed * speed_multiplier
	if player_controlled:
		_update_player_locomotion(position - previous_position, delta, sprinting, dodge_before > 0)
	else:
		_update_npc_locomotion(distance_moved, sprinting)
	if arena != null:
		combat.resolve_hits(arena.actors)
	_update_sprite()
	queue_redraw()

func _update_npc_locomotion(distance_moved: float, sprinting: bool) -> void:
	locomotion_state = "Idle"
	if distance_moved > 0.01 and intent.length_squared() > 0.01 and combat.current == null and combat.dodge_left <= 0 and combat.stagger_left <= 0 and not combat.statuses.blocks_movement():
		locomotion_state = "Run" if sprinting and velocity.length() > walk_speed * 1.1 else "Walk"
		# Advance by actual distance, so walls, slows and local hit-stop cannot
		# make the feet cycle while the actor is stationary.
		var stride := run_stride_length if locomotion_state == "Run" else walk_stride_length
		locomotion_phase = fmod(locomotion_phase + distance_moved / maxf(stride, 1.0), 1.0)
	elif distance_moved <= 0.01:
		locomotion_phase = 0.0

func _update_player_locomotion(displacement: Vector2, delta: float, sprinting: bool, dodging: bool) -> void:
	if combat.motion_delta <= 0:
		return # Keep the contact pose and phase through local hit-stop.
	var distance := displacement.length()
	var speed_now := distance / maxf(delta, 0.000001)
	if combat.current != null or dodging or combat.stagger_left > 0 or combat.statuses.blocks_movement() or distance < 0.01 or movement_velocity.length() < 1:
		locomotion_state = "Idle"
		return # Retain stride phase when stopped; do not restart the same foot.
	var was_running := locomotion_state == "Run"
	locomotion_state = "Run" if (sprinting or was_running) and speed_now > walk_speed * 1.1 else "Walk"
	var stride := run_stride_length if locomotion_state == "Run" else walk_stride_length
	locomotion_phase = fmod(locomotion_phase + distance / maxf(stride, 1), 1)
	# Four-direction locomotion art covers eight travel directions. Hysteresis
	# avoids alternating side/front poses around a 45-degree diagonal.
	var horizontal := absf(displacement.x) >= absf(displacement.y) * 0.999
	if locomotion_direction.x != 0:
		horizontal = absf(displacement.y) <= absf(displacement.x) * 1.2
	elif locomotion_direction.y != 0:
		horizontal = absf(displacement.x) > absf(displacement.y) * 1.2
	locomotion_direction = Vector2(signf(displacement.x), 0) if horizontal else Vector2(0, signf(displacement.y))

func _process(_delta: float) -> void:
	pass

func aim_at(world_position: Vector2) -> void:
	if combat.current != null and combat.elapsed < combat.current.startup + combat.current.active:
		return # Strike direction stays committed; recovery can aim the next action.
	var direction := world_position - global_position
	if direction.length_squared() > 0.01:
		facing = direction.normalized()

func _update_sprite() -> void:
	body_sprite.position = Vector2.ZERO
	if modular_rig != null:
		modular_rig.present(self)
		_apply_sprite_effects()
		return
	if archetype != null and archetype.sprites != null:
		archetype.sprites.present(self)
		_apply_sprite_effects()
		return
	var greatsword := loadout != null and loadout.two_handed
	var aim := combat.swing_direction if combat.current != null else facing
	var octant := posmod(roundi(aim.angle() / (PI * 0.25)), 8)
	var diagonal := combat.current != null and octant % 2 == 1 and (greatsword or not is_human())
	var frame_count := 4 if greatsword else 6
	var frame_rate := 8.0 if greatsword else (10.0 if is_human() else 7.0)
	var pose := Vector2i(int(age * frame_rate) % frame_count, 1 if velocity.length() > 10 else 0)
	body_sprite.flip_h = facing.x < 0
	if combat.dodge_left > 0:
		pose = Vector2i(mini(frame_count - 1, int(combat.dodge_progress() * frame_count)), 1)
		body_sprite.flip_h = combat.dodge_direction.x < 0
	if greatsword and velocity.length() <= 10 and absf(facing.y) > absf(facing.x):
		# Directional recovery poses also provide an attached-sword resting guard.
		pose = Vector2i(3, 3 if facing.y > 0 else 4)
	if combat.current != null:
		var a := combat.current
		var frame: int
		if greatsword or diagonal:
			# Two anticipation poses, one complete cut, one return to guard.
			frame = mini(1, int(combat.elapsed / a.startup * 2)) if combat.elapsed < a.startup else (2 if combat.elapsed < a.startup + a.active else 3)
			if greatsword and combat.elapsed >= a.startup + a.active and combat.elapsed < a.startup + a.active + a.recovery * 0.35:
				frame = 2
		elif combat.elapsed < a.startup:
			frame = mini(2, int(combat.elapsed / a.startup * 3))
		elif combat.elapsed < a.startup + a.active:
			frame = 3 + mini(1, int((combat.elapsed - a.startup) / a.active * 2))
		else:
			frame = 5
		var row := 2
		if octant == 2:
			row = 4 if is_human() and not greatsword else 3
		elif octant == 6:
			row = 6 if is_human() and not greatsword else 4
		pose = Vector2i(frame, row)
		body_sprite.flip_h = combat.swing_direction.x < 0
	if diagonal:
		var row := (0 if aim.y > 0 else 1) + (0 if is_human() else 2)
		DIAGONAL_POSES.apply(body_sprite, pose.x, row, heavy)
	elif greatsword and combat.dodge_left > 0:
		LOCOMOTION_POSES.apply(body_sprite, combat.dodge_direction, true, mini(3, int(combat.dodge_progress() * 4)))
	elif greatsword and combat.current == null and locomotion_state != "Idle":
		LOCOMOTION_POSES.apply(body_sprite, velocity, locomotion_state == "Run", mini(3, int(locomotion_phase * 4)))
	elif greatsword:
		# The weapon, wrists and arms are authored together in each atlas pose.
		body_sprite.hframes = 1
		body_sprite.vframes = 1
		body_sprite.scale = Vector2.ONE * 0.35
		GREATSWORD_POSES.apply(body_sprite, pose)
		body_sprite.set_meta("animation", "idle" if combat.current == null else "attack")
	else:
		if is_human():
			body_sprite.frame = 0
			body_sprite.texture = preload("res://assets/art/Warrior_Blue.png")
			body_sprite.hframes = 6
			body_sprite.vframes = 8
			body_sprite.scale = Vector2.ONE * (0.784 if heavy else 0.7)
			body_sprite.position = Vector2(0, -21)
			body_sprite.offset = Vector2.ZERO
		else:
			body_sprite.frame = 0
			body_sprite.texture = preload("res://assets/art/Zombie.png")
			body_sprite.hframes = 6
			body_sprite.vframes = 5
			body_sprite.scale = Vector2.ONE * (0.3584 if heavy else 0.32)
			body_sprite.offset.x = 0
		body_sprite.frame_coords = pose
		body_sprite.set_meta("animation", "idle" if combat.current == null else "attack")
		if not is_human():
			body_sprite.offset.y = 114.5 - ZOMBIE_FEET[body_sprite.frame]
	_apply_sprite_effects()

func equip_visual(slot: StringName, layer: ModularSpriteLayer) -> bool:
	if modular_rig == null or slot == &"body":
		return false
	if layer != null and (layer.slot != slot or not modular_rig.accepts(layer)):
		return false
	visual_equipment[slot] = layer
	if is_node_ready():
		_update_sprite()
	return true

func set_visual_rig(rig: ModularSpriteRig) -> bool:
	if rig != null and not rig.validate().is_empty():
		push_warning("Invalid visual rig: " + "; ".join(rig.validate()))
		return false
	modular_rig = rig
	if body_sprite != null:
		body_sprite.self_modulate = Color.WHITE
	visual_equipment.clear()
	for slot in visual_layer_nodes:
		visual_layer_nodes[slot].visible = false
	if body_sprite != null and combat != null:
		_update_sprite()
	return true

func reset_visual_equipment() -> void:
	visual_equipment.clear()
	if is_node_ready() and modular_rig != null:
		_update_sprite()

func _apply_sprite_effects() -> void:
	var prone := combat.statuses.presentation_progress(&"prone")
	body_sprite.rotation = -PI * 0.5 * (1.0 - smoothstep(0.8, 1.0, prone)) if prone >= 0 else 0.0
	if prone < 0 and combat.current != null and loadout != null and loadout.two_handed:
		# Carry the equipped silhouette through a wider windup and sweep, so
		# both hands, armor and blade stay registered throughout the motion.
		var attack := combat.current
		var sweep := 0.0
		var extension := 0.0
		if combat.elapsed < attack.startup:
			sweep = -0.12 * smoothstep(0.0, attack.startup, combat.elapsed)
		elif combat.elapsed < attack.startup + attack.active:
			var progress := smoothstep(0.0, attack.active, combat.elapsed - attack.startup)
			sweep = lerpf(-0.12, 0.18, progress)
			extension = progress
		else:
			var recovery := smoothstep(0.0, attack.recovery, combat.elapsed - attack.startup - attack.active)
			sweep = 0.18 * (1.0 - recovery)
			extension = 1.0 - recovery
		body_sprite.rotation = sweep * (-1.0 if combat.swing_direction.x < 0 else 1.0)
		body_sprite.scale.x *= 1.0 + extension * 0.08
	if prone < 0 and hurt_age < 0.18 and (combat.stagger_left > 0 or combat.hit_pause > 0):
		# A small directional recoil works for every rig, including zombie sheets.
		body_sprite.rotation = -reaction_direction.x * 0.16 * (1.0 - hurt_age / 0.18)
	if combat.dodge_left > 0:
		# Animate the complete equipped silhouette around its foot pivot.
		# The presentation paths restore base transforms before each sample.
		var burst := sin(combat.dodge_progress() * PI)
		body_sprite.scale *= Vector2(1.0 + burst * 0.12, 1.0 - burst * 0.28)
		body_sprite.rotation = combat.dodge_direction.x * 0.32 * burst
		body_sprite.position.y -= burst * 5.0
		body_sprite.set_meta("animation", "dodge")
		body_sprite.modulate = Color(0.7, 0.9, 1.0, 0.8)
	else:
		body_sprite.modulate = Color(0.82, 0.90, 0.74) if heavy and not is_human() else Color.WHITE

func _die() -> void:
	dead = true
	flash_left = 0
	flash_material.set_shader_parameter("power", 0.0)
	collision_layer = 0
	collision_mask = 0
	intent = Vector2.ZERO
	velocity = Vector2.ZERO
	movement_velocity = Vector2.ZERO

func _draw() -> void:
	if dead or combat == null:
		return
	var color := Color("#e9cc8b") if player_controlled else (Color("#83c5db") if team == 0 else Color("#9baa67"))
	draw_arc(Vector2.ZERO, 22, 0, TAU, 48, Color(color, 0.75), 1.5)
	if not player_controlled:
		draw_rect(Rect2(-23, -62, 46, 4), Color("#18252b"))
		draw_rect(Rect2(-23, -62, 46.0 * health_controller.hp / health_controller.max_hp, 4), color)
	if combat.statuses.blocks_actions():
		draw_string(ThemeDB.fallback_font, Vector2(-35, -72), combat.statuses.action_label(), HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color("#19252b"))
	if combat.is_blocking():
		draw_arc(Vector2(0, -8), 30, facing.angle() - 0.9, facing.angle() + 0.9, 24, Color("#8bc5cc"), 3)
	if combat.current != null:
		var a := combat.current
		if a.delivery == AttackDefinition.Delivery.PROJECTILE:
			if combat.phase == "Startup":
				draw_line(Vector2(0, -30), Vector2(0, -30) + combat.swing_direction * 90, Color(color, 0.25 + combat.elapsed / a.startup * 0.4), 1)
			return
		var angle := combat.swing_direction.angle()
		var half_arc := deg_to_rad(a.arc_degrees * 0.5)
		if combat.phase == "Startup":
			var strength := combat.elapsed / a.startup
			draw_arc(Vector2.ZERO, a.reach, angle - half_arc, angle + half_arc, 32, Color(color, 0.2 + strength * 0.65), 2)
			draw_line(Vector2.ZERO, combat.swing_direction.rotated(-half_arc) * a.reach, Color(color, 0.3), 1)
			draw_line(Vector2.ZERO, combat.swing_direction.rotated(half_arc) * a.reach, Color(color, 0.3), 1)
		elif combat.is_active():
			draw_arc(Vector2.ZERO, a.reach, angle - half_arc, angle + half_arc, 32, Color("#fff0bf"), 3)
