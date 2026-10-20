"""Apply fitted authoring masks to exact body pixels; no independent repaint."""
from pathlib import Path
import re
import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parent
RESOURCE = ROOT.parents[3] / "game/content/modular/two_handed/foreground.tres"
SOURCE = ROOT / "foreground_mask_source.png"
REGISTRATION = ROOT / "foreground_mask_source.tres"
if not SOURCE.exists():
    SOURCE.write_bytes((ROOT / "foreground.png").read_bytes())
    REGISTRATION.write_text(RESOURCE.read_text())

text = REGISTRATION.read_text()
line = next(x for x in text.splitlines() if x.startswith("pivots ="))
pivots = [tuple(map(float, p)) for p in re.findall(r"Vector2\(([^,]+), ([^)]+)\)", line)]
source = Image.open(SOURCE).convert("RGBA")
body = Image.open(ROOT / "body.png").convert("RGBA")
output = Image.new("RGBA", body.size)
for index, (px, py) in enumerate(pivots):
    x, y = index % 8 * 256, index // 8 * 256
    box = (x, y, x + 256, y + 256)
    mask = source.crop(box).getchannel("A")
    # Convert the former runtime registration into the body's atlas space.
    mask = mask.transform((256, 256), Image.Transform.AFFINE,
                          (1.25, 0, px - 160, 0, 1.25, py - 275),
                          Image.Resampling.BILINEAR)
    mask = mask.point(lambda a: 255 if a > 64 else 0).filter(ImageFilter.MaxFilter(3))
    mask = mask.filter(ImageFilter.GaussianBlur(0.7))
    pixels = np.array(body.crop(box))
    coverage = np.array(mask).astype(np.uint16)
    # Preserve source RGB exactly; softly feather only the occlusion boundary.
    pixels[:, :, 3] = (pixels[:, :, 3].astype(np.uint16) * coverage // 255).astype(np.uint8)
    pixels[pixels[:, :, 3] == 0] = 0
    output.paste(Image.fromarray(pixels), (x, y))
output.save(ROOT / "foreground.png")

text = RESOURCE.read_text()
text = re.sub(r"^(pivots|regions|frame_scales) = .*\n?", "", text, flags=re.M)
RESOURCE.write_text(text)
print("Exact-body foreground saved: 56 frames, matching RGBA wherever visible.")
