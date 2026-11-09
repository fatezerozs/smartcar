"""Offline regression over frozen measured traces; no hardware or tuning."""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools.speed_planning.analyze_speed_response import analyze,negative_runs
from tools.speed_planning.export_speed_response import CONFIG_FIELDS,FIELDS

BASE=ROOT/'docs/changes/phase2-calibration-20261108/evidence'


def source(case):
    return BASE/f'speed-response-{case}'/f'export-{case}'/'trace.json'


class ResponseAnalysis(unittest.TestCase):
    def test_all_observed_nonzero_target_feedback_pairs_match_raw_counts(self):
        expected={'closed200-01':(146,0),'closed250-01':(80,1),'closed250-02':(145,1)}
        for case,(pairs,excluded) in expected.items():
            with self.subTest(case=case):
                report,matched=analyze(source(case))
                self.assertEqual(len(matched),pairs)
                self.assertEqual(len(report['excludedZeroTargetPairingAnomalies']),excluded)
                self.assertTrue(all(r['target_l']>0 and r['target_r']>0 for r in matched))
                self.assertFalse(report['productionApproved'])

    def test_stall_saturation_is_not_target_tracking(self):
        report,_=analyze(source('closed200-01'))
        self.assertEqual(report['outputCeiling']['r']['sampledCeilingDurationMs'],460)
        self.assertIsNone(report['responseOnset']['r']['firstSampleElapsedMs'])
        self.assertEqual(report['tailWindowsMs']['300']['r']['samplesWithin2Counts'],0)

    def test_reverse_fault_is_actual_sample_run_not_stale_reset_feedback(self):
        report,_=analyze(source('closed250-01'))
        runs=report['driveNegativeRuns']['r']
        self.assertEqual([(r['samples'],r['minimumNormalizedCounts10ms']) for r in runs],[(3,-2)])
        self.assertIn(8,report['faultDetailValues'])
        self.assertEqual(report['tailWindowsMs']['300']['uniqueControlSamples'],0)

    def test_no_drive_reverse_in_successful_repeat_and_seed_excluded(self):
        report,_=analyze(source('closed250-02'))
        self.assertEqual(report['driveNegativeRuns']['r'],[])
        self.assertEqual(report['faultDetailValues'],[0])
        self.assertEqual(report['responseOnset']['r']['firstSampleElapsedMs'],1130)
        self.assertEqual(report['responseOnset']['l']['firstSampleElapsedMs'],1200)
        self.assertEqual(report['tailWindowsMs']['300']['uniqueControlSamples'],29)
        self.assertGreater(report['tailWindowsMs']['300']['l']['maxOvershootCounts'],0)

    def test_crc_valid_but_wrong_nonzero_feedback_is_rejected(self):
        trace=json.loads(source('closed250-02').read_text(encoding='utf-8'))
        row=next(r for r in trace['rows'] if r['phase']==2 and r['target_l']>0)
        row['measured_l']+=1
        cfg=[trace['config'][key] for key in CONFIG_FIELDS]
        payload=struct.pack('<10I',*cfg)+b''.join(struct.pack('<22I',*(r[k]&0xffffffff for k in FIELDS)) for r in trace['rows'])
        trace['info']['crc32']=zlib.crc32(payload)
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'trace.json';path.write_text(json.dumps(trace),encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'Nonzero-target feedback/source mismatch'):analyze(path)


if __name__=='__main__':unittest.main(verbosity=2)
