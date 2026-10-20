"""Authorized deterministic mask finishing. Original generated sheets are preserved."""
from pathlib import Path
from PIL import Image, ImageFilter, ImageDraw
import numpy as np
import runpy

ART=Path(__file__).parent
SOURCE=ART/"source_generated"
ROOT=ART.parents[3]
OUT=ROOT/"game/content/modular/one_handed"
F=2048/1341
# Metadata generation operates on untouched source sheets.
for name in ("armor","warband_armor","foreground"):
 Image.open(SOURCE/(name+".png")).save(ART/(name+".png"))
ctx=runpy.run_path(str(ART/"calibrate.py"))
hands=ctx["hands"]
necks=ctx["head_necks"]
body=Image.open(SOURCE/"body.png").convert("RGBA")
prior=Image.open(SOURCE/"foreground.png").convert("RGBA")
foreground=Image.new("RGBA",body.size)
masks=[]
back=set(range(12,16))|set(range(24,28))|set(range(36,44))
death=set(range(48,52))
arm_paths={
 **{i:[((70,83),(59,91)),((101,84),(110,97))] for i in range(12,16)},
 24:[((70,85),(63,99)),((105,85),(117,91))],
 25:[((74,92),(60,85)),((108,94),(114,109))],
 26:[((77,90),(66,104)),((107,92),(126,88))],
 27:[((77,90),(66,104)),((107,92),(126,88))],
 34:[((80,101),(99,114))],
 36:[((73,90),(66,101)),((108,91),(123,85))],
 37:[((72,92),(64,106)),((103,82),(105,65))],
 38:[((76,84),(69,96)),((105,83),(128,73))],
 39:[((73,86),(67,100)),((104,89),(117,79))],
 40:[((67,94),(62,106)),((102,98),(115,99))],
 41:[((75,92),(61,73)),((101,95),(111,104))],
 42:[((72,86),(65,100)),((103,87),(126,72))],
 43:[((71,88),(64,103)),((105,91),(119,77))],
 54:[((91,107),(112,119))],
}
for i in range(56):
 x,y=i%8*256,i//8*256
 b=body.crop((x,y,x+256,y+256))
 raw=np.asarray(prior.crop((x,y,x+256,y+256)))[:,:,3]
 mask=Image.fromarray(np.uint8(raw>90)*255).filter(ImageFilter.MaxFilter(7))
 nx,ny=necks[i];hx,hy=hands[i]
 # Remove mistakenly retained shirt torso from the extraction guide, preserving
 # the arm-over-chest region below it. Coordinates are in the original1341 sheet.
 if i in death:
  polygon=[(nx-33,ny-12),(nx-8,ny-10),(nx-2,ny+2),(nx-31,ny+4)]
 elif i in back:
  polygon=[(nx-13,ny+1),(nx+13,ny+1),(nx+16,ny+30),(nx-15,ny+32)]
 else:
  bottom=hy-9 if hy>ny+13 else ny+30
  polygon=[(nx-12,ny+2),(nx+10,ny+2),(nx+19,bottom),(nx-7,bottom)]
 # Side-facing folded arms have a particularly visible triangular shirt bridge.
 if i in list(range(8))+[28,31,46,47,52,55]:
  polygon=[(nx-11,ny+1),(nx+8,ny+1),(nx+20,hy-7),(nx+1,hy-6),(nx-8,ny+13)]
 if i==45:
  polygon=[(62,79),(79,81),(94,103),(65,111),(53,94)]
 if i==54:
  polygon=[(87,88),(101,87),(113,104),(108,113),(94,108)]
 cut=Image.new("L",(1024,1024))
 ImageDraw.Draw(cut).polygon([(round(px*F*4),round(py*F*4)) for px,py in polygon],fill=255)
 cut=cut.resize((256,256),Image.Resampling.LANCZOS)
 ba=np.array(b)
 arm_keep=Image.new("L",(1024,1024))
 draw=ImageDraw.Draw(arm_keep)
 for start,end in arm_paths.get(i,[]):
  points=[(round(px*F*4),round(py*F*4)) for px,py in (start,end)]
  width=round(18*F*4)
  draw.line(points,fill=255,width=width)
  for px,py in points:draw.ellipse((px-width/2,py-width/2,px+width/2,py+width/2),fill=255)
 arm_keep=arm_keep.resize((256,256),Image.Resampling.LANCZOS)
 # Geometric sleeve/forearm paths, not skin-color thresholds, avoid stippled holes.
 exclude=np.array(cut,dtype=float)/255*(1-np.array(arm_keep,dtype=float)/255)
 ma=np.array(mask,dtype=float)/255*(1-exclude)
 # The forward shoulder needs a small explicit extension beyond the generated
 # guide. Do not extend opposite-side capsules into the visible back torso.
 if i in back:
  shoulder=Image.new("L",(1024,1024));sd=ImageDraw.Draw(shoulder)
  start,end=arm_paths[i][-1]
  points=[(round(px*F*4),round(py*F*4)) for px,py in (start,end)]
  width=round(12*F*4)
  sd.line(points,fill=255,width=width)
  for px,py in points:sd.ellipse((px-width/2,py-width/2,px+width/2,py+width/2),fill=255)
  shoulder=shoulder.resize((256,256),Image.Resampling.LANCZOS)
  ma=np.maximum(ma,np.array(shoulder,dtype=float)/255)
 ba[:,:,3]=np.uint8(ba[:,:,3]*ma)
 foreground.paste(Image.fromarray(ba),(x,y))
 masks.append(ma)
