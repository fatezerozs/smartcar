"""Single prepared bench/ground response action, dry-run by default. No flash or retries."""
import argparse
import json
import math
from pathlib import Path
import re
import time

from tools.speed_planning.export_stop_trace import Link
from tools.speed_planning.export_speed_response import VERSION,TURN_VERSION,SUPPORTED_VERSIONS,CONFIG_FIELDS,validate_trace,parse_info


def gain_configuration(args):
    kp,ki=getattr(args,'kp',2.0),getattr(args,'ki',0.1)
    if not math.isfinite(kp) or not math.isfinite(ki) or not 0<kp<=5 or not 0<=ki<=0.2 or int(kp*256)==0:
        raise ValueError('Gains must be finite:0<kp<=5,0<=ki<=0.2,nonzero Q8kp')
    return kp,ki,int(kp*256),int(ki*256)


def execute(args):
    version=getattr(args,'expected_build',VERSION)
    if version not in SUPPORTED_VERSIONS:raise ValueError('Unsupported expected build')
    kp,ki,kp_q8,ki_q8=gain_configuration(args)
    if not 1<=args.request<=600 or not 100<=args.duration<=3000:
        raise ValueError('Request1..600permille, duration100..3000ms')
    if not 1<=args.target_ticks<=60:
        raise ValueError('Closed-loop full-ramp target1..60counts/10ms')
    command=f'@motion line {args.request} {args.duration}'
    kind=getattr(args,'kind','line');servo=getattr(args,'servo_us',1550)
    if kind not in ('line','circle'):raise ValueError('Only line or bounded circle supported')
    if kind=='circle':
        if version!=TURN_VERSION or args.mode!='open' or args.request>250 or args.duration>1500 or servo not in (1500,1600):
            raise ValueError('Arc requires turn build, open loop, request<=250, duration<=1500, servo1500/1600')
        command=f'@motion circle {args.request} {args.duration} {servo}'
    elif servo!=1550:raise ValueError('Line requires servo1550')
    ground_ready=getattr(args,'ground_ready',False)
    if args.bench_ready and ground_ready:
        raise ValueError('Choose one physical setup: bench-ready or ground-ready')
    plan={'version':version,'command':command,'mode':args.mode,'targetTicks10ms':args.target_ticks if args.mode=='closed' else None,
          'closedLoopFeedforward':0,'normalStraightRequestPermille':450,'hardCapPermille':600,
          'configureRam':args.configure,'execute':args.execute,
          'scope':'One operator-prepared ground action' if ground_ready else 'One lifted-bench action; hardware motion is not run by default.'}
    plan.update(kpRequested=kp,kiRequested=ki,kpQ8=kp_q8,kiQ8=ki_q8)
    plan.update(kind=kind,servoUs=servo)
    if not args.execute:
        print(json.dumps(plan));return plan
    if not (args.bench_ready or ground_ready) or not args.port or not args.output:
        raise ValueError('Execution requires --bench-ready or --ground-ready, --port and a new --output directory')
    args.output.mkdir(parents=True,exist_ok=False)
    (args.output/'plan.json').write_text(json.dumps(plan,indent=2)+'\n',encoding='utf-8')
    with (args.output/'serial-raw.jsonl').open('x',encoding='utf-8') as log:
        link=Link(args.port,log)
        try:
            link.send('!s\r\n')
            link.wait_line(lambda line:line in ('[LINK] STOP','[FIELD] STOP'))
            claim=link.query('x',lambda line:line.startswith('[LINK] port='))
            if not re.search(r'port=(usb|radio) owner=\1\b',claim):raise ValueError('Ownership not confirmed')
            link.query('?t',lambda line:f'build={version};' in line)
            if args.release_verified:
                previous=json.loads(args.release_verified.read_text(encoding='utf-8'))
                if previous.get('complete') is not True or previous.get('version')!=version:raise ValueError('Wrong verified trace')
                validate_trace(previous['info'],previous['rows'],[previous['config'][key] for key in CONFIG_FIELDS])
                frozen=parse_info(link.query('@response_info',lambda line:line.startswith('[RESPONSE_INFO]')))
                if frozen!=previous['info']:raise ValueError('Verified trace does not match current frozen generation/CRC')
                answer=link.query('@response_release '+str(frozen['generation']),lambda line:line.startswith('[RESPONSE] RELEASE'))
                if 'RELEASE OK' not in answer:raise ValueError('Release rejected')
            retained=link.query('@response_info',lambda line:line.startswith('[RESPONSE_INFO]'))
            if not re.search(r'\brecording=0\b',retained) or not re.search(r'\bfrozen=0\b',retained):
                raise ValueError('Retained trace must be exported and explicitly released first')
            if args.configure:
                commands=['@startup 0 2000','@servo 1550 1430 1670 -1','@motor 0 0.6 0.3',
                          '@motor_dir 1 1','@balance 1 1','@encoder_sign -1 1']
                for item in commands:
                    link.query(item,lambda line:line.startswith(('[FIELD] OK','[SPEED] OK')))
            link.query(f'@speed_cfg {args.target_ticks} {kp:.9g} {ki:.9g}',lambda line:line.startswith('[SPEED] OK'))
            link.query('@speed_mode '+('1' if args.mode=='closed' else '0'),lambda line:line.startswith('[SPEED] OK'))
            readback=link.query('@encoder_status',lambda line:line.startswith('[SPEED] enabled='))
            match=re.search(r'enabled=(\d+) max_ticks=(\d+) kp=([0-9.]+) ki=([0-9.]+)',readback)
            if not match or int(match[1])!=int(args.mode=='closed') or int(match[2])!=args.target_ticks or abs(float(match[3])-kp_q8/256)>0.0001 or abs(float(match[4])-ki_q8/256)>0.0001:
                raise ValueError('Speed gain/mode readback mismatch; no motion sent')
            if 'faults=0' not in readback:
                raise ValueError('Speed fault present; no motion sent')
            profile=link.query('@status',lambda line:line.startswith('[FIELD] version='))
            if 'hw=1' not in profile or 'cruise=0.0000' not in profile or 'cap=0.6000' not in profile:
                raise ValueError('Unexpected RAM profile; --configure once after boot')
            # Motion authority is checked again in firmware against mode, signs,
            # fresh counts, per-session200/+50 gate and retained-trace state.
            link.pending.clear();link.port.reset_input_buffer()
            link.send(command+'\r\n')  # Exactly one motion command; never retry.
            end=time.monotonic()+(args.duration+350)/1000
            heartbeat=0.0;received=bytearray()
            while time.monotonic()<end:
                now=time.monotonic()
                if now>=heartbeat:link.send('h\r\n');heartbeat=now+.05
                chunk=link.port.read(min(max(link.port.in_waiting,1),4096))
                if chunk:
                    log.write(json.dumps({'direction':'RX','monotonic_ns':time.monotonic_ns(),'hex':chunk.hex()})+'\n');log.flush();received.extend(chunk)
                    if b'[MOTION] REJECT' in received:raise ValueError('Motion rejected; not retried')
            link.send('!s\r\n')
            if b'[MOTION] STARTED' not in received:raise ValueError('Motion acceptance unknown; STOP sent, not retried')
            (args.output/'motion-text.bin').write_bytes(received)
            print('Single action finished; STOP sent. Export frozen response trace before releasing it.')
        finally:
            try:link.send('!s\r\n')
            finally:link.port.close()
    return plan


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port');parser.add_argument('--output',type=Path)
    parser.add_argument('--expected-build',choices=SUPPORTED_VERSIONS,default=VERSION)
    parser.add_argument('--request',type=int,default=200);parser.add_argument('--duration',type=int,default=1000)
    parser.add_argument('--mode',choices=['open','closed'],default='open')
    parser.add_argument('--kind',choices=['line','circle'],default='line')
    parser.add_argument('--servo-us',type=int,default=1550)
    parser.add_argument('--target-ticks',type=int,default=23)
    parser.add_argument('--kp',type=float,default=2.0);parser.add_argument('--ki',type=float,default=0.1)
    parser.add_argument('--configure',action='store_true');parser.add_argument('--release-verified',type=Path)
    parser.add_argument('--execute',action='store_true');parser.add_argument('--bench-ready',action='store_true')
    parser.add_argument('--ground-ready',action='store_true')
    execute(parser.parse_args())


if __name__=='__main__':main()
