"""Actual frozen counter traces, explicit candidate scales, native C conversion."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.speed_planning.replay_metric_odom import run
TRACES = [ROOT/f'docs/changes/phase2-calibration-20261108/evidence/stop-trace/{name}/trace.json'
          for name in ('lower01', 'video02', 'video03')]


class MetricReplay(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory();cls.addClassCleanup(cls.temp.cleanup)
        cls.folder = Path(cls.temp.name)
        cls.report = run(TRACES, cls.folder/'native', 11394, 11389)

    def test_counts_times_and_midpoint_history_match_independent_oracle(self):
        self.assertEqual(len(self.report['experiments']), 3)
        self.assertTrue(all(c['rawTraceCrcVerified'] and c['allRowsMatchIndependentOracle']
                            for c in self.report['experiments']))
        self.assertFalse(self.report['productionApproved']);self.assertFalse(self.report['hardwareAccessed'])

    def test_counter_tamper_rejected_before_artifacts(self):
        bad = self.folder/'bad.json';data = json.loads(TRACES[0].read_text('utf-8'))
        data['rows'][3]['tl'] += 1;bad.write_text(json.dumps(data), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'CRC32'):
            run([bad], self.folder/'not-created', 11394, 11389)
        self.assertFalse((self.folder/'not-created').exists())

    def test_unknown_scale_not_guessed(self):
        with self.assertRaises(ValueError):
            run(TRACES, self.folder/'unknown', 0, 11389)
        self.assertFalse((self.folder/'unknown').exists())


if __name__ == '__main__':
    unittest.main(verbosity=2)
