class_name StatusEffectDefinition
extends Resource
## Authored, immutable effect policy. Runtime clocks live in StatusComponent.
@export var id: StringName
@export var label := ""
@export_range(0.01, 60) var duration := 1.0
@export var blocks_actions := false
@export var blocks_movement := false
@export var interrupts_action := false
@export_range(0.0, 3.0) var movement_multiplier := 1.0
@export var presentation: StringName
## Reapplication refreshes the duration; identical IDs never stack modifiers.
@export var refresh_on_reapply := true
