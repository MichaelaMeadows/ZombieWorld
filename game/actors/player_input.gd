extends Node
## Decision source only; no damage or attack timing in this controller.
var actor: ArenaActor

func _ready() -> void:
	actor = get_parent()
	process_physics_priority = -10 # Publish movement/aim before the shared actor tick.

func _physics_process(_delta: float) -> void:
	if get_viewport().gui_get_focus_owner() != null:
		actor.intent = Vector2.ZERO
		actor.run_intent = false
		actor.combat.block_intent = false
		return
	actor.intent = Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
	actor.run_intent = Input.is_action_pressed("aw_run")
	actor.aim_at(actor.get_global_mouse_position())
	actor.combat.block_intent = Input.is_action_pressed("aw_block")

func _unhandled_input(event: InputEvent) -> void:
	if actor.dead or get_viewport().gui_get_focus_owner() != null:
		return
	actor.aim_at(actor.get_global_mouse_position())
	if event.is_action_pressed("aw_light"):
		actor.combat.request_attack(actor.light_attack)
	elif event.is_action_pressed("aw_heavy"):
		actor.combat.request_attack(actor.heavy_attack)
	elif event.is_action_pressed("aw_dodge"):
		actor.combat.request_dodge(actor.intent)
