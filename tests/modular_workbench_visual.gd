extends Node

func _ready() -> void:
	call_deferred("_capture")

func _capture() -> void:
	var workbench := preload("res://game/equipment_workbench.tscn").instantiate()
	add_child(workbench)
	workbench.set_process(false)
	workbench.play_toggle.button_pressed = false
	for index in 4:
		workbench.unit_picker.select(index)
		workbench._select()
		workbench.action_picker.select(3)
		workbench.facing_picker.select(0)
		var attack: AttackDefinition = workbench.actors[1].light_attack
		workbench.phase = (attack.startup + 0.01) / attack.duration()
		workbench.scrub.set_value_no_signal(workbench.phase)
		workbench._process(0)
		await RenderingServer.frame_post_draw
		var destination := "res://docs/verification/modular-workbench-%d.png" % index
		print("WORKBENCH_CAPTURE: ", get_viewport().get_texture().get_image().save_png(destination), " ", destination)
	workbench.queue_free()
	await get_tree().process_frame
	get_tree().quit()
