"""Measure generated gear and author registration metadata; never edits artwork."""
from pathlib import Path
import numpy as np
from PIL import Image
import math
ROOT=Path(__file__).resolve().parents[4]
ART=Path(__file__).parent
OUT=ROOT/"game/content/modular/one_handed"
F=2048/1341
hands=[
 (109,98),(109,98),(111,98),(110,98),(110,98),(110,98),(111,98),(110,98),
 (114,103),(114,103),(114,104),(114,104),(49,87),(49,87),(49,87),(49,87),
 (109,105),(111,103),(118,103),(113,108),(108,119),(108,119),(108,119),(108,119),
 (123,90),(51,79),(136,88),(135,85),(109,94),(71,44),(141,82),(110,97),
 (88,104),(66,42),(99,114),(89,103),(127,85),(72,49),(146,64),(125,75),
 (120,96),(93,48),(142,61),(123,69),(84,116),(58,106),(120,102),(111,103),
 (130,130),(130,130),(145,130),(145,130),(112,94),(71,44),(112,119),(111,99)]
angles=[-45]*8+[-55]*4+[-125]*4+[-45,-35,0,-45]+[-50]*4+[0,-130,-20,-30]+[-40,-35,-8,-40]+[0,-60,42,-25]+[-35,-150,-25,-45]+[-20,-160,-25,-65]+[45,10,0,-20]+[0]*4+[-40,-35,40,-40]
images={}
component_regions={}
head_necks=[]
def component_region(name,i):
 if name not in component_regions:
  a=np.asarray(Image.open(ART/(name+".png")).convert("RGBA"))
  mask=a[:,:,3]>150
  seen=np.zeros(mask.shape,dtype=bool); comps=[]
  for yy,xx in zip(*np.where(mask)):
   if seen[yy,xx]:continue
   stack=[(xx,yy)];seen[yy,xx]=True;pts=[]
   while stack:
    x,y=stack.pop();pts.append((x,y))
    for nx,ny in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
     if 0<=nx<2048 and 0<=ny<1792 and mask[ny,nx] and not seen[ny,nx]:
      seen[ny,nx]=True;stack.append((nx,ny))
   if len(pts)>100:
    p=np.array(pts);comps.append((p.mean(axis=0),p.min(axis=0),p.max(axis=0)))
  component_regions[name]=[]
  for j in range(56):
   center=np.array([j%8*256+160,j//8*256+140])
   c,lo,hi=min(comps,key=lambda c:np.linalg.norm(c[0]-center))
   lo=np.maximum(lo-2,[0,0]);hi=np.minimum(hi+3,[2048,1792])
   component_regions[name].append((*lo,*(hi-lo)))
 return component_regions[name][i]
def points(name,i):
 if name not in images:images[name]=np.asarray(Image.open(ART/(name+".png")).convert("RGBA"))
 a=images[name]
 a=a[i//8*256:(i//8+1)*256,i%8*256:(i%8+1)*256]
 yy,xx=np.where(a[:,:,3]>150)
 return a,np.column_stack([xx,yy]).astype(float)
def weapon_registration(name):
 pivots=[]; scales=[]; rots=[]
 for i in range(56):
  points(name,i)
  left,top,width,height=component_region(name,i)
  a=images[name][top:top+height,left:left+width]
  offset=np.array([left-i%8*256,top-i//8*256])
  yy,xx=np.where(a[:,:,3]>150)
  p=np.column_stack([xx,yy]).astype(float)+offset
  if name=="weapon":
   _,_,vh=np.linalg.svd(p-p.mean(axis=0),full_matrices=False)
   v=vh[0]
   projection=p@v
   low,high=np.quantile(projection,[.02,.98])
   rgb=a[:,:,:3].astype(float)
   metal=(a[:,:,3]>150)&(rgb[:,:,1]>rgb[:,:,0]*.8)&(rgb[:,:,2]>rgb[:,:,0]*.65)&(rgb[:,:,0]>35)
   yy,xx=np.where(metal)
   cap=np.array([xx.mean(),yy.mean()])+offset
   if (cap-p.mean(axis=0))@v<0:
    v=-v;projection=p@v;low,high=np.quantile(projection,[.02,.98])
   handle=p[(projection<low+(high-low)*.13)].mean(axis=0)
   angle=math.atan2(v[1],v[0])
   scale=.65
  else:
   rgb=a[:,:,:3].astype(float)
   mask=(a[:,:,3]>150)&(rgb[:,:,0]>rgb[:,:,1]*1.18)&(rgb[:,:,0]>rgb[:,:,2]*1.3)&(rgb[:,:,0]>35)
   yy,xx=np.where(mask)
   handle=np.array([xx.mean(),yy.mean()])+offset
   bright=(a[:,:,3]>150)&(rgb.mean(axis=2)>115)&(abs(rgb[:,:,0]-rgb[:,:,2])<35)
   yy,xx=np.where(bright)
   blade=np.array([xx.mean(),yy.mean()])+offset
   v=blade-handle
   angle=math.atan2(v[1],v[0])
   scale=.52
  rot=math.radians(angles[i])-angle
  c,s=math.cos(-rot),math.sin(-rot)
  target=np.array(hands[i])*F-np.array([128,220])
  inverse=np.array([[c,-s],[s,c]])@target/scale
  pivots.append(handle-inverse);scales.append((scale,scale));rots.append(rot)
 return pivots,scales,rots
def armor_registration(name):
 pivots=[];scales=[]
 # Neck anchor position and vest size are tied to body, never centered by alpha.
 necks=[(92,74)]*8+[(91,246-167.625)]*4+[(84,247-167.625)]*4
 necks += [(92,420-335.25),(92,418-335.25),(101,418-335.25),(96,422-335.25)]+[(92,423-335.25)]*4
 necks += [(87,581-502.875),(87,581-502.875),(95,583-502.875),(94,582-502.875),(92,575-502.875),(95,576-502.875),(104,573-502.875),(96,577-502.875)]
 necks += [(88,752-670.5),(93,751-670.5),(95,758-670.5),(90,752-670.5),(86,747-670.5),(87,747-670.5),(102,741-670.5),(94,743-670.5)]
 necks += [(86,919-838.125),(85,918-838.125),(97,911-838.125),(88,914-838.125),(93,930-838.125),(65,922-838.125),(103,927-838.125),(100,927-838.125)]
 necks += [(94,1117-1005.75),(94,1117-1005.75),(110,1121-1005.75),(110,1121-1005.75),(94,1083-1005.75),(96,1083-1005.75),(101,85),(97,1087-1005.75)]
 head_necks[:]=necks
 for i in range(56):
  a,p=points(name,i)
  lo=p.min(axis=0);hi=p.max(axis=0)
  # Front/back collars lie near top-center; side collar shifts rightward.
  front=i in list(range(8,16))+list(range(20,28))+[32,33,35]
  fraction=.5 if front else .67
  if name=="warband_armor" and not front:fraction=.58
  if i in [48,49,50,51]: fraction=.81
  collar=lo+(hi-lo)*np.array([fraction,.15])
  scale=.60 if name=="armor" else .55
  if i in [48,49,50,51]:scale=.48
  neck=np.array(necks[i])+np.array([-4,1] if i not in [48,49,50,51] else [0,0])
  target=neck*F-np.array([128,220])
  pivots.append(collar-target/scale);scales.append((scale,scale))
 return pivots,scales,[0]*56
def write_layer(name,slot,depth,registration=None):
 sheet_name="body" if name=="head_foreground" else name
 text=f'''[gd_resource type="Resource" script_class="ModularSpriteLayer" load_steps=3 format=3]

[ext_resource type="Script" path="res://game/presentation/modular_sprite_layer.gd" id="1"]
[ext_resource type="Texture2D" path="res://assets/art/modular/one_handed/{sheet_name}.png" id="2"]

[resource]
script = ExtResource("1")
label = "{name} candidate"
family = &"one_handed"
rig_id = &"human_one_handed_v1"
slot = &"{slot}"
sheet = ExtResource("2")
depth = {depth}
'''
 if name=="foreground":
  regions=[];pivots=[]
  for i,(hx,hy) in enumerate(hands):
   top=max(0,round((hy-8)*F));bottom=min(256,round((hy+12)*F))
   regions.append((i%8*256,i//8*256+top,256,bottom-top))
   pivots.append((128,220-top))
  text+="regions = Array[Rect2](["+", ".join(f"Rect2({x}, {y}, {w}, {h})" for x,y,w,h in regions)+"])\n"
  text+="pivots = Array[Vector2](["+", ".join(f"Vector2({x}, {y})" for x,y in pivots)+"])\n"
 if name=="head_foreground":
  regions=[];pivots=[]
  for i,(nx,ny) in enumerate(head_necks):
   left=max(0,round((nx-17)*F));right=min(256,round((nx+22)*F))
   top=max(0,round((ny-28)*F));bottom=min(256,round((ny+1)*F))
   regions.append((i%8*256+left,i//8*256+top,right-left,bottom-top))
   pivots.append((128-left,220-top))
  text+="regions = Array[Rect2](["+", ".join(f"Rect2({x}, {y}, {w}, {h})" for x,y,w,h in regions)+"])\n"
  text+="pivots = Array[Vector2](["+", ".join(f"Vector2({x}, {y})" for x,y in pivots)+"])\n"
 if registration:
  p,s,r=registration
  if slot=="weapon":
   regions=[]
   for i in range(56):
    x,y=(i%8)*256,(i//8)*256
    left,top,width,height=component_region(name,i)
    regions.append((left,top,width,height))
    p[i]+=np.array([x-left,y-top])
   text+="regions = Array[Rect2](["+", ".join(f"Rect2({x}, {y}, {w}, {h})" for x,y,w,h in regions)+"])\n"
  text+="pivots = Array[Vector2](["+", ".join(f"Vector2({x:.4f}, {y:.4f})" for x,y in p)+"])\n"
  text+="frame_scales = PackedVector2Array("+", ".join(f"{x:.4f}, {y:.4f}" for x,y in s)+")\n"
  text+="frame_rotations = PackedFloat32Array("+", ".join(f"{v:.6f}" for v in r)+")\n"
 (OUT/(name+".tres")).write_text(text)
write_layer("body","body",0)
write_layer("foreground","foreground",3)
for name in ["weapon","sword"]:write_layer(name,"weapon",2,weapon_registration(name))
for name in ["armor","warband_armor"]:write_layer(name,"armor",1,armor_registration(name))
write_layer("head_foreground","head_foreground",4)
for name,armor,weapon in [("club","armor","weapon"),("warband","warband_armor","sword")]:
 (OUT/(name+".tres")).write_text(f'''[gd_resource type="Resource" script_class="ModularSpriteRig" load_steps=8 format=3]
[ext_resource type="Script" path="res://game/presentation/modular_sprite_rig.gd" id="1"]
[ext_resource type="Script" path="res://game/presentation/modular_sprite_layer.gd" id="2"]
[ext_resource type="Resource" path="res://game/content/modular/one_handed/body.tres" id="3"]
[ext_resource type="Resource" path="res://game/content/modular/one_handed/{armor}.tres" id="4"]
[ext_resource type="Resource" path="res://game/content/modular/one_handed/{weapon}.tres" id="5"]
[ext_resource type="Resource" path="res://game/content/modular/one_handed/foreground.tres" id="6"]
[ext_resource type="Resource" path="res://game/content/modular/one_handed/head_foreground.tres" id="7"]
[resource]
script = ExtResource("1")
label = "{name} (candidate — armor needs cleanup)"
family = &"one_handed"
rig_id = &"human_one_handed_v1"
body = ExtResource("3")
equipment = Array[ExtResource("2")]([ExtResource("4"), ExtResource("5"), ExtResource("6"), ExtResource("7")])
''')
print("Wrote 7 layers and 2 candidate rigs; armor visual registration is not approved.")
