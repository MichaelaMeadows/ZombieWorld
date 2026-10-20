"""User-authorized deterministic cleanup; no generated/reconstructed body pixels.
Weapon masks erase only shoulder quiver-stub boxes measured on the accepted atlas.
Foreground samples original body RGBA at exactly the original coordinates.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter
import numpy as np
from collections import deque

root=Path(__file__).parent
backup=root/"weapon-before-mask.png"
if not backup.exists():
    Image.open(root/"weapon.png").save(backup)
weapon=Image.open(backup).convert("RGBA")
mask=Image.new("L",weapon.size,0)
draw=ImageDraw.Draw(mask)
boxes={}
for i in range(8,12): boxes[i]=(98,78,143,129)
for i in range(12,16): boxes[i]=(138,75,189,125)
for i in range(16,20): boxes[i]=(98,90,139,124)
for i in range(20,24): boxes[i]=(96,79,145,134)
for i in range(24,28): boxes[i]=(131,73,184,128)
for i,box in boxes.items():
    x,y=i%8*256,i//8*256
    draw.rectangle((x+box[0],y+box[1],x+box[2],y+box[3]),fill=255)
# Cell 52 has a tiny detached spill from the previous death pose.
draw.rectangle((0+4*256,6*256+164,4*256+7,6*256+180),fill=255)
w=np.array(weapon);w[np.array(mask)>0]=0
Image.fromarray(w).save(root/"weapon.png")

body=np.array(Image.open(root/"body.png").convert("RGBA"))
r,g,b=[body[:,:,j].astype(float) for j in range(3)]
skin=(r>70)&(b>20)&(r-g>20)&(r>g*1.2)&(g>b*1.15)&(body[:,:,3]>0)
# Reject isolated brown antialias/shading specks on the gray tunic.
pending=skin.copy()
clean=np.zeros_like(skin)
for y,x in np.argwhere(skin):
    if not pending[y,x]: continue
    q=deque([(int(y),int(x))]);pending[y,x]=False;points=[]
    while q:
        yy,xx=q.popleft();points.append((yy,xx))
        for ny,nx in [(yy-1,xx),(yy+1,xx),(yy,xx-1),(yy,xx+1)]:
            if 0<=ny<1792 and 0<=nx<2048 and pending[ny,nx]:
                pending[ny,nx]=False;q.append((ny,nx))
    if len(points)>=80:
        p=np.array(points);clean[p[:,0],p[:,1]]=True
# Close small holes in skin shading, then include its one-pixel boundary.
skin=np.array(Image.fromarray((clean*255).astype("uint8")).filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.MinFilter(5)).filter(ImageFilter.MaxFilter(3)))>0
# Exact skin/hair samples restore faces, hands and bare forearms over the vest.
# No filled-in shapes, recoloring, alpha inflation or pose movement.
foreground=body.copy();foreground[~skin]=0
Image.fromarray(foreground).save(root/"foreground.png")
print("Weapon mask removed",int(((np.array(mask)>0)&(np.array(weapon)[:,:,3]>0)).sum()),"pixels")
print("Foreground exact body samples:",int(skin.sum()))
