from PIL import Image
import numpy as np
from collections import deque
from pathlib import Path
root=Path(__file__).parent
a=np.array(Image.open(root/"weapon.png").convert("RGBA"))
for i in range(56):
    cell=a[i//8*256:(i//8+1)*256,i%8*256:(i%8+1)*256]
    mask=cell[:,:,3]>32
    found=[]
    while mask.any():
        y,x=np.argwhere(mask)[0]
        q=deque([(int(y),int(x))]);mask[y,x]=False;p=[]
        while q:
            y,x=q.popleft();p.append((y,x))
            for yy,xx in [(y-1,x),(y+1,x),(y,x-1),(y,x+1)]:
                if 0<=yy<256 and 0<=xx<256 and mask[yy,xx]:
                    mask[yy,xx]=False;q.append((yy,xx))
        if len(p)>12:
            p=np.array(p);found.append((len(p),p.min(0)[::-1].tolist(),p.max(0)[::-1].tolist()))
    print(i,sorted(found,reverse=True))
