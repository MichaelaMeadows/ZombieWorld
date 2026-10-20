"""Build native Godot resource metadata; never modifies the generated PNGs."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONTENT = ROOT / "game/content"

def write(path, text):
    (ROOT / path).write_text(text.strip() + "\n", encoding="utf-8")

def frames(kind, sheet):
    log = (ROOT / f"docs/verification/{kind}-{sheet}-bounds.log").read_text(encoding="utf-8-sig")
    records = json.loads(next(line for line in log.splitlines() if line.startswith("[{")))
    if kind == "club" and sheet == "motion":
        # Two touching vertical run poses were returned as one component.
        merged = next(r for r in records if r["rect"][3] > 250)
        records.remove(merged)
        records += [
            {"rect": [439, 853, 177, 146], "feet": [78, 143]},
            {"rect": [439, 999, 166, 153], "feet": [88, 150]},
        ]
    records.sort(key=lambda r: r["rect"][1])
    result = []
    for start in range(0, len(records), 4):
        result.extend(sorted(records[start:start+4], key=lambda r: r["rect"][0]))
    assert len(result) == (28 if sheet == "motion" else 24)
    for r in result:
        if r["rect"][1] < 0:
            r["rect"][3] += r["rect"][1]
            r["feet"][1] += r["rect"][1]
            r["rect"][1] = 0
    return result

def array(records, field, type_name):
    return f"Array[{type_name}]([" + ", ".join(
        type_name + "(" + ", ".join(str(v) for v in r[field]) + ")" for r in records
    ) + "])"

for kind in ("club", "archer"):
    motion, combat = frames(kind, "motion"), frames(kind, "combat")
    diagonal_log = (ROOT / "docs/verification/bandit-diagonal-bounds.log").read_text(encoding="utf-8-sig")
    diagonal_records = json.loads(next(line for line in diagonal_log.splitlines() if line.startswith("[{")))
    diagonal_records.sort(key=lambda r: r["rect"][1])
    diagonal = sorted(diagonal_records[:4] if kind == "club" else diagonal_records[4:], key=lambda r: r["rect"][0])
    # Airborne boots should not pull the torso sideways between running frames.
    for i, center in enumerate([190, 530, 865, 1202] if kind == "club" else [240, 552, 870, 1170]):
        motion[16+i]["feet"][0] = center - motion[16+i]["rect"][0]
    write(f"game/content/bandit_{kind}_sprites.tres", f"""
[gd_resource type="Resource" script_class="ActorSpriteSet" load_steps=5 format=3]
[ext_resource type="Script" path="res://game/presentation/actor_sprite_set.gd" id="1"]
[ext_resource type="Texture2D" path="res://assets/art/bandits/{kind}_motion.png" id="2"]
[ext_resource type="Texture2D" path="res://assets/art/bandits/{kind}_combat.png" id="3"]
[ext_resource type="Texture2D" path="res://assets/art/bandits/diagonal_attacks.png" id="4"]
[resource]
script = ExtResource("1")
motion_sheet = ExtResource("2")
combat_sheet = ExtResource("3")
motion_scale = {0.40 if kind == "club" else 0.40}
combat_scale = {0.37 if kind == "club" else 0.38}
motion_regions = {array(motion, "rect", "Rect2")}
motion_pivots = {array(motion, "feet", "Vector2")}
combat_regions = {array(combat, "rect", "Rect2")}
combat_pivots = {array(combat, "feet", "Vector2")}
diagonal_sheet = ExtResource("4")
diagonal_regions = {array(diagonal, "rect", "Rect2")}
diagonal_pivots = {array(diagonal, "feet", "Vector2")}
diagonal_scale = {0.20 if kind == "club" else 0.188}
""")

attacks = {
    "club": ('Club swing', .38, .14, .45, 18, 64, 100, 16, .25, 145, False, 0),
    "club_heavy": ('Club smash', .60, .15, .65, 28, 70, 110, 25, .42, 210, True, 0),
    "bow": ('Arrow', .65, .10, .65, 14, 420, 8, 14, .12, 65, False, 1),
}
keys = ["label", "startup", "active", "recovery", "damage", "reach", "arc_degrees",
        "stamina_cost", "stagger", "knockback", "heavy", "delivery"]
for name, values in attacks.items():
    fields = "\n".join(f"{k} = {json.dumps(v)}" for k, v in zip(keys, values))
    write(f"game/content/bandit_{name}_attack.tres", f"""
[gd_resource type="Resource" script_class="AttackDefinition" load_steps=2 format=3]
[ext_resource type="Script" path="res://game/combat/attack_definition.gd" id="1"]
[resource]
script = ExtResource("1")
{fields}
""")
for kind, weapon, attack, secondary in [
    ("club", "Ironbound club", "club", "club_heavy"),
    ("archer", "Hunting bow", "bow", "bow"),
]:
    write(f"game/content/bandit_{kind}_loadout.tres", f"""
[gd_resource type="Resource" script_class="WeaponLoadout" load_steps=4 format=3]
[ext_resource type="Script" path="res://game/combat/weapon_loadout.gd" id="1"]
[ext_resource type="Resource" path="res://game/content/bandit_{attack}_attack.tres" id="2"]
[ext_resource type="Resource" path="res://game/content/bandit_{secondary}_attack.tres" id="3"]
[resource]
script = ExtResource("1")
label = "{weapon}"
primary = ExtResource("2")
secondary = ExtResource("3")
""")
    write(f"game/content/bandit_{kind}.tres", f"""
[gd_resource type="Resource" script_class="ActorArchetype" load_steps=4 format=3]
[ext_resource type="Script" path="res://game/content/actor_archetype.gd" id="1"]
[ext_resource type="Resource" path="res://game/content/bandit_{kind}_loadout.tres" id="2"]
[ext_resource type="Resource" path="res://game/content/bandit_{kind}_sprites.tres" id="3"]
[resource]
script = ExtResource("1")
label = "{'Club bandit' if kind == 'club' else 'Bandit archer'}"
health = {80 if kind == 'club' else 65}
weapon = ExtResource("2")
sprites = ExtResource("3")
ranged = {str(kind == 'archer').lower()}
""")
    write(f"game/actors/bandit_{kind}.tscn", f"""
[gd_scene load_steps=3 format=3]
[ext_resource type="PackedScene" path="res://game/actors/actor.tscn" id="1"]
[ext_resource type="Resource" path="res://game/content/bandit_{kind}.tres" id="2"]
[node name="Bandit{kind.capitalize()}" instance=ExtResource("1")]
team = 1
archetype = ExtResource("2")
""")
write("game/bandit_arena.tscn", """
[gd_scene load_steps=5 format=3]
[ext_resource type="PackedScene" path="res://game/arena.tscn" id="1"]
[ext_resource type="Resource" path="res://game/content/bandit_club.tres" id="2"]
[ext_resource type="Resource" path="res://game/content/bandit_archer.tres" id="3"]
[ext_resource type="Script" path="res://game/content/actor_archetype.gd" id="4"]
[node name="BanditAmbush" instance=ExtResource("1")]
enemy_archetypes = Array[ExtResource("4")]([ExtResource("2"), ExtResource("2"), ExtResource("3")])
encounter_label = "bandits"
encounter_hint = "Two clubs   •   One archer   •   Block or dodge arrows"
include_warband = false
""")
print("Built bandit sprite metadata, weapons, archetypes and reusable scenes.")
