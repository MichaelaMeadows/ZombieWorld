extends SceneTree
func _initialize():
	var rig=load("res://game/content/modular/two_handed/hero.tres")
	var errors=rig.validate()
	print("HERO_RIG_VALIDATION: ",errors)
	for layer in [rig.body]+rig.equipment:
		print(layer.slot,": ",layer.frame_count()," poses ",layer.sheet.get_size())
	quit(0 if errors.is_empty() else 1)