foreground.save(ART/"foreground.png")

for name in ("armor","warband_armor"):
 source=Image.open(SOURCE/(name+".png")).convert("RGBA")
 result=Image.new("RGBA",body.size)
 pivots,scales,_=ctx["armor_registration"](name)
 for i in range(56):
  x,y=i%8*256,i//8*256
  cell=source.crop((x,y,x+256,y+256))
  px,py=pivots[i];sx,sy=scales[i]
  # Hurt recoil leans back: its visible chest lies right of the neck anchor.
  if i==45:
   px-=30/sx;py+=10/sy
  fitted=cell.transform((256,256),Image.Transform.AFFINE,
    (1/sx,0,px-128/sx,0,1/sy,py-220/sy),Image.Resampling.BICUBIC)
  pixels=np.array(fitted)
  if i in [45,48,54]:
   fitted.save(ART/(name+"-debug"+str(i)+"-unmasked.png"))
  silhouette=body.crop((x,y,x+256,y+256)).getchannel("A").filter(ImageFilter.MaxFilter(5))
  coverage=np.array(silhouette,dtype=float)/255
  pixels[:,:,3]=np.uint8(pixels[:,:,3]*coverage*(1-masks[i]))
  result.paste(Image.fromarray(pixels),(x,y))
 result.save(ART/(name+".png"))
 # Armor is now already registered in canonical body space.
 ctx["write_layer"](name,"armor",1)

# Exact body-derived head/arms are a single registered occlusion sheet. No
# duplicated head rectangle or cropped forearm bands are needed.
foreground_text=f'''[gd_resource type="Resource" script_class="ModularSpriteLayer" load_steps=3 format=3]
[ext_resource type="Script" path="res://game/presentation/modular_sprite_layer.gd" id="1"]
[ext_resource type="Texture2D" path="res://assets/art/modular/one_handed/foreground.png" id="2"]
[resource]
script = ExtResource("1")
label = "Canonical body occlusion"
family = &"one_handed"
rig_id = &"human_one_handed_v1"
slot = &"foreground"
sheet = ExtResource("2")
depth = 3
'''
(OUT/"foreground.tres").write_text(foreground_text)
for name in ("club","warband"):
 path=OUT/(name+".tres")
 text=path.read_text().replace("load_steps=8","load_steps=7")
 text=text.replace('[ext_resource type="Resource" path="res://game/content/modular/one_handed/head_foreground.tres" id="7"]\n',"")
 text=text.replace(', ExtResource("7")',"")
 text=text.replace(f'{name} (candidate — armor needs cleanup)',"Club bandit" if name=="club" else "Sword warband")
 path.write_text(text)
for path in OUT.glob("*.tres"):
 text=path.read_text().replace(" candidate","")
 path.write_text(text)
print("Finished exact-body masks and registered armor.")
