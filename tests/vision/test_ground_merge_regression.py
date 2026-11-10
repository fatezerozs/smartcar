"""Fail on the original gray-floor pixels becoming white in the native mask.

This gate covers three visually reviewed, post-selection clear-road cases. It
does not use coverage counts or existing accepted reference frames as a proxy.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools.speed_planning.phase3_profile import PROFILE,header
from tools.speed_planning.replay import compile_command
from tools.speed_planning.visual_trace import decode_frame
from tools.testing.vision_image_benchmark import FLAGS,SOURCES

def run(output,indices=None,cold=False):
    out=Path(output).resolve()
    if out.exists() or not any(out.is_relative_to(ROOT/name) and out!=ROOT/name for name in ('.build','.test-results')):
        raise ValueError('Use a new output folder under repository .build or .test-results')
    labels_path=ROOT/'tests/vision/fixtures/ground-merge-cases-20261109.json'
    labels=json.loads(labels_path.read_text('utf-8'))
    cases=[c for c in labels['cases'] if indices is None or c['index'] in indices]
    if not cases or (indices is not None and set(indices)!={c['index'] for c in cases}):
        raise ValueError('Unknown or empty reviewed case selection')
    session=ROOT/labels['sourceSession'];saved=json.loads((session/'session.json').read_text('utf-8'))
    selected=[c['index'] for c in cases] if cold else list(range(max(c['index'] for c in cases)+1))
    if cold and len(cases)!=1:raise ValueError('Cold replay requires exactly one selected case')
    out.mkdir(parents=True)
    raw=out/'original.gray';hashes={}
    with raw.open('wb') as stream:
        for i in selected:
            metadata=saved['frames'][i];blob=(session/metadata['file']).read_bytes()
            digest=hashlib.sha256(blob).hexdigest()
            if digest!=metadata['sha256']:raise ValueError('Original SHA mismatch')
            decoded=decode_frame(blob,saved['frozen']['gen'],i)
            if any(decoded[k]!=metadata[k] for k in decoded):raise ValueError('VQF metadata mismatch')
            stream.write(blob[96:]);hashes[i]=digest
    for case in cases:
        if hashes[case['index']]!=case['sha256']:raise ValueError('Annotation/input identity mismatch')
    (out/'phase3_shadow_profile.h').write_text(header(json.loads(PROFILE.read_text('utf-8'))),encoding='utf-8')
    family=os.environ.get('SMARTCAR_TEST_COMPILER','gcc');mode=os.environ.get('SMARTCAR_TEST_MODE','optimized')
    compiler=os.environ.get('SMARTCAR_TEST_COMPILER_PATH') or shutil.which('cl' if family=='msvc' else 'gcc')
    if not compiler:raise ValueError('Native compiler missing')
    source_paths=[ROOT/'tools/speed_planning/vision_image_probe.c']+[ROOT/p for p in SOURCES]
    exe=out/'ground-merge-probe.exe'
    command=compile_command(compiler,family,mode,source_paths,[ROOT/'demo/code',out],exe)
    command[1:1]=[('/D' if family=='msvc' else '-D')+f for f in FLAGS]
    built=subprocess.run(command,cwd=out,capture_output=True,text=True,timeout=90)
    (out/'build.log').write_text(built.stdout+built.stderr,encoding='utf-8');built.check_returncode()
    results=[]
    for enabled in (False,True):
        name='route-active' if enabled else 'observe'
        rows_path=out/(name+'-rows.csv');masks_path=out/(name+'-masks.gray')
        p=subprocess.run([str(exe),str(raw),str(rows_path),str(masks_path)]+(['route-active'] if enabled else []),
                         cwd=out,capture_output=True,text=True,timeout=30)
        (out/(name+'-run.log')).write_text(p.stdout+p.stderr,encoding='utf-8');p.check_returncode()
        with rows_path.open(encoding='utf-8',newline='') as f:rows=list(csv.DictReader(f))
        masks=masks_path.read_bytes();size=labels['width']*labels['height']
        if len(masks)!=len(selected)*size or len(rows)!=len(selected)*labels['height']:
            raise ValueError('Incomplete native outputs')
        for case in cases:
            index=selected.index(case['index']);y=case['row'];r=rows[index*labels['height']+y]
            if int(r['frame'])!=index or int(r['row'])!=y:raise ValueError('Native row order mismatch')
            pixels=[dict(probe,actual=masks[index*size+y*labels['width']+probe['x']]) for probe in case['expectedPixels']]
            boundary_checks=[]
            for band in case.get('boundaryBands',[]):
                measured=rows[index*labels['height']+band['row']]
                boundary_checks.append(dict(band,actualLeft=int(measured['left']),actualRight=int(measured['right']),
                    flags=int(measured['flags']),passed=int(measured['flags'])==3 and
                    abs(int(measured['left'])-band['left'])<=labels['boundaryTolerancePixels'] and
                    abs(int(measured['right'])-band['right'])<=labels['boundaryTolerancePixels']))
            geometry=all(int(r[key])==1 for key in ('valid','near_valid','full_valid'))
            passed=all(p['actual']==p['value'] for p in pixels) and all(b['passed'] for b in boundary_checks) and geometry
            results.append({'index':case['index'],'mode':name,'passed':passed,'pixels':pixels,
                'boundaryChecks':boundary_checks,'geometryAccepted':geometry,
                'threshold':int(r['threshold']),'rowFlags':int(r['flags']),'rawValid':int(r['valid']),
                'nearValid':int(r['near_valid']),'fullValid':int(r['full_valid'])})
            print(f"{'PASS' if passed else 'FAIL'} {name} frame={case['index']} row={y} threshold={r['threshold']} flags={r['flags']} pixels="+
                  ','.join(f"x{p['x']}:{p['actual']}/{p['value']}" for p in pixels),flush=True)
    report={'status':'PASS' if all(c['passed'] for c in results) else 'FAIL','coldSingleFrame':cold,
            'replayedOriginalFrames':selected,'cases':results,'compileCommand':command,
            'sourceSha256':{p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in source_paths},
            'labelsSha256':hashlib.sha256(labels_path.read_bytes()).hexdigest(),
            'scope':labels['annotation'],'hardwareAccessed':False,'productionCodeModifiedByTest':False}
    (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path.cwd()/'ground-merge-check')
    parser.add_argument('--indices',type=int,nargs='+');parser.add_argument('--cold',action='store_true')
    args=parser.parse_args()
    try:sys.exit(0 if run(args.output,args.indices,args.cold)['status']=='PASS' else 1)
    except (ValueError,OSError,subprocess.SubprocessError) as error:
        print('GROUND MERGE CHECK ERROR: '+str(error),file=sys.stderr);sys.exit(2)
