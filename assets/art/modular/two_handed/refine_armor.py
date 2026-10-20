"""Bake fitted armor into body space and cut occlusion without repainting."""
from pathlib import Path
import re
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parent
RESOURCE=ROOT.parents[3]/"game/content/modular/two_handed/armor.tres"
SOURCE=ROOT/"armor_fit_source.png"
REGISTRATION=ROOT/"armor_fit_source.tres"
if not SOURCE.exists():
    SOURCE.write_bytes((ROOT/"armor.png").read_bytes())
    REGISTRATION.write_text(RESOURCE.read_text())
text=REGISTRATION.read_text()
line=next(x for x in text.splitlines() if x.startswith("pivots ="))
pivots=[tuple(map(float,p)) for p in re.findall(r"Vector2\(([^,]+), ([^)]+)\)",line)]
armor=Image.open(SOURCE).convert("RGBA")
body=Image.open(ROOT/"body.png").convert("RGBA")
foreground=Image.open(ROOT/"foreground.png").convert("RGBA")
output=Image.new("RGBA",body.size)
for i,(px,py) in enumerate(pivots):
    x,y=i%8*256,i//8*256
    box=(x,y,x+256,y+256)
    fitted=armor.crop(box).transform((256,256),Image.Transform.AFFINE,
        (1/.85,0,px-128/.85,0,1/.85,py-220/.85),Image.Resampling.BICUBIC)
    pixels=np.array(fitted)
    ba=np.array(body.crop(box))[:,:,3].astype(float)/255
    fa=np.array(foreground.crop(box))[:,:,3].astype(float)/255
    # Foreground is already multiplied by body alpha. Recover its mask coverage.
    mask=np.divide(fa,ba,out=np.zeros_like(fa),where=ba>0)
    pixels[:,:,3]=np.rint(pixels[:,:,3]*ba*(1-mask)).astype(np.uint8)
    pixels[pixels[:,:,3]==0]=0
    output.paste(Image.fromarray(pixels),(x,y))
output.save(ROOT/"armor.png")
text=RESOURCE.read_text()
text=re.sub(r"^(pivots|regions|frame_scales) = .*\n?","",text,flags=re.M)
RESOURCE.write_text(text)
rig=ROOT.parents[3]/"game/content/modular/two_handed/hero.tres"
text=rig.read_text().replace(', ExtResource("6")])','])')
text=text.replace('Greatsword hero (calibration candidate)','Greatsword hero')
rig.write_text(text)
print("Armor fitted and cut for exact body: 56 poses; foreground no longer required.")
