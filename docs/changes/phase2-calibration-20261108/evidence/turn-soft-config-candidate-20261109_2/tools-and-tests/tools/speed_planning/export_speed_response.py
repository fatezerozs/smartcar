"""Read-only frozen response trace exporter; STOP first, never move or release."""
import argparse
import json
from pathlib import Path
import re
import struct
import time
import zlib

from tools.speed_planning.export_stop_trace import Link, parse_info as parse_stop_info

VERSION = 'PHASE2_SPEED_RESPONSE_20261109_1'
HEARTBEAT_VERSION = 'PHASE2_HEARTBEAT_DIAG_20261109_1'
HEARTBEAT_QUIET_VERSION = 'PHASE2_HEARTBEAT_DIAG_20261109_2'
TURN_VERSION = 'PHASE2_TURN_GEOMETRY_20261109_1'
TURN_CONFIG_VERSION = 'PHASE2_TURN_GEOMETRY_20261109_2'
SUPPORTED_VERSIONS = (VERSION, HEARTBEAT_VERSION, HEARTBEAT_QUIET_VERSION, TURN_VERSION, TURN_CONFIG_VERSION)
FIELDS = ('ms','seq','tl','tr','rl','rr','pr','pl','phase','reason','interval',
          'control_seq','control_ms','target_r','target_l','measured_r','measured_l',
          'integral_r','integral_l','faults','detail','enabled')
SIGNED = {'tl','tr','rl','rr','target_r','target_l','measured_r','measured_l','integral_r','integral_l'}
CONFIG_FIELDS=('requestPermille','durationMs','profileCapPermille','maximumTargetTicks10ms',
               'kpQ8','kiQ8','signLeftRawU32','signRightRawU32','closedLoop','servoUs')


def parse_info(line):
    if not line.startswith('[RESPONSE_INFO] '):
        raise ValueError('Wrong response version/info')
    return parse_stop_info(line.replace('[RESPONSE_INFO]', '[STOP_INFO]', 1))


def parse_row(line, generation, index):
    match = re.fullmatch(r'\[RESPONSE_ROW\] gen=(\d+) i=(\d+) v=([0-9A-Fa-f,]+)', line)
    if not match or (int(match[1]),int(match[2])) != (generation,index):
        raise ValueError('Response generation/index mismatch')
    parts = match[3].split(',')
    if len(parts) != len(FIELDS) or any(len(part) != 8 for part in parts):
        raise ValueError('Wrong response payload width')
    row = {}
    for key, part in zip(FIELDS, parts):
        value = int(part,16)
        row[key] = value-0x100000000 if key in SIGNED and value >= 0x80000000 else value
    return validate_row(row)


def validate_row(row):
    if not 0 <= row['pr'] <= 6000 or not 0 <= row['pl'] <= 6000:
        raise ValueError('PWM exceeds600permille hard ceiling')
    for key in ('rl','rr','target_r','target_l','measured_r','measured_l'):
        if not -32768 <= row[key] <= 32767:
            raise ValueError('Invalid int16 field')
    if row['enabled'] not in (0,1) or not 0 <= row['phase'] <= 255 or not 0 <= row['reason'] <= 255:
        raise ValueError('Invalid response state')
    return row


