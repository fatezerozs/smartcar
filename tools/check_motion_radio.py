"""One module-level test of main's ASCII radio transport and identity.

Never sends START, ARM, reset, erase or motion commands.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
SCRIPT=r'''param([string]$Out,[string]$Port)
$ErrorActionPreference='Stop'
$p=New-Object System.IO.Ports.SerialPort($Port,115200,'None',8,'One')
$p.DtrEnable=$false;$p.RtsEnable=$false;$p.Handshake='None';$p.WriteTimeout=2000
$f=[IO.File]::Open((Join-Path $Out 'rx.bin'),'CreateNew','Write')
$log=New-Object IO.StreamWriter((Join-Path $Out 'tx.log'))
$buf=New-Object byte[] 16384
try{
 $p.Open();$p.Write("`r`nx`r`n");$log.WriteLine('x: native stopped-only radio ownership claim');$log.Flush()
 $watch=[Diagnostics.Stopwatch]::StartNew();$next=1000;$queries=0
 while($watch.ElapsedMilliseconds -lt 20000){
  if($queries -lt 5 -and $watch.ElapsedMilliseconds -ge $next){
   $text="`r`n@MOTION STATUS`r`n"
   $p.Write($text);$log.WriteLine(([DateTime]::UtcNow.ToString('o')+' @MOTION STATUS'));$log.Flush()
   $queries++;$next+=3500
  }
  $n=$p.BytesToRead
  if($n -gt 0){$count=$p.Read($buf,0,[Math]::Min($n,$buf.Length));$f.Write($buf,0,$count)}
  Start-Sleep -Milliseconds 10
 }
}finally{$log.Dispose();$f.Dispose();if($p.IsOpen){$p.Close()};$p.Dispose()}
'''


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='COM10')
    parser.add_argument('--build',required=True,type=Path)
    args=parser.parse_args()
    build=args.build.resolve()
    proof=json.loads((build/'motion-evidence.json').read_text())
    source=proof['source_digest']
    if proof.get('transport')!='main_radio115200':raise ValueError('Main radio candidate required')
    changed=[p for p,h in proof['sources'].items() if hashlib.sha256((ROOT/p).read_bytes()).hexdigest()!=h]
    # Read-only STATUS can inspect a manually saved project. Source changes
    # remain a stop; report project drift without rewriting frozen evidence.
    if any(not p.replace('\\','/').endswith('/candidate.uvprojx') for p in changed):
        raise ValueError('Candidate source inputs changed; audit before board test')
    out=ROOT.parent/'wireless_diagnostics'/('module-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'))
    out.mkdir(parents=True)
    (out/'capture.ps1').write_text(SCRIPT)
    result=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(out/'capture.ps1'),
                           '-Out',str(out),'-Port',args.port],capture_output=True,text=True,timeout=30)
    raw=(out/'rx.bin').read_bytes()
    import re
    lines=[s.decode('ascii') for s in re.findall(rb'MOTION id=[ -~]+\r?\n',raw)]
    lines=[s.rstrip('\r\n') for s in lines]
    identity=[line for line in lines if line.startswith('MOTION id=RECORDER_MOTION_C26_3S_1 ')
              and f'source={source} ' in line and 'transport=main_radio115200 ' in line
              and 'outputs=bounded_2900ms_pwm2000 ' in line and 'c26=1 camera=0 storage=0 ' in line]
    passed=result.returncode==0 and len(identity)>=2
    report={'module':'main ASCII radio communication','passed':passed,'source':source,
            'changed_project_inputs':changed,
            'local_axf_sha256':hashlib.sha256((build/'Objects/recorder_motion_c26_3s.axf').read_bytes()).hexdigest(),
            'valid_identity_count':len(identity),
            'bytes':len(raw),'raw_sha256':hashlib.sha256(raw).hexdigest(),
            'start_sent':False,'arm_sent':False,'motion_tested':False,'port_closed':True,
            'criteria':'at least two complete matching idle/OFF identity lines during five STATUS queries',
            'limitations':'Native main ASCII transport has no frame CRC; matching lines do not exclude interference or prove future reliability',
            'stderr':result.stderr}
    (out/'decoded.txt').write_text('\n'.join(lines),encoding='utf-8')
    (out/'result.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2));print('Evidence:',out)
    return 0 if passed else 1


if __name__=='__main__':sys.exit(main())
