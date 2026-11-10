"""Measure current native vision on hash-pinned real images; audit near/full loss.

Accuracy is limited to the existing 28 visually reviewed boundary bands. Coverage
on all 750 frames is reported separately. Host timing is not target WCET.
"""
from __future__ import annotations
import argparse
from collections import Counter
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
sys.path.insert(0,str(ROOT/'tools/testing'))
from benchmark import compare,statistic,validate_benchmark
from common import inside,read_json,sha256,write_json
from tools.speed_planning.phase3_profile import PROFILE,header
from tools.speed_planning.replay import compile_command
from tools.speed_planning.visual_trace import decode_frame

MANIFESTS=['tests/benchmarks/manifests/vision-images-timing.json',
           'tests/benchmarks/manifests/vision-images-quality.json']
SOURCES=['demo/code/vision_midterm.c','demo/code/vision_metric_io.c','demo/code/vision_planner.c',
         'demo/code/vision_planner_shadow.c','tools/speed_planning/vision_adapter.c','tools/speed_planning/speed_planner.c']
FLAGS=['VISION_MIDTERM_TRAINING=1','VISION_PLANNER_RUNTIME=1','VISION_ROUTE_SWITCH=1',
       'VISION_ENCODER_ENABLE=1','VISION_MIDTERM_ROUTE_TRIAL=1','VISION_MIDTERM_AUTONOMOUS=0']
REASONS={0:'accepted',1:'source_invalid_or_quality',2:'requested_row_has_no_supported_path',
         3:'adapter_geometry_or_history',4:'outside_configured_ground_domain'}
SUBREASONS={0:'accepted',1:'source_invalid_or_quality',2:'requested_row_has_no_supported_path',
            3:'temporal_history_rejection_confirmed_by_native_cold_retry',4:'non_forward_ground_projection',
            5:'other_geometry_or_row_contract',6:'outside_configured_ground_domain'}

def csv_rows(path):
    with Path(path).open(encoding='utf-8',newline='') as stream:return list(csv.DictReader(stream))

def complete_grid(rows,count,repeats):
    expected=[(r,i) for r in range(repeats) for i in range(count)]
    if [(int(r['repetition']),int(r['index'])) for r in rows]!=expected:
        raise ValueError('Incomplete, duplicate or reordered timing samples')
    for row in rows:
        if any(not math.isfinite(float(row[k])) or float(row[k])<0 for k in ('vision_ms','metric_ms')):
            raise ValueError('Invalid monotonic timing sample')

def quality_samples(labels,rows,hashes):
    if len(labels['reviewRows'])!=len(labels['referenceCentresLeftRight']):
        raise ValueError('Label rows and boundary references do not align')
    if len({f['index'] for f in labels['frames']})!=len(labels['frames']):
        raise ValueError('Duplicate labelled frame')
    samples=[]
    for frame in labels['frames']:
        index=frame['index']
        if hashes[index]!=frame['sha256']:raise ValueError('Label image SHA mismatch')
        for y,(left,right) in zip(labels['reviewRows'],labels['referenceCentresLeftRight']):
            row=rows[index*labels['height']+y]
            missing=int(row['flags']!='3')
            # Missing detections remain in the denominator and fail independently.
            errors=[abs(int(row['left'])-left),abs(int(row['right'])-right)] if not missing else [labels['width']]*2
            samples.append({'sampleId':f'frame-{index:04d}-row-{y:03d}','repetition':0,'valid':True,
                'metrics':{'boundary-mae':sum(errors)/2,'boundary-max-error':max(errors),
                           'missing-pair':missing,'within-reference-band':int(not missing and max(errors)<=labels['tolerancePixels'])}})
    if not samples:raise ValueError('Empty accuracy reference')
    return samples

def loss_episodes(rows,key):
    result=[];start=None
    for i,row in enumerate(rows+[{'near_accepted':'1','full_accepted':'1'}]):
        if row[key]=='0' and start is None:start=i
        elif row[key]!='0' and start is not None:
            result.append({'firstIndex':start,'lastIndex':i-1,'frames':i-start,'modelDurationMs':20*(i-start)})
            start=None
    return result

