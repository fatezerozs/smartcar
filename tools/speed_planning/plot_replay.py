"""Optional exploratory plots. matplotlib is not required by the test suite."""
import argparse
import csv
from pathlib import Path


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--csv',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    rows=list(csv.DictReader(args.csv.open(encoding='utf-8')))
    args.output.mkdir(parents=True,exist_ok=True)
    plt.rcParams.update({'font.size':10,'axes.grid':True,'grid.alpha':.2,'axes.spines.top':False,'axes.spines.right':False})
    fig,axes=plt.subplots(2,3,figsize=(12,6),layout='constrained')
    labels=['Straight','Gentle constant curve','Sharp constant curve','S curve','Straight into sharp curve','Gentle curve + noise']
    for case,ax in enumerate(axes.flat):
        data=[r for r in rows if r['track']=='spatial' and int(r['case'])==case]
        ax.plot([float(r['coordinate']) for r in data],[float(r['speed']) for r in data],color='#17648c',marker='.',lw=1.5)
        ax.set(title=labels[case],xlabel='Path distance (m)',ylabel='Speed cap (m/s)',ylim=(0,3.2))
    fig.suptitle('Synthetic SI model: spatial caps, finite visible horizon; no vehicle measurements')
    fig.savefig(args.output/'spatial-preview.png',dpi=160);plt.close(fig)
    data=[r for r in rows if r['track']=='temporal'];t=[float(r['coordinate']) for r in data]
    speed=[float(r['speed']) for r in data];a=[float(r['feature']) for r in data]
    jerk=[(y-x)/.02 for x,y in zip([0,*a],a)]
    target=[2 if n<150 else 1 if n<320 else 2.5 for n in range(len(data))]
    fig,axes=plt.subplots(3,1,figsize=(11,7),sharex=True,layout='constrained')
    axes[0].plot(t,target,'--',color='#a6a6a6',label='Requested speed')
    for ax,y,label in zip(axes,[speed,a,jerk],['Speed (m/s)','Acceleration (m/s²)','Jerk (m/s³)']):
        ax.plot(t,y,color='#17648c',lw=1.2,label='Reference');ax.set_ylabel(label)
    axes[0].legend(loc='lower right');axes[-1].set_xlabel('Time (s); fixed dt = 0.02 s')
    axes[1].axhline(1,color='#a6a6a6',ls='--');axes[1].axhline(-2,color='#a6a6a6',ls='--')
    axes[2].axhline(4,color='#a6a6a6',ls='--');axes[2].axhline(-6,color='#a6a6a6',ls='--')
    fig.suptitle('Synthetic trajectory reference; no measured wheel speed or physical stopping evidence')
    fig.savefig(args.output/'trajectory-reference.png',dpi=160);plt.close(fig)
    print(args.output)


if __name__=='__main__':main()
