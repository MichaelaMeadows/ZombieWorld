# Milestone 2 tactical AI

Three enemies demonstrate two profiles: two shambling zombies and one larger,
dark-green zombie brute. All share the same actor, health and combat code as the player.

The bounded decision states are Guard, Approach, Attack, Recover and Dead.
An enemy activates when the hero is within 290px or enters the sand court.
Every 180 ms it selects the nearest living hostile, evaluates range and decides whether to approach
or attack. Initial offsets stagger decisions. Attack direction locks at startup.
Facing uses the normalized target vector without cardinal snapping. Attack arcs
and hit checks use that exact vector; diagonal swipe art covers NE/SE and mirrors
for NW/SW. The eight visual sectors do not quantize combat direction.
Attack cooldown includes recovery plus 220 ms breathing room (300 ms for the brute).
Both profiles use quick bare-hand swipes. The brute is slower to move, tougher,
and cannot be interrupted by a light hit, but has no long-range sword cleave.

Paths use the obstacle-inflated static arena grid and disallow diagonal corner
cutting. Near blocked target cells, a nearby walkable endpoint is selected.
Within 96px, a clear line of sight permits a direct final approach to the hero's
actual position, avoiding grid endpoints outside hand range. Empty paths otherwise
stop movement. A 600 ms stuck detector requests a new route.
Target death clears the path and returns to Guard. Separation prevents most close
overlap in this small encounter, but is not a large-crowd avoidance solution.

The sandbox now reads the player's campaign warband and spawns human sword allies.
Target selection uses the local roster and team filtering for both sides, so
enemies can attack allies and allies never target the hero. Allies follow a
formation offset near the hero before engagement and after the enemies are gone.
Combat's existing team checks reject friendly damage. Controller ownership is
separate from human/zombie presentation.

Retreat commands and richer high-level orders remain later work. No global
per-frame group/tree searches or behavior-tree dependency. Roster scans and
separation remain intended for small encounters, not a measured army-scale solution.

The bandit archer adds local tactical retreat: it seeks space inside 150px,
draws a shot when it has line of sight within 82% of arrow range, and repositions
otherwise. It checks actors as well as cover before drawing. It can shoot when
cornered rather than remaining passive. Aim locks for the draw/release timeline.
The club bandit uses the shared melee brain and runs during distant approaches.
Both share nearest-hostile selection, staggered decisions, navigation, status
interruptions and combat damage policy. Player-issued retreat orders remain
outside this tactical behavior.
