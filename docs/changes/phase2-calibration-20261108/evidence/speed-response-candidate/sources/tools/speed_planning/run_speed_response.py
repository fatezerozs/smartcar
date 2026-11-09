"""Single lifted-bench response action, dry-run by default. No flash or retries."""
import argparse
import json
from pathlib import Path
import re
import time

from tools.speed_planning.export_stop_trace import Link
from tools.speed_planning.export_speed_response import VERSION,CONFIG_FIELDS,validate_trace,parse_info


def execute(args):
    if not 1<=args.request<=600 or not 100<=args.duration<=3000:
        raise ValueError('Request1..600permille, duration100..3000ms')
    if not 1<=args.target_ticks<=60:
        raise ValueError('Closed-loop full-ramp target1..60counts/10ms')
    command=f'@motion line {args.request} {args.duration}'
    plan={'version':VERSION,'command':command,'mode':args.mode,'targetTicks10ms':args.target_ticks if args.mode=='closed' else None,
          'closedLoopFeedforward':0,'normalStraightRequestPermille':450,'hardCapPermille':600,
          'configureRam':args.configure,'execute':args.execute,'scope':'One lifted-bench action; hardware motion is not run by default.'}
    if not args.execute:
        print(json.dumps(plan));return plan
    if not args.bench_ready or not args.port or not args.output:
        raise ValueError('Execution requires --bench-ready, --port and a new --output directory')
    args.output.mkdir(parents=True,exist_ok=False)
    (args.output/'plan.json').write_text(json.dumps(plan,indent=2)+'\n',encoding='utf-8')
    with (args.output/'serial-raw.jsonl').open('x',encoding='utf-8') as log:
        link=Link(args.port,log)
        try:
            link.send('!s\r\n')
            claim=link.query('x',lambda line:line.startswith('[LINK] port='))
            if not re.search(r'port=(usb|radio) owner=\1\b',claim):raise ValueError('Ownership not confirmed')
            link.query('?t',lambda line:f'build={VERSION};' in line)
            if args.release_verified:
                previous=json.loads(args.release_verified.read_text(encoding='utf-8'))
                if previous.get('complete') is not True or previous.get('version')!=VERSION:raise ValueError('Wrong verified trace')
                validate_trace(previous['info'],previous['rows'],[previous['config'][key] for key in CONFIG_FIELDS])
                frozen=parse_info(link.query('@response_info',lambda line:line.startswith('[RESPONSE_INFO]')))
                if frozen!=previous['info']:raise ValueError('Verified trace does not match current frozen generation/CRC')
                answer=link.query('@response_release '+str(frozen['generation']),lambda line:line.startswith('[RESPONSE] RELEASE'))
                if 'RELEASE OK' not in answer:raise ValueError('Release rejected')
            retained=link.query('@response_info',lambda line:line.startswith('[RESPONSE_INFO]'))
            if not re.search(r'\bcount=0\b',retained) or 'recording=1' in retained:
                raise ValueError('Retained trace must be exported and explicitly released first')
            if args.configure:
                commands=['@startup 0 2000','@servo 1550 1430 1670 -1','@motor 0 0.6 0.3',
                          '@motor_dir 1 1','@balance 1 1','@encoder_sign -1 1']
                for item in commands:
                    link.query(item,lambda line:line.startswith(('[FIELD] OK','[SPEED] OK')))
            link.query(f'@speed_cfg {args.target_ticks} 2 0.1',lambda line:line.startswith('[SPEED] OK'))
            link.query('@speed_mode '+('1' if args.mode=='closed' else '0'),lambda line:line.startswith('[SPEED] OK'))
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
    parser.add_argument('--request',type=int,default=200);parser.add_argument('--duration',type=int,default=1000)
    parser.add_argument('--mode',choices=['open','closed'],default='open')
    parser.add_argument('--target-ticks',type=int,default=23)
    parser.add_argument('--configure',action='store_true');parser.add_argument('--release-verified',type=Path)
    parser.add_argument('--execute',action='store_true');parser.add_argument('--bench-ready',action='store_true')
    execute(parser.parse_args())


if __name__=='__main__':main()
