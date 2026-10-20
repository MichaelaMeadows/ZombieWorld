class_name StatusApplication
extends Resource
@export var effect: StatusEffectDefinition
@export_range(0, 1) var chance := 1.0

func rolls_success(rng: RandomNumberGenerator) -> bool:
	if chance <= 0:
		return false
	if chance >= 1:
		return true
	return rng.randf() < clampf(chance, 0, 1)
