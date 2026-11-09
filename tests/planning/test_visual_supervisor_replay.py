"""Observed visual trace + native C supervisor; no serial or firmware access."""
import csv
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.speed_planning.replay_visual_supervisor import recovery_episodes, run

SESSION = ROOT/'docs/changes/phase2-calibration-20261108/evidence/visual-continuous/session04'


class VisualSupervisorReplay(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.folder = Path(cls.temp.name)
        family = os.environ.get('SMARTCAR_TEST_COMPILER', 'gcc')
        compiler = os.environ.get('SMARTCAR_TEST_COMPILER_PATH') or shutil.which('cl' if family == 'msvc' else 'gcc')
        if not compiler:
            raise RuntimeError('Existing native compiler required for supervisor replay')
        cls.report = run(SESSION, cls.folder/'native', compiler, arm_index=103)
        cls.cases = {case['case']: case for case in cls.report['experiments']}

    def test_real_metadata_matches_independent_oracle_without_motion_claim(self):
        self.assertEqual(self.report['sourceFrames'], 750)
        self.assertEqual(len(self.cases), 5)
        self.assertTrue(all(case['allRowsMatchIndependentOracle'] for case in self.cases.values()))
        self.assertFalse(self.report['productionApproved'])
        self.assertFalse(self.report['hardwareAccessed'])
        self.assertEqual(self.report['syntheticInputs']['odometerM'], 0)

    def test_denied_admission_does_not_auto_arm_on_later_valid_frames(self):
        case = self.cases['initial-admission-only']
        self.assertFalse(case['armAccepted'])
        self.assertEqual([t['mode'] for t in case['transitions']], ['DISARMED'])

    def test_time_expiry_latches_and_later_recovery_cannot_unlock(self):
        case = self.cases['reference-time-budget']
        self.assertEqual(case['farBudgetMs'], 600)
        self.assertEqual(case['finalMode'], 'STOP_LATCHED')
        with (self.folder/'native/reference-time-budget.csv').open(newline='') as file:
            rows = list(csv.DictReader(file))
        stop = next(i for i, row in enumerate(rows) if row['mode'] == '4')
        manifest = json.loads((SESSION/'session.json').read_text(encoding='utf-8'))
        data = manifest['frames']
        age = (data[stop]['observe_end_ms']-int(rows[stop]['farStartedMs'])) & 0xffffffff
        self.assertGreaterEqual(age, 600)
        self.assertLess(age, 620)
        latched = next(i for i, row in enumerate(rows) if row['mode'] == '5')
        self.assertTrue(all(r['mode'] == '5' and r['authorized'] == '0' and r['inhibit'] == '1'
                            for r in rows[latched:]))
        self.assertEqual(data[-1]['control_far_valid'], 1)  # Actual later recovery was present.

    def test_four_valid_frames_can_clear_three_confirmations_but_not_five(self):
        three = self.cases['diagnostic-recovery-3']['transitions']
        five = self.cases['diagnostic-recovery-5']['transitions']
        self.assertTrue(any(t['mode'] == 'NORMAL_TRACK' and 289 <= t['index'] <= 292 for t in three))
        self.assertFalse(any(t['mode'] == 'NORMAL_TRACK' and 274 < t['index'] < 528 for t in five))
        self.assertEqual(self.cases['diagnostic-recovery-5']['farBudgetMs'], 10000)

    def test_source_corruption_rejected_before_building(self):
        bad = self.folder/'tampered'
        bad.mkdir()
        shutil.copy2(SESSION/'session.json', bad/'session.json')
        raw = bytearray((SESSION/'frame-0000.vqf').read_bytes())
        raw[-1] ^= 1
        (bad/'frame-0000.vqf').write_bytes(raw)
        output = self.folder/'must-not-exist'
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            run(bad, output)
        self.assertFalse(output.exists())

    def test_censored_observation_episode_does_not_invent_distance(self):
        rows = [dict(control_valid=1, control_far_valid=far, capture_ms=i*20)
                for i, far in enumerate([0, 1, 0])]
        episodes = recovery_episodes(rows, 2)
        self.assertEqual(len(episodes), 1)
        self.assertTrue(episodes[0]['leftCensored'])
        self.assertTrue(episodes[0]['rightCensored'])
        self.assertNotIn('distanceM', episodes[0])


if __name__ == '__main__':
    unittest.main(verbosity=2)
