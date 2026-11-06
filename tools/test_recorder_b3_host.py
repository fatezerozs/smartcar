"""Run only explicit C26/Encoder/PI catalog cases and the private-hook variant.

Bypasses unrelated framework/temp discovery; uses unchanged assertions and a new
task-owned output directory. No hardware, production writes or runtime models.
"""
from pathlib import Path
import argparse,json,subprocess,datetime
ROOT=Path(__file__).resolve().parents[1]
def run(build,private_only=False):
 build=Path(build).resolve();assert build.is_relative_to(ROOT/'.build/recorder-b3-20261106')
 out=ROOT/'docs/changes/recorder-b3-20261106/evidence'/('host-regression-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'));out.mkdir()
 suites={s['id']:s for s in json.loads((ROOT/'tests/suites/catalog.json').read_text())['suites']};records=[]
 cases=[('route-switch',True)] if private_only else [('route-switch',False),('speed',False),('encoder-board',False),('route-switch',True)]
 for name,private in cases:
  v=suites[name]['entrypoint']['variants'][0];tag=name+('-private-hooks' if private else '-baseline')
  sources=[ROOT/p for p in v['sources']]
  if private:sources=[build/'adapted'/p.name if p.name in ('app_mode.c','vision_speed.c') else p for p in sources]
  flags=['gcc','-std=c11','-O2','-Wall','-Werror','-Wno-misleading-indentation','-Wno-unused-function']
  flags+=['-I'+str(ROOT/p) for p in v['includeDirectories']]+['-D'+d for d in v['defines']]
  if private:flags+=['-I'+str(ROOT/'demo/recorder_b3/include'),'-include',str(ROOT/'tests/recorder/b3/hook_probe.h')];sources.append(ROOT/'tests/recorder/b3/hook_probe.c')
  command=[*flags,*map(str,sources),'-lm','-o',str(out/(tag+'.exe'))]
  compiled=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=90)
  (out/(tag+'-compile.log')).write_text(compiled.stdout+compiled.stderr)
  if compiled.returncode:raise RuntimeError(tag+' compile failed: '+compiled.stderr[-2500:])
  tested=subprocess.run([str(out/(tag+'.exe'))],cwd=ROOT,capture_output=True,text=True,timeout=60)
  (out/(tag+'-run.log')).write_text(tested.stdout+tested.stderr)
  if tested.returncode:raise RuntimeError(tag+' failed: '+tested.stderr[-2500:])
  records.append({'test':tag,'passed':True,'command':command,'assertions_modified':False,'evidence':'host_simulated'})
  print('PASS',tag,tested.stdout[-220:].strip())
 (out/'result.json').write_text(json.dumps(records,indent=2));print('Evidence:',out)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',required=True);p.add_argument('--private-only',action='store_true');a=p.parse_args();run(a.build,a.private_only)