def audit_summary(rows):
    result={'frames':len(rows),'timeModel':'uniform 20ms capture + 4ms observe; stationary synthetic encoder, no physical motion inference',
            'nearAndFullAreIndependent':True,'reasonMatchesActualNativeAcceptance':True}
    for scope in ('near','full'):
        counts=Counter(int(r[scope+'_reason']) for r in rows)
        sub=Counter(int(r[scope+'_subreason']) for r in rows)
        result[scope]={'accepted':sum(r[scope+'_accepted']=='1' for r in rows),
            'reasonCounts':{REASONS[k]:v for k,v in sorted(counts.items())},
            'subreasonCounts':{SUBREASONS[k]:v for k,v in sorted(sub.items())},
            'missingPairFirstFailureRowCounts':dict(Counter(r[scope+'_row'] for r in rows if r[scope+'_reason']=='2')),
            'missingPairFlagCounts':dict(Counter(r[scope+'_flags'] for r in rows if r[scope+'_reason']=='2')),
            'episodes':loss_episodes(rows,scope+'_accepted'),
            'exampleIndices':{SUBREASONS[k]:[int(r['index']) for r in rows if int(r[scope+'_subreason'])==k][:5] for k in sub if k}}
    result['acceptanceCombinations']=dict(Counter('near='+r['near_accepted']+',full='+r['full_accepted'] for r in rows))
    result['firstPlannerFaultIndex']=next((int(r['index']) for r in rows if r['planner_status']=='4'),None)
    return result

