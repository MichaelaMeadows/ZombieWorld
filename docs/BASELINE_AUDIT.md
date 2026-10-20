# Baseline audit — 2026-10-20

Upstream: https://github.com/stesproject/godot-2d-topdown-template
Commit: ea95789514fe28532d4ed444bdd06a9d4cddad31 (v1.9.4), MIT.
README requires Godot 4.4+ and explicitly lists 4.7.2 as latest tested.
Installed/tested: 4.7.2.stable.mono.official.ed1daf0bf, Compatibility renderer.

## Unmodified verification

Imported before source changes. Headless menu boots. Graphical menu and New Game
successfully open playground_01; screenshot in verification/baseline-demo.jpg.
Movement and attack keys exercised. This is a smoke test, not exhaustive gameplay QA.
Initial sandbox run could not write editor preferences; rerunning with normal user
directory access resolved those errors.

Existing defects: icon UID cannot resolve on a clean clone; texture references in
demo scenes warn about invalid UIDs but successfully fall back to paths. Two
ObjectDB objects and one resource remain at shutdown (also on a headless menu boot).
Targeted repair: use the existing icon.png path in project.godot. No gameplay source
was changed to establish this baseline.

## Reuse inventory

- CharacterEntity: shared CharacterBody2D, acceleration/friction, facing, impulse,
  animation tree, weapon cooldown. Extend for Tiny Swords presentation; reuse move().
- StateMachine / State: inspectable composable states, navigation/follow/idle/input,
  save state index. Keep upstream demos intact.
- HitBox / HurtBox: Area2D damage, collision-layer routing. Existing swings depend
  on upstream animations. New timeline combat must add stamina, arcs, stagger,
  directional block and per-swing deduplication absent from this implementation.
- HealthController: health, recovery immunity, health events and state hooks.
  Compose it into the arena; guard damage before invoking change_hp().
- TargetManager / StateNavigation: targeting and NavigationAgent2D support. The
  stock navigation state updates a target each physics frame; stagger arena AI
  decisions instead.
- InteractionArea / StateInteract: existing interaction detection and callable tasks.
- Inventory / DataItem / DataWeapon: resource-backed items, contents and equipment.
- SceneManager / Transfer: scene transition, loading and destination routing.
- DataManager / SaveFileManager: resource persistence for player and level state.
  Non-player keys are scene-tree paths: unsuitable for persistent populations.
  Defer a versioned stable-ID adapter to milestone 6, do not silently reuse paths.
- UserPrefs: SFX/music/language resource persistence.
- Existing UI: title, settings, inventory, hearts HUD, dialogue and debug window.
  Functional but demo-specific; a shared arena theme can coexist with it.

## Shortest route to milestone 1

Keep upstream directories and demo scenes. Add game/ extensions, a Tiny Swords
CharacterEntity subclass with its own sprite adapter, small authored arena,
colliders and themed HUD. Use the same actor scene for future AI. Then add one
resource-driven sword timeline for both controllers. No settlement simulation yet.

## Integration risks

Upstream CharacterEntity assumes an AnimationTree, Jumper and fall detector.
Override lifecycle hooks deliberately in the presentation adapter, retaining its
movement/facing API. Document that boundary instead of modifying upstream code.
Original save code expects Level and PlayerEntity; arena saving is not wired until
the stable-ID adapter exists. Do not expose a misleading Save button.
Tiny Swords current pack disallows asset redistribution; use its explicitly CC0
old edition (2024-01-03 download) for source-controlled game assets.
