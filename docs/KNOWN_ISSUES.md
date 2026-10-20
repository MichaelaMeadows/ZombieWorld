# Known limits

- Extended manual combat-feel validation is not complete. Automated replay is
  explicitly not a substitute for the user's repeated timing/positioning tests.
- Upstream emits two leaked ObjectDB instances and one GDScript resource at
  shutdown. Verbose diagnostic identifies scripts/state_machine/states/state.gd.
  It reproduces in the original menu, before our extension; left isolated rather
  than changing third-party state-machine types during combat work.
- Clean-clone demo texture UIDs can warn and fall back to valid file paths.
- Four-direction swings are deliberate. Sprite art is not a full eight-direction
  movement set. The ground arc is the authoritative reach/angle cue.
- The static grid is appropriate to this small authored map; navigation changes
  and large crowds require later profiling. No 20/100-actor target is claimed.
- Enemy selection currently targets the player. Allies, orders and ranged units
  remain deferred to the next milestones.
- No arena save/load, world simulation, inventory UI or full main menu yet.
  Existing template equivalents are preserved but not falsely wired to the arena.
- Settings currently cover feedback/audio only; window/resolution/UI-scale and
  rebinding controls remain milestone 4 work.
- Dead actors remain as faded bodies until restart; no unbounded spawn loop.
- Screenshots recorded through the Windows capture API may be JPEG. The milestone
  1 screenshot extension was corrected after detecting the actual file format.
