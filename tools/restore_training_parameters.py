# Recorded 2026-10-21 vehicle baseline, not a universal hardware profile.
# Only STOP, ownership/help and fixed idempotent configuration/query commands are sent.
# No arm/heartbeat/probe/save command; camera confirmation and cruise remain zero.
import argparse, sys, time, re
from pathlib import Path
from datetime import datetime

try:
    import serial
except ImportError:
    raise SystemExit('Install pyserial first: python -m pip install pyserial')
parser=argparse.ArgumentParser(description='Restore the recorded training baseline while stopped; no camera calibration or motion.')
parser.add_argument('--port', required=True, help='Actual USB serial port, for example COM9')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
logpath=root/'.build'/'field'/('restore-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f')+'.log')
logpath.parent.mkdir(parents=True,exist_ok=True)
port=serial.Serial()
port.port=args.port; port.baudrate=115200; port.timeout=.05; port.write_timeout=1
port.dtr=False;port.rts=False
with logpath.open('x',encoding='utf-8') as log:
    def recv(seconds):
        end=time.monotonic()+seconds; data=bytearray()
        while time.monotonic()<end:
            chunk=port.read(max(1,port.in_waiting))
            if chunk: data.extend(chunk)
        result=data.decode('utf-8',errors='replace')
        log.write(result);log.flush()
        return result
    def tx(text):
        log.write('\n# TX '+text+'\n');log.flush()
        port.write((text+'\r\n').encode('ascii'));port.flush()
    def claim():
        tx('x'); reply=recv(.4)
        if not re.search(r'\[LINK\] port=usb owner=usb\b',reply):
            raise RuntimeError('USB ownership was not confirmed')
    def command(text,pattern):
        for attempt in range(2):
            claim();tx(text);reply=recv(.7)
            if '[FIELD] REJECT' in reply or '[SPEED] REJECT' in reply:
                raise RuntimeError('Firmware rejected '+text)
            if re.search(pattern,reply):
                print('OK '+text,flush=True)
                return reply
            if attempt==0:
                tx('!s');recv(.2)
        raise RuntimeError('No verified acknowledgement for '+text)
    try:
        port.open();tx('!s');recv(.3);claim();tx('?t');identity=recv(1.1)
        if '[FIELD] build=MIDTERM_TRAIN_1;' not in identity:
            raise RuntimeError('Training firmware identity not confirmed')
        commands=['@startup 0 2000','@bands 0.78 0.62 0.035',
                  '@servo 1550 1500 1600 -1','@motor_dir 1 1','@balance 1 1',
                  '@motor 0 0.15 0.1','@duration 5000','@encoder_sign -1 1',
                  '@speed_cfg 40 1 0.05','@speed_mode 1']
        for text in commands:
            pattern=r'\[SPEED\] OK;' if text.startswith(('@encoder_sign','@speed_cfg','@speed_mode')) else r'\[FIELD\] OK;'
            command(text,pattern)
        speed=command('@speed_status',r'\[SPEED\] enabled=')
        status=command('@status',r'\[FIELD\] version=MIDTERM_TRAIN_1 ')
        for pattern in [r'\[SPEED\] enabled=1 max_ticks=40 kp=1\.0000 ki=0\.0469',r'sign3=-1 sign4=1 confirmed=1',r'pwm_R/L=0/0 faults=0']:
            if not re.search(pattern,speed):raise RuntimeError('Unexpected speed readback: '+pattern)
        for token in ['cam=0 hw=1 servo=1550/1500/1600 sign=-1','cruise=0.0000 cap=0.1500','near=0.7800 far=0.6200 half=0.0350','confirmed=1 dir=1/1 scale=1.0000/1.0000','enabled=0 delay_ms=2000']:
            if token not in status:raise RuntimeError('Unexpected vehicle readback: '+token)
        for line in (speed+'\n'+status).splitlines():
            if line.startswith(('[FIELD] version=','[BANDS]','[MOTOR]','[STARTUP]','[ENCODER]','[SPEED] enabled=')):
                print(line,flush=True)
        print('RESTORE VERIFIED; STOPPED; CAMERA UNCALIBRATED; CRUISE=0; RAM ONLY',flush=True)
    finally:
        if port.is_open:
            try:
                tx('!s');recv(.2)
            finally:
                port.close()
        print('LOG='+str(logpath),flush=True)
