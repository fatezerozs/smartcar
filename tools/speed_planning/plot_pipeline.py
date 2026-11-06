"""Optional exploratory charts of synthetic composed replay; not vehicle data."""
import argparse
import csv
from pathlib import Path


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--csv',type=Path,required=True);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    rows=list(csv.DictReader(args.csv.open(encoding='utf-8')));args.output.mkdir(parents=True,exist_ok=True)
    plt.rcParams.update({'font.size':10,'axes.grid':True,'grid.alpha':.2,'axes.spines.top':False})
    names=['Straight','Preview bend','Brief far loss','Far time budget','Far distance budget','Near recovery to far loss']
    fig,axes=plt.subplots(2,3,figsize=(13,7),layout='constrained')
    for case,ax in enumerate(axes.flat):
        data=[r for r in rows if int(r['case'])==case];time=[float(r['timeMs'])/1000 for r in data]
        ax.plot(time,[float(r['speedRefMps']) for r in data],label='Reference',color='#17648c',lw=1.5)
        ax.plot(time,[float(r['measuredMps']) for r in data],label='Synthetic measured',color='#b9642e',lw=.8,alpha=.8)
        ax.set(title=names[case],xlabel='Time (s)',ylabel='Speed (m/s)',ylim=(-.015,.56))
    axes[0,0].legend(loc='lower right',fontsize=8)
    fig.suptitle('Reference v0.2: C planner + actual PI code + fictitious wheel plant; no vehicle measurements')
    fig.savefig(args.output/'composed-motion.png',dpi=150);plt.close(fig)
    fig,axes=plt.subplots(2,3,figsize=(13,7),layout='constrained')
    labels={3:'Far time budget',4:'Far distance budget',5:'Near recovers, far absent',6:'Near timeout',7:'Camera freeze',9:'STOP during candidate'}
    for case,ax in zip(labels,axes.flat):
        data=[r for r in rows if int(r['case'])==case];time=[float(r['timeMs'])/1000 for r in data]
        ax.step(time,[int(r['mode']) for r in data],where='post',color='#17648c',lw=1.5,label='Mode')
        ax.set(title=labels[case],xlabel='Time (s)',ylabel='Mode',ylim=(0,6.5),yticks=range(7))
        right=ax.twinx();right.plot(time,[int(r['leftPwmPermille']) for r in data],color='#b9642e',lw=.8,label='Mock left PWM')
        right.set(ylabel='Mock PWM (permille)',ylim=(-5,220));right.grid(False)
    fig.suptitle('Synthetic safety traces: 1 normal / 2 far-loss / 3 recovery / 4 controlled-stop / 5 stop / 6 fault')
    fig.savefig(args.output/'composed-safety.png',dpi=150);plt.close(fig)
    print(args.output)


if __name__=='__main__':main()
