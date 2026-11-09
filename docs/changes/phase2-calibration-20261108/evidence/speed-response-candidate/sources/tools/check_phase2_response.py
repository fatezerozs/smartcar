"""Host-check an isolated response candidate. Never opens a port or downloads."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.speed_planning.export_speed_response import FIELDS,SIGNED,validate_trace


def check(candidate,compiler):
    p=Path(candidate).resolve();manifest=json.loads((p/'candidate.json').read_text(encoding='utf-8'))
    if not p.is_relative_to(ROOT/'.build/phase2-calibration-20261108') or manifest['version']!='PHASE2_SPEED_RESPONSE_20261109_1':
        raise ValueError('Wrong isolated candidate')
    suite=next(s for s in json.loads((ROOT/'tests/suites/catalog.json').read_text(encoding='utf-8'))['suites'] if s['id']=='speed')
    variant=suite['entrypoint']['variants'][0];out=p/'host';out.mkdir(exist_ok=True)
    sources=[ROOT/'tests/planning/test_speed_response.c']+[p/Path(x) for x in variant['sources'][1:]]+[p/'demo/code/stop_trace.c']
    command=[str(compiler),'-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
             *['-D'+x for x in variant['defines']],'-I'+str(ROOT/'tests/vision/stubs'),'-I'+str(p/'demo/code'),
             *map(str,sources),'-lm','-o',str(out/'response-test.exe')]
    result=subprocess.run(command,capture_output=True,text=True,timeout=60)
    (out/'build.log').write_text(result.stdout+result.stderr,encoding='utf-8')
    if result.returncode:raise RuntimeError(result.stderr)
    result=subprocess.run([out/'response-test.exe'],cwd=out,capture_output=True,text=True,timeout=30)
    (out/'run.log').write_text(result.stdout+result.stderr,encoding='utf-8')
    if result.returncode:raise RuntimeError(result.stdout+result.stderr)
    native=(out/'response-native.bin').read_bytes();magic,generation,count,stop,crc=struct.unpack_from('<5I',native)
    if magic!=0x31505352 or len(native)!=60+count*88:raise ValueError('Bad native trace')
    config=list(struct.unpack_from('<10I',native,20))
    rows=[]
    for index in range(count):
        raw=struct.unpack_from('<22I',native,60+index*88)
        rows.append({key:value-0x100000000 if key in SIGNED and value>=0x80000000 else value for key,value in zip(FIELDS,raw)})
    verified=validate_trace({'generation':generation,'count':count,'stop_ms':stop,'crc32':crc},rows,config)
    report={'hostPassed':True,'compiler':str(compiler),'compileCommand':command,'nativeTraceVerification':verified,
            'stdout':result.stdout,'scope':'Actual private adapters and PI with fake timer/counters/PWM; no hardware.',
            'sourcesSha256':{str(path.relative_to(ROOT)):hashlib.sha256(path.read_bytes()).hexdigest() for path in sources},
            'flashed':False,'productionApproved':False}
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(result.stdout.strip());print(json.dumps(verified))
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--candidate',type=Path,required=True);parser.add_argument('--compiler',required=True)
    args=parser.parse_args();check(args.candidate,args.compiler)
