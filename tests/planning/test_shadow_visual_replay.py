"""Hybrid visual metadata regression for the exact portable target shadow code."""
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.speed_planning.replay_shadow_visual import run
SESSION = ROOT/'docs/changes/phase2-calibration-20261108/evidence/visual-continuous/session04'


class ShadowReplay(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(); cls.addClassCleanup(cls.temp.cleanup)
        cls.folder = Path(cls.temp.name)
        cls.report = run(SESSION, cls.folder/'replay')

    def test_all_rows_and_explicit_synthetic_limits(self):
        self.assertEqual(sum(c['rows'] for c in self.report['experiments']), 3750)
        self.assertTrue(all(c['supervisorMatchesIndependentOracle'] for c in self.report['experiments']))
        self.assertFalse(self.report['productionApproved'])
        self.assertFalse(self.report['hardwareAccessed'])
        self.assertFalse(self.report['runtimeConnected'])
        self.assertEqual(self.report['syntheticInputs']['referenceSpeedMps'], 0)

    def test_stop_and_disarmed_candidates(self):
        cases = {c['case']: c for c in self.report['experiments']}
        self.assertEqual(cases['initial-admission-only']['readyCandidates'], 0)
        self.assertGreater(cases['reference-time-budget']['controlledStopCandidates'], 0)
        self.assertGreater(cases['reference-time-budget']['inhibitedRows'], 0)

    def test_tamper_rejected_without_artifact_creation(self):
        bad = self.folder/'bad'; bad.mkdir()
        shutil.copyfile(SESSION/'session.json', bad/'session.json')
        data = bytearray((SESSION/'frame-0000.vqf').read_bytes()); data[-1] ^= 1
        (bad/'frame-0000.vqf').write_bytes(data)
        out = self.folder/'not-created'
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            run(bad, out)
        self.assertFalse(out.exists())


if __name__ == '__main__':
    unittest.main(verbosity=2)
