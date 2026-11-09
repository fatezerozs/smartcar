"""Offline response review with identity-aligned held PI states; no device access."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path

from tools.speed_planning.export_speed_response import CONFIG_FIELDS, VERSION, validate_trace


def age(later, earlier):
    return (later-earlier)&0xffffffff


def negative_runs(rows, raw_key, sign, stop):
    found, current = [], []
    for row in rows[1:]:
        negative = row['phase']==2 and age(stop,row['ms'])<0x80000000 and row[raw_key]*sign<0
        if negative:
            current.append(row)
        elif current:
            found.append(current);current=[]
    if current:
        found.append(current)
    return [{'firstSequence': run[0]['seq'], 'lastSequence': run[-1]['seq'],
             'samples': len(run), 'elapsedSampleSpanMs': age(run[-1]['ms'],run[0]['ms']),
             'minimumNormalizedCounts10ms': min(r[raw_key]*sign for r in run)} for run in found]


def analyze(path):
    path=Path(path)
    trace=json.loads(path.read_text(encoding='utf-8'))
    if trace.get('version')!=VERSION or trace.get('complete') is not True:
        raise ValueError('Complete supported trace required')
    rows,config,info=trace['rows'],trace['config'],trace['info']
    validate_trace(info,rows,[config[k] for k in CONFIG_FIELDS])
    if config['closedLoop']!=1:
        raise ValueError('Closed-loop trace required')
    signs={'l':-1 if config['signLeftRawU32']==0xffffffff else 1,
           'r':-1 if config['signRightRawU32']==0xffffffff else 1}
    by_seq={row['seq']:row for row in rows[1:]}  # Seed0 is never a measured sample.
    matched,anomalies,seen=[],[],set()
    stop,start=info['stop_ms'],info['start_ms']
    for row in rows:
        if row['phase']!=2 or row['enabled']!=1:
            continue
        identity=(row['control_seq'],row['control_ms'])
        if identity in seen:
            continue
        seen.add(identity)
        sample=by_seq.get(row['control_seq'])
        if sample is None:
            continue  # Explicit reset or state inherited before this trace.
        if age(row['ms'],row['control_ms'])>=0x80000000 or age(row['control_ms'],sample['ms'])>=0x80000000:
            raise ValueError('Impossible control/sample time pairing')
        if (row['measured_l'],row['measured_r']) != (sample['rl']*signs['l'],sample['rr']*signs['r']):
            detail={'heldAtSequence':row['seq'],'controlSequence':row['control_seq'],
                    'controlElapsedMs':age(row['control_ms'],start),'targetLeftRight':[row['target_l'],row['target_r']],
                    'heldFeedbackLeftRight':[row['measured_l'],row['measured_r']],
                    'rawFeedbackLeftRight':[sample['rl']*signs['l'],sample['rr']*signs['r']],
                    'reason':'held feedback does not match source sample'}
            if row['target_l'] or row['target_r']:
                raise ValueError('Nonzero-target feedback/source mismatch')
            anomalies.append(detail)
            continue
        if row['target_l']<=0 or row['target_r']<=0:
            continue
        matched.append(row)
    target=config['maximumTargetTicks10ms']
    full=[row for row in matched if row['target_r']==target and row['target_l']==target]
    windows={}
    for window in (300,500):
        group=[row for row in full if age(stop,row['control_ms'])<=window]
        windows[str(window)]={'uniqueControlSamples':len(group), 'notSteadyStateCertification':True}
        for wheel in ('l','r'):
            vals=[r['measured_'+wheel] for r in group]
            windows[str(window)][wheel]={'meanCounts10ms':sum(vals)/len(vals) if vals else None,
                'rangeCounts10ms':[min(vals),max(vals)] if vals else None,
                'rmseCounts10ms':math.sqrt(sum((v-target)**2 for v in vals)/len(vals)) if vals else None,
                'samplesWithin2Counts':sum(abs(v-target)<=2 for v in vals),
                'maxOvershootCounts':max([0,*[v-target for v in vals]])}
    movement={}
    for wheel,key in [('l','rl'),('r','rr')]:
        measured=[row for row in rows[1:] if row['phase']==2 and row[key]*signs[wheel]>=3]
        movement[wheel]={'thresholdCounts10ms':3,'firstSampleElapsedMs':age(measured[0]['ms'],start) if measured else None,
                         'interpretation':'Thresholded sampled encoder response, not exact physical start or reliable deadband.'}
    saturated={}
    ceiling=min(config['requestPermille'],config['profileCapPermille'],600)*10
    for wheel,key in [('l','pl'),('r','pr')]:
        active=[row for row in rows[1:] if row['phase']==2 and age(stop,row['ms'])<0x80000000]
        saturated[wheel]={'sampledAtCeiling':sum(row[key]==ceiling for row in active),
                          'sampledCeilingDurationMs':sum(row['interval'] for row in active if row[key]==ceiling),
                          'softwarePwmPeakPermille':max(row[key] for row in rows)/10}
    result={'schemaVersion':'smartcar.speed-response-review/1','sourceSha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'version':VERSION,'crcVerified':True,'rows':len(rows),'config':config,
            'firstZeroAfterArmMs':age(stop,start),'postZeroMs':age(rows[-1]['ms'],stop),
            'stopReason':info['reason'],'faultDetailValues':sorted(set(row['detail'] for row in rows)),
            'faultValues':sorted(set(row['faults'] for row in rows)),
            'uniquePositiveTargetMatchedControls':len(matched),'excludedZeroTargetPairingAnomalies':anomalies,
            'tailWindowsMs':windows,'responseOnset':movement,'outputCeiling':saturated,
            'driveNegativeRuns':{wheel:negative_runs(rows,key,signs[wheel],stop) for wheel,key in [('l','rl'),('r','rr')]},
            'signedRawTotalDeltaLeftRight':[rows[-1]['tl']-rows[0]['tl'],rows[-1]['tr']-rows[0]['tr']],
            'productionApproved':False,'hardwareAccessed':False,
            'limits':['Measured elapsed counts and held PI state are matched by sample identity; seed0 is excluded.',
                      'PWM columns are held software submissions, not guaranteed to belong to the PI calculation in the same row.',
                      'Saturation duration is sample-window attribution, not electrical timing.',
                      'Short lifted tests and unmeasured battery voltage cannot establish stationary or ground performance.',
                      'Forward/reverse sampled counts do not independently prove visible rotation or encoder correctness.']}
    return result,matched


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('traces',type=Path,nargs='+');parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False);reports=[]
    for number,path in enumerate(args.traces):
        result,matched=analyze(path);result['sourceTrace']=str(path);reports.append(result)
        with (args.output/f'paired-controls-{number+1}.csv').open('w',encoding='utf-8',newline='') as file:
            keys=['seq','ms','control_seq','control_ms','target_l','target_r','measured_l','measured_r','integral_l','integral_r','pl','pr']
            writer=csv.DictWriter(file,keys,extrasaction='ignore');writer.writeheader();writer.writerows(matched)
    (args.output/'comparison.json').write_text(json.dumps({'reports':reports,'productionApproved':False},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps([{'rows':r['rows'],'matched':r['uniquePositiveTargetMatchedControls'],
                      'excludedZeroTarget':len(r['excludedZeroTargetPairingAnomalies']),
                      'faultDetails':r['faultDetailValues'],'tail300ms':r['tailWindowsMs']['300']} for r in reports]))


if __name__=='__main__':main()
