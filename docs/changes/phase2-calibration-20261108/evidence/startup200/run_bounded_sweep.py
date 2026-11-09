import json,re,subprocess,time,hashlib,shutil
from pathlib import Path
from datetime import datetime,timezone
root=Path.cwd();p=root/'docs/changes/phase2-calibration-20261108';f=p/'startup200-candidate.json'
s=json.loads(f.read_text(encoding='utf-8'));a=Path(s['archive']);e=p/'evidence/startup200'
runs=[];stop='MAX_REACHED_WITHOUT_BOTH_RESPONSE'
for duty in range(150,201,10):
 if runs:time.sleep(2.1)
 stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
 console=a/f'lifted-{duty}-{stamp}.console.txt'
 cmd=['pwsh','-NoLogo','-NoProfile','-File',str(a/'exact-client/motion_test.ps1'),'-Mode','line','-Port','COM10','-Role','radio','-DutyPermille',str(duty),'-DurationMs','3000','-Execute']
 result=subprocess.run(cmd,capture_output=True,text=True,encoding='utf-8',errors='replace')
 text=result.stdout+result.stderr;console.write_text(text,encoding='utf-8')
 m=re.search(r'Log: (.+\.jsonl)',text)
 # Parse only the complete raw log, rebuilding serial chunks into text lines.
 log=Path(m[1].strip()) if m else None
 response='';samples=[]
 if log and log.exists():
  records=[json.loads(x) for x in log.read_text(encoding='utf-8-sig').splitlines() if x.strip()]
  response=''.join(x.get('data','') for x in records if x.get('direction')=='RX')
  phase=None
  for line in response.splitlines():
   q=re.search(r'\[MOTION\] seq=\d+ kind=\d+ phase=(\d+)',line)
   if q:phase=int(q[1])
   q=re.search(r'\[SPEED\] ms=(\d+) sample=(\d+).*?raw3_L=(-?\d+) raw4_R=(-?\d+)',line)
   if q and phase==2:samples.append([int(q[1]),int(q[2]),int(q[3]),int(q[4])])
 consecutive=0;peak_consecutive=0
 for ms,seq,left,right in samples:
  consecutive=consecutive+1 if left<=-5 and right>=5 else 0
  peak_consecutive=max(peak_consecutive,consecutive)
 accepted='[MOTION] STARTED' in response
 ended=bool(re.search(r'\[MOTION\] seq=\d+ kind=\d+ phase=0 reason=1\b',response))
 run={'recordedAtUtc':datetime.now(timezone.utc).isoformat(),'requestPermille':duty,'durationMs':3000,'port':'COM10','role':'radio','mode':'lifted','accepted':accepted,'hostExitCode':result.returncode,'normalTimedEndConfirmed':ended,'sampleCount':len(samples),'rawSamplesMsSeqLeftRight':samples,'maxConsecutiveBothForwardResponseSamples':peak_consecutive,'bothResponseScreeningCriterion':'5 consecutive received motion-phase samples: left<=-5, right>=5 counts/10ms. Diagnostic response screen only; not physical stability/accuracy acceptance.','bothEncoderResponseObserved':peak_consecutive>=5,'operatorObservation':None,'physicalMovementVerified':False,'console':str(console),'rawLog':str(log) if log else None}
 if samples:run['leftRawMinMax']=[min(x[2] for x in samples),max(x[2] for x in samples)];run['rightRawMinMax']=[min(x[3] for x in samples),max(x[3] for x in samples)]
 runs.append(run)
 for q in [console,log]:
  if q and q.exists():
   dst=e/q.name;shutil.copy2(q,dst);s.setdefault('evidenceSha256',{})[dst.relative_to(p).as_posix()]=hashlib.sha256(dst.read_bytes()).hexdigest()
 s['startupSweepRuns']=runs;s['status']='BOUNDED_SWEEP_IN_PROGRESS';f.write_text(json.dumps(s,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:run.get(k) for k in ['requestPermille','accepted','hostExitCode','normalTimedEndConfirmed','sampleCount','leftRawMinMax','rightRawMinMax','maxConsecutiveBothForwardResponseSamples']},ensure_ascii=False),flush=True)
 if result.returncode!=0 or not accepted or not ended:
  stop='ABORT_COMMAND_OR_FINAL_STATUS_FAILURE';break
 if peak_consecutive>=5:
  stop='BOTH_ENCODER_RESPONSE_SCREEN_MET';break
s['status']=stop;s['startupSweepStopReason']=stop;s['physicalTestPassed']=False
f.write_text(json.dumps(s,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('SWEEP_END '+stop,flush=True)
