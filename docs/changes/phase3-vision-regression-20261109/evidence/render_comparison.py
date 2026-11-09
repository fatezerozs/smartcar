from pathlib import Path
import csv
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
base=Path(__file__).resolve().parent
raw=(base/"final-metric-audit/original-images.gray").read_bytes()
before=list(csv.DictReader((base/"baseline/rows.csv").open()))
after=list(csv.DictReader((base/"verified-final/rows.csv").open()))
fig,axs=plt.subplots(3,3,figsize=(12,7.5),layout="constrained")
for j,frame in enumerate([0,256,650]):
    im=np.frombuffer(raw[frame*22560:(frame+1)*22560],np.uint8).reshape(120,188)
    for i,(label,rows) in enumerate([("Original",None),("Before",before),("After",after)]):
        ax=axs[j,i];ax.imshow(im,cmap="gray",vmin=0,vmax=255)
        ax.set_title(f"{label} / original frame {frame}");ax.set_xlim(0,187);ax.set_ylim(110,35)
        if rows:
            selected=[rows[frame*120+y] for y in range(35,111) if rows[frame*120+y]["flags"]=="3"]
            yy=[int(r["row"]) for r in selected]
            for key,color in [("left","#00e5ff"),("right","#00e5ff"),("center","#ff6758")]:
                ax.scatter([int(r[key]) for r in selected],yy,s=3,c=color)
        ax.set_xlabel("raw pixel x");ax.set_ylabel("raw row y")
fig.savefig(base/"verified-final/comparison.png",dpi=140)
plt.close(fig)
