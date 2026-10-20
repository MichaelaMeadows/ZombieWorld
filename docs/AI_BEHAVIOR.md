# Milestone 2 tactical AI

Three enemies demonstrate two profiles: two agile red raiders and one violet
iron warden. All share the same actor, health and combat code as the player.

The bounded decision states are Guard, Approach, Attack, Recover and Dead.
An enemy activates when the hero is within 290px or enters the sand court.
Every 180 ms it validates the hero, evaluates range and decides whether to approach
or attack. Initial offsets stagger decisions. Attack direction locks at startup.
Attack cooldown includes recovery plus breathing room. The warden is slower,
telegraphs a heavy attack and cannot be interrupted by a light hit.

Paths use the obstacle-inflated static arena grid and disallow diagonal corner
cutting. Near blocked target cells, a nearby walkable endpoint is selected.
Empty paths stop movement. A 600 ms stuck detector requests a new route.
Target death clears the path and returns to Guard. Separation prevents most close
overlap in this small encounter, but is not a large-crowd avoidance solution.

Targeting is currently scoped to the single hero. Allied hostile selection,
retreat profiles and high-level orders belong to milestone 3 and are not claimed
implemented. No global per-frame group/tree searches or behavior-tree dependency.
