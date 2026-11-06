"""Explicit joint handoff regression; no whole-tree framework discovery."""
from pathlib import Path
import datetime,json,subprocess
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'docs/changes/recorder-b3-20261106/encoder-joint-static-20261106'/('joint-host-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'))
out.mkdir()
variant=next(s for s in json.loads((ROOT/'tests/suites/catalog.json').read_text())['suites'] if s['id']=='route-switch')['entrypoint']['variants'][0]
sources=[p for p in variant['sources'] if not p.startswith('tests/')]
sources+=['demo/recorder_joint/joint_control.c','demo/code/vision_midterm.c','demo/code/track_bayes.c','tests/recorder/joint_control_harness.c']
include=['tests/recorder/startup_mocks','tests/recorder/mocks','tests/vision/stubs','demo/recorder/include','demo/recorder_joint/include','demo/recorder_b3/include','demo/code']
command=['gcc','-std=c11','-O2','-Wall','-Werror','-Wno-misleading-indentation','-Wno-unused-function',*['-I'+str(ROOT/p) for p in include],*['-D'+d for d in variant['defines']],*map(str,[ROOT/p for p in sources]),'-lm','-o',str(out/'joint-control.exe')]
compiled=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=90)
(out/'compile.log').write_text(compiled.stdout+compiled.stderr)
if compiled.returncode:raise RuntimeError(compiled.stderr[-3500:])
tested=subprocess.run([str(out/'joint-control.exe')],cwd=ROOT,capture_output=True,text=True,timeout=60)
(out/'run.log').write_text(tested.stdout+tested.stderr)
records=[{'name':'native C26 and joint handoff','passed':tested.returncode==0,'command':command}]
if tested.returncode:raise RuntimeError(tested.stdout+tested.stderr)
metric_includes=['tests/recorder/b3/mocks','tests/recorder/mocks','demo/recorder/include','demo/recorder_joint/include','demo/recorder_b3/include','demo/recorder_b2/include']
metric_command=['gcc','-std=c11','-O2','-Wall','-Werror','-Wno-misleading-indentation',*['-I'+str(ROOT/p) for p in metric_includes],str(ROOT/'tests/recorder/joint_metrics_harness.c'),'-o',str(out/'joint-metrics.exe')]
metric_compile=subprocess.run(metric_command,cwd=ROOT,capture_output=True,text=True,timeout=60)
(out/'metrics-compile.log').write_text(metric_compile.stdout+metric_compile.stderr)
if metric_compile.returncode:raise RuntimeError(metric_compile.stderr)
metrics=subprocess.run([str(out/'joint-metrics.exe')],cwd=ROOT,capture_output=True,text=True,timeout=30)
(out/'metrics-run.log').write_text(metrics.stdout+metrics.stderr)
records.append({'name':'phase metrics error and isolation guards','passed':metrics.returncode==0,'command':metric_command})
(out/'result.json').write_text(json.dumps({'passed':all(r['passed'] for r in records),'tests':records,'evidence':'host simulated IO, not RTOS scheduling or SD hardware'},indent=2))
print(tested.stdout+tested.stderr+metrics.stdout+metrics.stderr);print('Evidence:',out)
raise SystemExit(metrics.returncode)
