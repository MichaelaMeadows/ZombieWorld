extends Node
## Decision source only; no damage or attack timing in this controller.
var actor: ArenaActor

func _ready() -> void:
	actor = get_parent()

func _physics_process(_delta: float) -> void:
	if get_viewport().gui_get_focus_owner() != null:
		actor.intent = Vector2.ZERO
		actor.combat.block_intent = false
		return
	actor.intent = Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
	if actor.combat.current == null:
		var aim := actor.global_position.direction_to(actor.get_global_mouse_position())
		actor.facing = Vector2(signf(aim.x), 0) if absf(aim.x) > absf(aim.y) else Vector2(0, signf(aim.y))
	actor.combat.block_intent = Input.is_action_pressed("aw_block")

func _unhandled_input(event: InputEvent) -> void:
	if actor.dead or get_viewport().gui_get_focus_owner() != null:
		return
	if event.is_action_pressed("aw_light"):
		actor.combat.request_attack(actor.light_attack)
	elif event.is_action_pressed("aw_heavy"):
		actor.combat.request_attack(actor.heavy_attack)
	elif event.is_action_pressed("aw_dodge"):
		actor.combat.request_dodge(actor.intent)
