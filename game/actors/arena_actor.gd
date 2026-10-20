class_name ArenaActor
extends CharacterEntity
## Tiny Swords adapter. Retains upstream move(), facing and HealthController.
## Deliberately replaces lifecycle hooks tied to the upstream AnimationTree.

@export var entity_id := "hero"
@export var player_controlled := false
@export var team := 0
@export var heavy := false
var intent := Vector2.ZERO
var age := 0.0
var dead := false
var body_sprite: Sprite2D
var combat: CombatComponent
var arena: Node2D
var light_attack: AttackDefinition = preload("res://game/content/light.tres")
var heavy_attack: AttackDefinition = preload("res://game/content/heavy.tres")
var brain: Node
var flash_left := 0.0
var death_age := 0.0
var flash_material: ShaderMaterial

func _ready() -> void:
	update_facing_with_movement = false
	max_speed = 215.0 if player_controlled else (105.0 if heavy else 155.0)
	friction = 1900.0
	health_controller = $Health
	body_sprite = $Sprite
	health_controller.owner = self
	health_controller.max_hp = 100 if player_controlled else (115 if heavy else 66)
	health_controller.hp = health_controller.max_hp
	body_sprite.texture = load("res://assets/art/Warrior_%s.png" % ("Blue" if team == 0 else ("Purple" if heavy else "Red")))
	if heavy:
		body_sprite.scale *= 1.12
	light_attack = light_attack.duplicate()
	heavy_attack = heavy_attack.duplicate()
	if not player_controlled:
		light_attack = preload("res://game/content/enemy_light.tres").duplicate()
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
	health_controller.hp_changed.connect(func(_hp: int): flash_left = 0.13)
	if arena != null:
		combat.impact.connect(arena.on_impact.bind(self))
		brain = (preload("res://game/actors/player_input.gd") if player_controlled else preload("res://game/ai/melee_brain.gd")).new()
		add_child(brain)

func _physics_process(delta: float) -> void:
	if dead:
		death_age += delta
		body_sprite.rotation = lerpf(body_sprite.rotation, -PI * 0.5, delta * 8)
		body_sprite.modulate = Color(0.42, 0.43, 0.41, maxf(0.2, 1 - death_age * 0.5))
		queue_redraw()
		return
	combat.tick(delta)
	flash_left = maxf(0, flash_left - delta)
	flash_material.set_shader_parameter("power", 0.85 if flash_left > 0 else 0.0)
	if combat.hit_pause <= 0:
		age += delta
		var motion := intent
		if arena != null:
			motion += arena.separation(self) * 0.5
		speed_multiplier = 0.38 if combat.is_blocking() else 1.0
		if combat.current != null:
			speed_multiplier = 0.18 if combat.elapsed < combat.current.startup + combat.current.active else 0.65
		if combat.stagger_left > 0:
			motion = Vector2.ZERO
		move(motion)
		if combat.dodge_left > 0:
			velocity = combat.dodge_direction * 480
		velocity += combat.knockback_velocity * delta * 12
	else:
		velocity = Vector2.ZERO
	move_and_slide()
	if arena != null:
		combat.resolve_hits(arena.actors)
	_update_sprite()
	queue_redraw()

func _process(_delta: float) -> void:
	pass

func _update_sprite() -> void:
	body_sprite.frame_coords = Vector2i(int(age * 10.0) % 6, 1 if velocity.length() > 10 else 0)
	body_sprite.flip_h = facing.x < 0
	if combat.current != null:
		var a := combat.current
		var frame: int
		if combat.elapsed < a.startup:
			frame = mini(2, int(combat.elapsed / a.startup * 3))
		elif combat.elapsed < a.startup + a.active:
			frame = 3 + mini(1, int((combat.elapsed - a.startup) / a.active * 2))
		else:
			frame = 5
		var row := 2
		if combat.swing_direction.y > 0:
			row = 4
		elif combat.swing_direction.y < 0:
			row = 6
		body_sprite.frame_coords = Vector2i(frame, row)
		body_sprite.flip_h = combat.swing_direction.x < 0
	if combat.dodge_left > 0:
		body_sprite.modulate = Color(0.7, 0.9, 1.0, 0.65)
	else:
		body_sprite.modulate = Color.WHITE

func _die() -> void:
	dead = true
	flash_left = 0
	flash_material.set_shader_parameter("power", 0.0)
	collision_layer = 0
	collision_mask = 0
	intent = Vector2.ZERO
	velocity = Vector2.ZERO

func _draw() -> void:
	if dead or combat == null:
		return
	var color := Color("#e9cc8b") if player_controlled else Color("#be665e")
	draw_arc(Vector2.ZERO, 22, 0, TAU, 48, Color(color, 0.75), 1.5)
	if not player_controlled:
		draw_rect(Rect2(-23, -62, 46, 4), Color("#18252b"))
		draw_rect(Rect2(-23, -62, 46.0 * health_controller.hp / health_controller.max_hp, 4), color)
	if combat.is_blocking():
		draw_arc(Vector2(0, -8), 30, facing.angle() - 0.9, facing.angle() + 0.9, 24, Color("#8bc5cc"), 3)
	if combat.current != null:
		var a := combat.current
		var angle := combat.swing_direction.angle()
		var half_arc := deg_to_rad(a.arc_degrees * 0.5)
		if combat.phase == "Startup":
			var strength := combat.elapsed / a.startup
			draw_arc(Vector2.ZERO, a.reach, angle - half_arc, angle + half_arc, 32, Color(color, 0.2 + strength * 0.65), 2)
			draw_line(Vector2.ZERO, combat.swing_direction.rotated(-half_arc) * a.reach, Color(color, 0.3), 1)
			draw_line(Vector2.ZERO, combat.swing_direction.rotated(half_arc) * a.reach, Color(color, 0.3), 1)
		elif combat.is_active():
			draw_arc(Vector2.ZERO, a.reach, angle - half_arc, angle + half_arc, 32, Color("#fff0bf"), 3)