def validate_trace(info, rows, config):
    if len(rows) != info['count']:
        raise ValueError('Missing response rows')
    if len(config)!=10 or not 0<config[0]<=600 or not 100<=config[1]<=3000 or not config[0]<=config[2]<=1000:
        raise ValueError('Invalid frozen request limits')
    if config[6] not in (1,0xffffffff) or config[7] not in (1,0xffffffff) or config[8] not in (0,1):
        raise ValueError('Invalid encoder/mode config')
    if config[8] and not (0<config[3]<=60 and 0<config[4]<=1280 and config[5]<=51 and config[2]<=600):
        raise ValueError('Invalid closed-loop limits')
    payload = struct.pack('<10I',*config)+b''.join(struct.pack('<22I', *(row[key]&0xffffffff for key in FIELDS)) for row in rows)
    if zlib.crc32(payload) != info['crc32']:
        raise ValueError('Response CRC mismatch')
    seed_reset_offset_ms=0
    for i, row in enumerate(rows):
        validate_row(row)
        if row['pr']>min(config[0],config[2],600)*10 or row['pl']>min(config[0],config[2],600)*10:
            raise ValueError('PWM exceeds this run ceiling')
        if i:
            delta = (row['ms']-rows[i-1]['ms'])&0xffffffff
            if (row['seq']-rows[i-1]['seq'])&0xffffffff != 1 or not 0 < delta <= 100:
                raise ValueError('Non-consecutive response samples')
            # Row0 is an admission seed between hardware samples. The first
            # measured interval starts before that seed, so delta can be shorter.
            if i == 1 and not delta <= row['interval'] <= 100:
                raise ValueError('Invalid first sample duration relative to seed')
            if i > 1 and row['interval'] != delta:
                raise ValueError('Sample duration differs from timebase')
        if row['control_seq'] and (row['seq']-row['control_seq'])&0xffffffff >= 0x80000000:
            raise ValueError('Control identity newer than sample')
        if row['control_ms'] and (row['ms']-row['control_ms'])&0xffffffff >= 0x80000000:
            # Admission's now argument can precede its zero-output reset by a
            #1ms timer tick. Only the synthetic zero seed may express that;
            # never accept future PI timestamps in a measured sample row.
            offset=(row['control_ms']-row['ms'])&0xffffffff
            zero_seed=i==0 and row['interval']==0 and row['phase']==1 and all(row[k]==0 for k in
                ('rl','rr','pr','pl','target_r','target_l','measured_r','measured_l','integral_r','integral_l'))
            if not zero_seed or offset!=1:
                raise ValueError('Future control state timestamp')
            seed_reset_offset_ms=offset
    post = (rows[-1]['ms']-info['stop_ms'])&0xffffffff
    if not 2000 <= post <= 2100:
        raise ValueError('Incomplete2s post-zero capture')
    return {'crcVerified':True,'rows':len(rows),'postZeroMs':post,
            'admissionSeedResetOffsetMs':seed_reset_offset_ms,
            'softwarePwmMaxUnits10000':max(max(row['pr'],row['pl']) for row in rows),
            'productionApproved':False,'electricalPwmVerified':False,'mechanicalStopVerified':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='COM10');parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--expected-build',choices=SUPPORTED_VERSIONS,default=VERSION)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    with (args.output/'serial-raw.jsonl').open('x',encoding='utf-8') as log:
        link=Link(args.port,log)
        try:
            link.send('!s\r\n')
            link.wait_line(lambda line:line in ('[LINK] STOP','[FIELD] STOP'))
            link.query('x',lambda line:'port=' in line and 'owner=' in line)
            link.query('?t',lambda line:f'build={args.expected_build};' in line)
            info=None
            for _ in range(6):
                line=link.query('@response_info',lambda line:line.startswith('[RESPONSE_INFO]'))
                try:info=parse_info(line);break
                except ValueError:
                    # A just-issued STOP may still be collecting its2s tail.
                    if 'recording=1' not in line:raise
                    time.sleep(.5)
            if info is None:raise ValueError('Response trace not frozen')
            line=link.query('@response_config',lambda line:line.startswith('[RESPONSE_CONFIG]'))
            match=re.fullmatch(r'\[RESPONSE_CONFIG\] gen=(\d+) v=([0-9A-Fa-f,]+)',line)
            if not match or int(match[1])!=info['generation']:raise ValueError('Config generation mismatch')
            parts=match[2].split(',')
            if len(parts)!=10 or any(len(part)!=8 for part in parts):raise ValueError('Config payload width mismatch')
            config=[int(part,16) for part in parts]
            rows=[]
            for index in range(info['count']):
                line=link.query(f'@response_row {index}',lambda line:line.startswith('[RESPONSE_ROW]'))
                rows.append(parse_row(line,info['generation'],index))
            check=validate_trace(info,rows,config)
            final=parse_info(link.query('@response_info',lambda line:line.startswith('[RESPONSE_INFO]')))
            if final!=info:raise ValueError('Frozen response metadata changed')
            result={'schemaVersion':'smartcar.speed-response-trace/1','version':args.expected_build,'complete':True,
                    'info':info,'config':dict(zip(CONFIG_FIELDS,config)),'rows':rows,'verification':check,'workingCountsPerMetre':{'left':11394,'right':11389},
                    'limits':['PWM fields are software driver submissions, not electrical measurements.',
                              'Row0 is a seed; subsequent sample rows precede foreground PI for that sample.',
                              'control_seq identifies the held PI state; compare identities before pairing feedback.',
                              '450permille is a request level, not a calibrated speed; no automatic release.']}
            if args.expected_build in (HEARTBEAT_VERSION,HEARTBEAT_QUIET_VERSION,TURN_VERSION,TURN_CONFIG_VERSION):
                result['heartbeatDiagnostic']={'rawReply':link.query('@heartbeat_diag',lambda line:line.startswith('[HB_DIAG]')),
                                               'coveredByTraceCrc':False,'instrumentedBuild':True}
            if args.expected_build==TURN_CONFIG_VERSION:
                result['turnConfiguration']={'rawReply':link.query('@turn_frozen',lambda line:line.startswith('[TURN_CONFIG]')),
                                             'coveredByTraceCrc':False}
            (args.output/'trace.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
            print(json.dumps(check))
        finally:link.port.close()


if __name__=='__main__':main()
