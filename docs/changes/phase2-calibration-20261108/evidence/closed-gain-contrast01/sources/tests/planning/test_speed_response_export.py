"""Offline decoder checks; no serial port is opened."""
from pathlib import Path
import struct
import sys
import unittest
import zlib

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from tools.speed_planning.export_speed_response import FIELDS,parse_info,parse_row,validate_trace


def fixture():
    config=[450,3000,600,23,512,25,0xffffffff,1,0,1550]
    rows=[]
    for i in range(201):
        row=dict.fromkeys(FIELDS,0)
        row.update(ms=i*10,seq=i+1,tl=-i,tr=i,rl=-1,rr=1,
                   interval=10 if i else 0,reason=2)
        rows.append(row)
    info={'generation':7,'count':len(rows),'stop_ms':0}
    update_crc(info,rows,config)
    return info,rows,config


def update_crc(info,rows,config):
    payload=struct.pack('<10I',*config)+b''.join(struct.pack('<22I',*(r[k]&0xffffffff for k in FIELDS)) for r in rows)
    info['crc32']=zlib.crc32(payload)


class ResponseExport(unittest.TestCase):
    def test_canonical_config_and_negative_counts(self):
        info,rows,cfg=fixture();self.assertTrue(validate_trace(info,rows,cfg)['crcVerified'])
        wire='[RESPONSE_ROW] gen=7 i=1 v='+','.join(f'{rows[1][k]&0xffffffff:08X}' for k in FIELDS)
        self.assertEqual(parse_row(wire,7,1),rows[1])

    def test_mutated_config_not_silently_accepted(self):
        info,rows,cfg=fixture();cfg[0]=400
        with self.assertRaisesRegex(ValueError,'CRC'):validate_trace(info,rows,cfg)

    def test_per_run_limit_is_stricter_than_hard_limit(self):
        info,rows,cfg=fixture();cfg[0]=200;rows[10]['pr']=2010;update_crc(info,rows,cfg)
        with self.assertRaisesRegex(ValueError,'run ceiling'):validate_trace(info,rows,cfg)

    def test_hard_limit_and_generation(self):
        info,rows,cfg=fixture();rows[2]['pr']=6001;update_crc(info,rows,cfg)
        with self.assertRaisesRegex(ValueError,'hard ceiling'):validate_trace(info,rows,cfg)
        with self.assertRaisesRegex(ValueError,'generation/index'):parse_row('[RESPONSE_ROW] gen=8 i=1 v=00',7,1)

    def test_gap_and_incomplete_tail(self):
        info,rows,cfg=fixture();rows[10]['seq']+=1;update_crc(info,rows,cfg)
        with self.assertRaisesRegex(ValueError,'Non-consecutive'):validate_trace(info,rows,cfg)
        info,rows,cfg=fixture();info['stop_ms']=100
        with self.assertRaisesRegex(ValueError,'post-zero'):validate_trace(info,rows,cfg)

    def test_control_state_cannot_come_from_future(self):
        info,rows,cfg=fixture();rows[4]['control_seq']=rows[4]['seq']+1;update_crc(info,rows,cfg)
        with self.assertRaisesRegex(ValueError,'newer'):validate_trace(info,rows,cfg)

    def test_admission_seed_is_not_a_hardware_sample_boundary(self):
        info,rows,cfg=fixture();rows[0]['ms']=4;update_crc(info,rows,cfg)
        self.assertTrue(validate_trace(info,rows,cfg)['crcVerified'])
        rows[2]['interval']=9;update_crc(info,rows,cfg)
        with self.assertRaisesRegex(ValueError,'duration'):validate_trace(info,rows,cfg)

    def test_reject_active_export(self):
        with self.assertRaisesRegex(ValueError,'not frozen'):
            parse_info('[RESPONSE_INFO] gen=1 start_ms=10 stop_ms=20 count=201 recording=1 frozen=0 stop_seen=1 overflow=0 reason=2 crc=12345678')

    def test_runner_dry_run_and_missing_readiness_never_open_port(self):
        from argparse import Namespace
        from unittest.mock import patch
        from tools.speed_planning.run_speed_response import execute
        args=Namespace(request=450,duration=1000,target_ticks=23,mode='open',configure=True,
                       execute=False,bench_ready=False,port='COM10',output=None,release_verified=None)
        with patch('tools.speed_planning.run_speed_response.Link',side_effect=AssertionError('port opened')):
            self.assertFalse(execute(args)['execute'])
            args.execute=True
            with self.assertRaisesRegex(ValueError,'bench-ready'):execute(args)

    def test_gain_contrast_is_quantized_and_invalid_values_never_open_port(self):
        from argparse import Namespace
        from unittest.mock import patch
        from tools.speed_planning.run_speed_response import gain_configuration,execute
        args=Namespace(kp=2.0,ki=0.05,request=250,duration=3000,target_ticks=23,mode='closed',
                       configure=False,execute=False,bench_ready=False,port='COM14',output=None,release_verified=None)
        self.assertEqual(gain_configuration(args),(2.0,0.05,512,12))
        with patch('tools.speed_planning.run_speed_response.Link',side_effect=AssertionError('port opened')):
            self.assertEqual(execute(args)['kiQ8'],12)
            for kp,ki in [(float('nan'),.05),(2,float('inf')),(0,.05),(6,.05),(2,.3),(2,-.01),(.0001,.05)]:
                args.kp,args.ki=kp,ki
                with self.assertRaisesRegex(ValueError,'Gains'):execute(args)


if __name__=='__main__':unittest.main(verbosity=2)
