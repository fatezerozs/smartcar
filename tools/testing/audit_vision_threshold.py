"""Explain selected clear-scene row thresholds with an exact rational oracle.

Diagnostic of the current 18-gray / 12-pixel / 3:4 refinement guards. This is
not a replacement detector or a demand that future implementations retain the
same decisions. Compare predictions with saved native row outputs explicitly.
"""
import argparse
import csv
from fractions import Fraction
import json
from pathlib import Path

def otsu(values,start=0):
    histogram=[values.count(i) for i in range(256)]
    count=sum(histogram[start:]);total=sum(i*histogram[i] for i in range(start,256))
    back=back_sum=0;best=None;ties=[]
    for t in range(start,255):
        back+=histogram[t];back_sum+=t*histogram[t]
        if not back or back==count:continue
        score=Fraction((total*back-back_sum*count)**2,back*(count-back))
        if best is None or score>best:best=score;ties=[t]
        elif score==best:ties.append(t)
    return (ties[0]+ties[-1])//2 if ties else 0

def inspect(values):
    first=otsu(values);stages=[]
    for _ in range(2):
        bright=[v for v in values if v>first];second=otsu(values,first+1)
        middle=[v for v in values if first<v<=second];white=[v for v in values if v>second]
        left=right=93
        while left>0 and values[left-1]>first:left-=1
        while right<187 and values[right+1]>first:right+=1
        middle_mean=Fraction(sum(middle),len(middle)) if middle else 0
        white_mean=Fraction(sum(white),len(white)) if white else 0
        checks={'brightSupport':len(bright)>=24,'brightRange':bool(bright and max(bright)-min(bright)>=18),
            'bothClassesSupported':len(middle)>=12 and len(white)>=12,
            'classSeparation':bool(middle and white and white_mean-middle_mean>=18),
            'middleAtLeastThreeQuartersWhite':bool(middle and white and 4*middle_mean>=3*white_mean),
            'axisAboveFirst':values[93]>first,'connectedMidpointAboveSecond':values[(left+right)//2]>second}
        stages.append({'first':first,'second':second,'middleMean':float(middle_mean),'whiteMean':float(white_mean),'checks':checks})
        if not all(checks.values()):break
        first=second
    return {'predictedFinalThreshold':first,'stages':stages}

def run(folder):
    folder=Path(folder);raw=(folder/'original.gray').read_bytes()
    with (folder/'rows.csv').open(newline='') as stream:rows=list(csv.DictReader(stream))
    results=[]
    for frame in (0,32,80,99):
        y=54;values=list(raw[frame*22560+y*188:frame*22560+(y+1)*188])
        result=dict(frame=frame,row=y,**inspect(values))
        result['nativeThreshold']=int(rows[frame*120+y]['threshold'])
        result['matchesNativeThreshold']=result['nativeThreshold']==result['predictedFinalThreshold']
        results.append(result)
    (folder/'threshold-reason-check.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(results,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--artifacts',type=Path,required=True)
    run(parser.parse_args().artifacts)
