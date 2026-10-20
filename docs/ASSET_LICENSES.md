# Asset provenance

- Starter code and original demo assets: stesproject/godot-2d-topdown-template,
  MIT, upstream LICENSE retained. Bundled Dialogue Manager retains its license.
- Original Tiny Swords images in assets/art (excluding the generated Hero_*,
  Diagonal_Attacks.png, Zombie.png and the bandits/ and modular/ directories): Pixel Frog, Tiny Swords **Update 010**, downloaded from the official
  itch.io page's **TS_old version_CC0 Licensed** file (dated 2024-01-03).
  https://pixelfrog-assets.itch.io/tiny-swords
  The CC0 old edition is distinct from the current Free Pack, whose redistribution
  restrictions are not suitable for committing raw assets.
  https://creativecommons.org/publicdomain/zero/1.0/
- Only the used subset of the old edition is committed. The original extraction
  is ignored. No paid assets are used.
- New code, UI styles and synthesized audio in game/ are original project work.
- Hero_Greatsword.png and Zombie.png are generated project artwork made with the
  built-in image_gen tool. Generation prompts are saved in their adjacent
  .prompt.txt files. They are not Pixel Frog assets.
- Hero_Locomotion.png, Hero_Locomotion_Vertical.png and Diagonal_Attacks.png are
  also generated with the built-in image_gen tool, referencing those characters.
  Prompts are in Locomotion.prompt.txt and Diagonal_Attacks.prompt.txt.
- All five PNGs in assets/art/bandits/ are generated project artwork made with
  the built-in image_gen tool. Accepted prompts are in that directory's
  GENERATION_PROMPTS.txt. Arrow projectile geometry is original Godot code.
- The modular humanoid body, armor, weapon and foreground sheets under
  assets/art/modular/ are built-in image_gen edits of the generated hero and
  bandit characters. Astra agents at medium reasoning authored the kits against
  the shared 56-pose contract. Exact prompts are saved per family in
  GENERATION_PROMPTS.txt. Reference atlases and QA composites are deterministic
  Godot renders of those sources. Registration scripts measure images and write
  metadata; Godot normalizes atlas dimensions. Original sheets are retained.
- After explicit user authorization, deterministic pixel/mask cleanup aligns
  generated armor to the accepted body, cuts out occluding head/arm pixels,
  and removes detached weapon fragments. Per-family cleanup scripts and source
  copies preserve reproducibility. No external third-party artwork was added.

Do not replace these images with the current Free Pack without revisiting its
source redistribution terms. Preserve this provenance when adding assets.
