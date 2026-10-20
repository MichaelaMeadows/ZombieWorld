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

func _ready() -> void:
	update_facing_with_movement = false
	max_speed = 215.0
	friction = 1900.0
	health_controller = $Health
	body_sprite = $Sprite
	health_controller.owner = self
	health_controller.max_hp = 100
	health_controller.hp = 100
	body_sprite.texture = load("res://assets/art/Warrior_Blue.png")

func _physics_process(delta: float) -> void:
	age += delta
	if player_controlled:
		intent = Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
		var aim := global_position.direction_to(get_global_mouse_position())
		facing = Vector2(signf(aim.x), 0) if absf(aim.x) > absf(aim.y) else Vector2(0, signf(aim.y))
	move(intent)
	move_and_slide()
	_update_sprite()

func _process(_delta: float) -> void:
	pass

func _update_sprite() -> void:
	body_sprite.frame_coords = Vector2i(int(age * 10.0) % 6, 1 if velocity.length() > 10 else 0)
	body_sprite.flip_h = facing.x < 0

func _draw() -> void:
	if player_controlled:
		draw_arc(Vector2.ZERO, 22, 0, TAU, 48, Color("#e9cc8b"), 1.5)
