import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    'training_analysis', Path(__file__).resolve().parents[2] / 'tools/analyze_training_run.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

COMPACT = ('[RUN] n=7 ms=100 age=2 process_max=4 gap_max=20 enc_age=3 '
           'e=-200 h=100 u=-300 duty=145 target=90/80 measured=89/81 pwm=220/240 '
           'enabled=1 faults=0 q=85 both=24 inferred=63 paired=1000 edge=1000 '
           'ring=1 side=-1 guide=20 bayes=6 accepted=0 pwm_hz_cfg=17000 scale=1000\n')
LEGACY = ('[RUN] n=7 ms=100 e=-0.2 h=0.1 u=-0.3 pwm=0.145 q=85 age=2\n'
          '[SPEED] ms=100 sample=4 enabled=1 raw3_L=-81 raw4_R=89 '
          'target_R/L=90/80 measured_R/L=89/81 pwm_R/L=220/240 faults=0\n')


class TrainingAnalysisTests(unittest.TestCase):
    def test_legacy_and_compact_units_match(self):
        self.assertEqual(module.summarize(COMPACT), module.summarize(LEGACY))
        row = module.summarize(COMPACT)[0]
        self.assertEqual(row['near_error_min_max'], [-0.2, -0.2])
        self.assertEqual(row['peak_output_R_L_permille'], [220, 240])
        self.assertEqual(row['last_target_R_L'], [90, 80])
        self.assertEqual(row['first_nonzero_feedback_delay_ms'], 0)
        self.assertEqual(module.read_samples(COMPACT)[1][0][2:4], [None, None])

    def test_stop_separates_protocol_versions(self):
        rows = module.summarize(LEGACY + '[LINK] STOP\n' + COMPACT)
        self.assertEqual(len(rows), 2)
        self.assertTrue(rows[0]['stop_ack_seen'])
        self.assertFalse(rows[1]['stop_ack_seen'])

    def test_damaged_packets_are_not_reconstructed(self):
        self.assertEqual(module.summarize(COMPACT.replace('measured=89/81', 'measured=89/')), [])
        self.assertEqual(module.summarize(COMPACT.replace('duty=145 ', '')), [])


if __name__ == '__main__':
    unittest.main()
