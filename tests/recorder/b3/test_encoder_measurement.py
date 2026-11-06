"""Offline acceptance guards for operator-counted Encoder measurements."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'tools'))
from measure_b3_encoder import compare


class EncoderComparison(unittest.TestCase):
    def setUp(self):
        self.before = {'source': 'same', 'last_ms': 100, 'sample_sequence': 10,
                       'total_left_right': [0, 0], 'raw_left_right': [2, 65530]}
        self.after = {'source': 'same', 'first_ms': 200, 'sample_sequence': 20,
                      'total_left_right': [-3072, 0], 'raw_left_right': [62466, 65530]}

    def test_forward_left_and_wrap(self):
        r = compare(self.before, self.after, 'left', 'forward', 3)
        self.assertEqual((r['effective_wheel_cpr'], r['forward_raw_sign']), (1024, -1))
        self.assertFalse(r['accepted_final_calibration'])

    def test_reverse_left(self):
        a = {**self.after, 'total_left_right': [3072, 0], 'raw_left_right': [3074, 65530]}
        self.assertEqual(compare(self.before, a, 'left', 'reverse', 3)['forward_raw_sign'], -1)

    def test_forward_right(self):
        a = {**self.after, 'total_left_right': [0, 3072], 'raw_left_right': [2, 3066]}
        self.assertEqual(compare(self.before, a, 'right', 'forward', 3)['forward_raw_sign'], 1)

    def test_invalid_trials(self):
        for change in [{'source': 'changed'}, {'first_ms': 50}, {'sample_sequence': 1},
                       {'raw_left_right': [2, 65530]},
                       {'total_left_right': [-3072, 1], 'raw_left_right': [62466, 65531]},
                       {'total_left_right': [0, 0], 'raw_left_right': [2, 65530]}]:
            with self.subTest(change=change), self.assertRaises(ValueError):
                compare(self.before, {**self.after, **change}, 'left', 'forward', 3)
        for turns in (0, -1, 101):
            with self.assertRaises(ValueError):
                compare(self.before, self.after, 'left', 'forward', turns)


if __name__ == '__main__':
    unittest.main()
