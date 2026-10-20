# Visual system

Tiny Swords Update 010 CC0 provides all arena art. Original 64px terrain tiles;
192px warrior animation cells render at 0.7 scale with nearest texture filtering.
Characters use side/up/down attacks, mirrored for left. No free-angle rotation of
the sprite. A cardinal ground arc makes the actual attack direction explicit.
The heavy opponent is violet and slightly larger; agile opponents are red.

`game/ui/design.gd` centralizes Theme, StyleBox, buttons, labels and bars.
Palette: charcoal #19252b, parchment #eee3c6, bronze #b69a62,
muted text #a7b9b0, vitality #b96359, stamina #81ad88.
Spacing: 24px outer inset, 20px panel side padding, 12px container gaps.
Body type 16–18px, secondary 14–15px, titles 28–30px. Buttons 44px minimum.
Hover, pressed, focused and disabled styles are shared.

Logical viewport: 1280 × 720, canvas-items stretch. At 1920 × 1080 the same layout
scales to 1.5×. The HUD, pause, settings and tuning overlay are Control/Container
nodes. Modal screens pause the scene and consume input. Closing a modal releases
GUI focus and clears movement/block intent. Tuning sliders retain focus until
the user chooses Return focus to combat or closes the lab.

This is the initial HUD/theme, not the complete milestone 4 component catalogue.
The original template UI remains available in its original demonstration scenes.