def run(output,route_active=False):
    out=Path(output).resolve()
    if out.exists():raise ValueError('Use a new output directory to preserve earlier evidence')
    if not (out.is_relative_to(ROOT/'.build') or out.is_relative_to(ROOT/'.test-results')):
        raise ValueError('Output must be below repository .build or .test-results')
    specs=[read_json(ROOT/p) for p in MANIFESTS]
    if route_active:
        for spec in specs:
            spec['id']+='-route-active'
            spec['parameters']['routeRecognitionEnabled']=True
    for spec in specs:validate_benchmark(spec)
    timing,quality=specs
    if timing['dataset']!=quality['dataset']:raise ValueError('Timing and quality must use the same pinned images')
    content=read_json(ROOT/timing['dataset']['path']);samples=content['samples'];count=len(samples)
    labels=read_json(ROOT/timing['dataset']['groundTruthPath'])
    session_path=ROOT/content['sessionPath']
    if sha256(session_path)!=content['sessionSha256']:raise ValueError('Session metadata hash mismatch')
    saved=read_json(session_path)
    if count!=750 or len(saved['frames'])!=count:raise ValueError('Incomplete source session')
    out.mkdir(parents=True)
    raw=out/'original.gray';hashes={}
    with raw.open('wb') as stream:
        for i,(sample,metadata) in enumerate(zip(samples,saved['frames'])):
            path=inside(ROOT,sample['path']);blob=path.read_bytes()
            if sample['id']!=f'frame-{i:04d}' or path!=session_path.parent/metadata['file']:
                raise ValueError('Dataset/session order mismatch')
            if sha256(path)!=metadata['sha256']:raise ValueError('Session frame SHA mismatch')
            decoded=decode_frame(blob,saved['frozen']['gen'],i)
            if any(decoded[k]!=metadata[k] for k in decoded):raise ValueError('VQF metadata mismatch')
            stream.write(blob[96:]);hashes[i]=sample['sha256']
    generated=header(read_json(PROFILE));(out/'phase3_shadow_profile.h').write_text(generated,encoding='utf-8')
    family=os.environ.get('SMARTCAR_TEST_COMPILER','gcc');mode=os.environ.get('SMARTCAR_TEST_MODE','optimized')
    compiler=os.environ.get('SMARTCAR_TEST_COMPILER_PATH') or shutil.which('cl' if family=='msvc' else 'gcc')
    if not compiler:raise ValueError('Selected native compiler unavailable')
    commands=[]
    def execute(command,log):
        p=subprocess.run(list(map(str,command)),cwd=out,capture_output=True,text=True,timeout=120)
        (out/log).write_text(p.stdout+p.stderr,encoding='utf-8');p.check_returncode()
    for name,source in [('timing','tools/testing/vision_image_timing.c'),
                        ('probe','tools/speed_planning/vision_image_probe.c'),
                        ('audit','tools/speed_planning/metric_input_audit.c')]:
        command=compile_command(compiler,family,mode,[ROOT/source]+[ROOT/s for s in SOURCES],
                                [ROOT/'demo/code',out],out/(name+'.exe'))
        command[1:1]=[('/D' if family=='msvc' else '-D')+f for f in FLAGS];commands.append(command)
        execute(command,name+'-build.log')
    route_args=['route-active'] if route_active else []
    execute([out/'timing.exe',raw,out/'timing.csv',count,timing['warmup'],timing['repetitions'],*route_args],'timing-run.log')
    execute([out/'probe.exe',raw,out/'rows.csv',out/'masks.gray',*route_args],'probe-run.log')
    execute([out/'audit.exe',raw,out/'audit.csv',*route_args],'audit-run.log')
    timed=csv_rows(out/'timing.csv');rows=csv_rows(out/'rows.csv');audited=csv_rows(out/'audit.csv')
    complete_grid(timed,count,timing['repetitions'])
    if [(int(r['frame']),int(r['row'])) for r in rows]!=[(i,y) for i in range(count) for y in range(120)]:
        raise ValueError('Incomplete row-level outputs')
    if [int(r['index']) for r in audited]!=list(range(count)):raise ValueError('Incomplete rejection audit')
    for r in timed:
        i=int(r['index']);first=timed[i]
        for key in ('output_hash','raw_valid','near_valid','full_valid'):
            if r[key]!=first[key]:raise ValueError('Temporal replay differs across repetitions: '+key)
        for scope in ('near','full'):
            if r[scope+'_valid']!=rows[i*120][scope+'_valid'] or r[scope+'_valid']!=audited[i][scope+'_accepted']:
                raise ValueError('Independent native probes disagree')
    # Pin all native headers and implementation files, including transitive
    # route headers; compiler/preprocessor options and generated config are saved.
    paths=set((ROOT/'demo/code').glob('*.h'))|{ROOT/s for s in SOURCES}
    paths.update((ROOT/'tools/speed_planning').glob('*.h'))
    paths.update(ROOT/p for p in ['tools/testing/vision_image_timing.c','tools/speed_planning/vision_image_probe.c',
        'tools/speed_planning/metric_input_audit.c','tools/testing/vision_image_benchmark.py',
        'tools/testing/benchmark.py','tools/speed_planning/phase3_profile.py','tools/speed_planning/replay.py',*MANIFESTS,
        timing['dataset']['path'],timing['dataset']['groundTruthPath'],str(PROFILE.relative_to(ROOT))])
    source_hashes={p.relative_to(ROOT).as_posix():sha256(p) for p in sorted(paths)}
    source_hashes['generated/phase3_shadow_profile.h']=sha256(out/'phase3_shadow_profile.h')
    fingerprint=hashlib.sha256(json.dumps(source_hashes,sort_keys=True).encode()).hexdigest()
    def git(*args):return subprocess.check_output(['git','-C',str(ROOT),*args],text=True).strip()
    version=subprocess.run([compiler,'/?' if family=='msvc' else '--version'],capture_output=True,text=True,timeout=15)
    compiler_id=family+':'+sha256(compiler)+':'+(version.stdout+version.stderr).splitlines()[0]
    common={'gitSha':git('rev-parse','HEAD'),'dirty':bool(git('status','--porcelain')),'inputTreeSha256':fingerprint,
            'compilerId':compiler_id,'environmentId':platform.node()+'|'+platform.platform()+'|'+platform.machine(),
            'mode':mode,'dataset':timing['dataset']}
    measurements=[]
    for spec in specs:
        measurements.append(dict(common,parameters=spec['parameters'],warmup=spec['warmup'],repetitions=spec['repetitions'],samples=[]))
    measurements[0]['samples']=[{'sampleId':f"frame-{int(r['index']):04d}",'repetition':int(r['repetition']),'valid':True,
        'metrics':{'processing-time':float(r['vision_ms']),'metric-adapter-time':float(r['metric_ms']),
                   'vision-plus-metric-time':float(r['vision_ms'])+float(r['metric_ms'])}} for r in timed]
    # Quality has its own two complete cold-start chronological replays. The
    # second pass is rerun through the row probe, not duplicated measurements.
    measurements[1]['samples']=quality_samples(labels,rows,hashes)
    execute([out/'probe.exe',raw,out/'rows-repeat.csv',out/'masks-repeat.gray',*route_args],'probe-repeat-run.log')
    again=csv_rows(out/'rows-repeat.csv')
    if again!=rows:raise ValueError('Accuracy replay is not deterministic')
    repeated=quality_samples(labels,again,hashes)
    for sample in repeated:sample['repetition']=1
    measurements[1]['samples']+=repeated
    results=[]
    for spec,measurement in zip(specs,measurements):
        result=compare(spec,measurement);results.append(result)
        write_json(out/(spec['id']+'-measurements.json'),measurement)
        write_json(out/(spec['id']+'-result.json'),result)
        write_json(out/(spec['id']+'-manifest.json'),spec)
    audit=audit_summary(audited)
    # These raw-image cases were visually inspected during the audit. Record
    # current outputs without asserting that the known failure must persist.
    audit['visuallyReviewedClearSceneCases']=[{
        'index':i,'review':'Visible white road and side transitions at row54; targeted diagnostic review, not independent labels',
        'rawValid':int(rows[i*120+54]['valid']),'row54Flags':int(rows[i*120+54]['flags']),
        'row54Threshold':int(rows[i*120+54]['threshold']),
        'junctionRows':int(rows[i*120+54]['junction_rows']),'junctionAnchors':int(rows[i*120+54]['junction_anchors']),
        'metricNearAccepted':int(audited[i]['near_accepted']),'metricFullAccepted':int(audited[i]['full_accepted'])}
        for i in (32,80,99)]
    write_json(out/'audit-summary.json',audit)
    timing_summary={}
    for key in ('vision_ms','metric_ms'):
        values=[float(r[key]) for r in timed]
        timing_summary[key]={s:statistic(values,s) for s in ('mean','median','p95','p99','max')}
    per_pass=[{'repetition':i,**{s:statistic([float(r['vision_ms']) for r in timed if int(r['repetition'])==i],s)
              for s in ('mean','p95','p99','max')}} for i in range(timing['repetitions'])]
    report={'status':'PASS' if all(r['status']=='PASS' for r in results) else 'FAIL','routeRecognitionEnabled':route_active,
        'accuracyScope':labels['labelMethod'],'qualityRowsPerReplay':len(measurements[1]['samples'])//2,
        'timingScope':'Host monotonic clock; preloaded 188x120 originals; no IO/allocation/diagnostic output in timed calls; not MCU timing',
        'timing':timing_summary,'visionTimingPerPass':per_pass,'audit':audit,'compilerCommands':commands,'sourceSha256':source_hashes,
        'metadata':common,'framesVerified':count,'hardwareAccessed':False,'baselineApproved':False,
        'limitations':['28 local post-selection boundary bands, no independent holdout or all-frame accuracy claim',
                       'Timing observation only: no approved host regression baseline or target WCET claim',
                       'Uniform diagnostic time and stationary encoder; recorded historical board timings are not current implementation timings']}
    write_json(out/'report.json',report)
    lines=['# Vision image performance and regression','',f"Status: **{report['status']}**",'',
        '| Metric | Value | Unit | Decision |','| --- | ---: | --- | --- |']
    for result in results:
        lines += [f"| {m['id']} | {m['value']:.9g} | {m['unit']} | {m['decision']} |" for m in result['metrics']]
    lines += ['',f"Original frames: {count}; local labelled pairs: {report['qualityRowsPerReplay']} per replay.",
        'Accuracy and speed gate independently; missing labelled pairs are counted as failures.',
        'All-frame acceptance is availability, not accuracy. Timings are host observations.', '', '## Near/full rejection audit','']
    for scope in ('near','full'):
        lines += [scope+': '+json.dumps(audit[scope]['subreasonCounts']), '']
    lines += ['## Targeted clear-scene review','',
        'Frames 32, 80, 99: visually visible road at row54. Current outputs below are diagnostic observations; local labelled-band PASS does not certify these cases.',
        json.dumps(audit['visuallyReviewedClearSceneCases']), '']
    lines += report['limitations']
    (out/'report.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print(json.dumps({'status':report['status'],'routeRecognitionEnabled':route_active,'timing':timing_summary,'near':audit['near']['subreasonCounts'],
                      'full':audit['full']['subreasonCounts'],'report':str(out/'report.md')},indent=2))
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path.cwd()/'vision-image-performance')
    args=parser.parse_args()
    try:
        observe=run(args.output)
        active=run(args.output.with_name(args.output.name+'-route-active'),True)
        sys.exit(0 if observe['status']==active['status']=='PASS' else 1)
    except (ValueError,OSError,subprocess.SubprocessError) as error:
        print('VISION BENCHMARK ERROR: '+str(error),file=sys.stderr);sys.exit(2)
