import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('run_analysis', Path(__file__).resolve().parents[2] / 'tools/analyze_run_log.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PassiveTraceTests(unittest.TestCase):
    def test_complete_and_values(self):
        text = ('[TRACE_BEGIN] run=3 count=2 dt_ms=60 scale=1000\r\n'
                '[TRACE] run=3 i=0 ms=100 e=-1998 u=-1000 v=1 raw=-11/13 junction=36 anchors=8 ring=2 ring_side=-1 ring_rows=41\r\n'
                '[TRACE] run=3 i=1 ms=160 e=0 u=0 v=0\r\n'
                '[TRACE_END] run=3 count=2\r\n')
        trace = module.analyze_text(text)['traces'][0]
        self.assertTrue(trace['complete'])
        self.assertEqual(trace['duration_ms'], 60)
        self.assertEqual(trace['max_abs_lateral'], 1.998)
        self.assertEqual(trace['full_steer_records'], 1)
        self.assertEqual(trace['records'][0]['junction'], 36)
        self.assertEqual(trace['records'][0]['anchors'], 8)
        self.assertEqual(trace['records'][0]['ring'], 2)
        self.assertEqual(trace['records'][0]['ring_side'], -1)
        self.assertEqual(trace['records'][0]['ring_rows'], 41)

    def test_loss_is_not_complete(self):
        for indices in ((0, 2), (0, 0, 1)):
            text = '[TRACE_BEGIN] run=1 count=3 scale=1000\n'
            text += ''.join(f'[TRACE] run=1 i={i} ms={i*60} e=0\n' for i in indices)
            text += '[TRACE_END] run=1 count=3\n'
            self.assertFalse(module.analyze_text(text)['traces'][0]['complete'])

    def test_reboot_or_interrupted_dump_kept_separately(self):
        text = ('[TRACE_BEGIN] run=1 count=1 scale=1000\n'
                '[TRACE] run=1 i=0 ms=1 e=0\n'
                '[TRACE_BEGIN] run=1 count=1 scale=1000\n'
                '[TRACE] run=1 i=0 ms=2 e=0\n[TRACE_END] run=1 count=1\n')
        traces = module.analyze_text(text)['traces']
        self.assertEqual(len(traces), 2)
        self.assertFalse(traces[0]['complete'])
        self.assertTrue(traces[1]['complete'])


if __name__ == '__main__':
    unittest.main()
