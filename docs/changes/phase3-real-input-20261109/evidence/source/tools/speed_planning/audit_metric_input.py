"""Audit original images through the current native adapter and shadow planner.

The tool changes no firmware/calibration and accesses no device. Image bytes
are original; planner composition uses explicit stationary synthetic encoder
feedback and uniform relative timestamps, not measured closed-loop motion.
Actual capture times remain in the original manifest.
"""
import argparse
from collections import Counter
import csv
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[2]
from tools.speed_planning.phase3_profile import PROFILE,header
from tools.speed_planning.visual_trace import decode_frame

NAMES={0:'accepted',1:'source_invalid_or_quality',2:'requested_row_not_two_measured_edges',
       3:'adapter_geometry_or_history',4:'outside_configured_ground_domain',5:'near_geometry_required'}


def run(output,resample=False):
    output=Path(output).resolve()
    if output.exists() or not output.is_relative_to(ROOT/'.build') or output==ROOT/'.build':
        raise ValueError('A new .build output directory required')
    compiler=shutil.which('gcc')
    if not compiler:raise ValueError('Existing GCC required')
    profile=json.loads(PROFILE.read_text('utf-8'));generated=header(profile)
    session=ROOT/'docs/changes/phase2-calibration-20261108/evidence/visual-continuous/session04'
    manifest=json.loads((session/'session.json').read_text('utf-8'))
    assert manifest['complete'] and len(manifest['frames'])==750
    output.mkdir(parents=True);(output/'phase3_shadow_profile.h').write_text(generated,'utf-8')
    raw=output/'original-images.gray';frame_hashes={}
    with raw.open('wb') as stream:
        for i,saved in enumerate(manifest['frames']):
            path=session/saved['file'];blob=path.read_bytes();digest=hashlib.sha256(blob).hexdigest()
            if digest!=saved['sha256']:raise ValueError('Original frame SHA mismatch')
            decoded=decode_frame(blob,manifest['frozen']['gen'],i)
            if any(decoded[k]!=saved[k] for k in decoded):raise ValueError('Metadata mismatch')
            stream.write(blob[96:]);frame_hashes[saved['file']]=digest
    sources=[ROOT/n for n in ('tools/speed_planning/metric_input_audit.c','demo/code/vision_midterm.c','demo/code/vision_metric_io.c',
             'demo/code/vision_planner.c','demo/code/vision_planner_shadow.c','tools/speed_planning/vision_adapter.c','tools/speed_planning/speed_planner.c')]
    flags=['VISION_MIDTERM_TRAINING=1','VISION_PLANNER_RUNTIME=1','VISION_ROUTE_SWITCH=1','VISION_ENCODER_ENABLE=1',
           'VISION_MIDTERM_ROUTE_TRIAL=1','VISION_MIDTERM_AUTONOMOUS=0']
    exe=output/'metric-audit.exe';cmd=[compiler,'-std=c11','-O2','-Wall','-Werror','-Wno-misleading-indentation',
            *['-D'+f for f in flags],'-I'+str(ROOT/'demo/code'),'-I'+str(output),*map(str,sources),'-lm','-o',str(exe)]
    compiled=subprocess.run(cmd,capture_output=True,text=True,timeout=60)
    (output/'build.log').write_text(compiled.stdout+compiled.stderr,'utf-8');compiled.check_returncode()
    result=subprocess.run([exe,raw,output/'frames.csv']+(['paired-resample'] if resample else []),capture_output=True,text=True,timeout=60)
    (output/'run.log').write_text(result.stdout+result.stderr,'utf-8');result.check_returncode()
    with (output/'frames.csv').open() as file:rows=list(csv.DictReader(file))
    summaries={}
    for scope in ('near','full'):
        counts=Counter(int(row[scope+'_reason']) for row in rows)
        summaries[scope]={NAMES[key]:counts.get(key,0) for key in NAMES}
    report={'scope':'Actual original images and current C detector/metric adapter; rejection audit, no correctness ground truth or active control approval',
            'profileId':profile['profileId'],'frames':len(rows),'sourceFramesCrcAndShaVerified':len(frame_hashes),
            'classificationMatchedActualAdapterForAllFrames':True,'uniformRelativeTimeModelMs':20,
            'sourceSession':str(session.relative_to(ROOT)).replace('\\','/'),'counts':summaries,
            'experimentalPairedRowResampling':resample,'scopeEndpointsChanged':False,
            'plannerComposition':{'encoder':'explicit synthetic stationary feedback; no measured motion',
                'admission':'diagnostic caller assertion, not physical C26 authorization',
                'armIndices':[int(r['index']) for r in rows if r['arm_event']=='1'],
                'readyIndices':[int(r['index']) for r in rows if r['planner_status']=='2'],
                'supervisorModeCounts':dict(Counter(r['supervisor_mode'] for r in rows)),
                'peakTicksRight':max(int(r['target_right']) for r in rows),
                'peakTicksLeft':max(int(r['target_left']) for r in rows),'autoRearmAfterLatch':False},
            'compileCommand':cmd,'sourceSha256':{str(p.relative_to(ROOT)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
            'profileSha256':hashlib.sha256(PROFILE.read_bytes()).hexdigest(),'hardwareAccessed':False,
            'firmwareChanged':False,'domainChanged':False,'productionApproved':False}
    (output/'report.json').write_text(json.dumps(report,indent=2)+'\n','utf-8')
    (output/'frame-hashes.json').write_text(json.dumps(frame_hashes,indent=2)+'\n','utf-8')
    print(result.stdout.strip());print(json.dumps(summaries,indent=2));print(json.dumps(report['plannerComposition'],indent=2));return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--paired-resample',action='store_true',help='Offline experiment only; retain scope endpoints and domain')
    args=parser.parse_args();run(args.output,args.paired_resample)
