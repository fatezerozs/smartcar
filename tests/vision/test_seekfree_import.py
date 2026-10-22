import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[2] / 'tools'
sys.path.insert(0, str(TOOLS))
spec = importlib.util.spec_from_file_location('seekfree_import', TOOLS / 'import_seekfree_log.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ImportTests(unittest.TestCase):
    def test_compact_run_units_and_embedded_speed(self):
        rows = module.split_segments(
            '[RUN] n=1 ms=100 e=-200 h=100 u=-300 duty=145 target=90/80 '
            'measured=89/81 pwm=220/240 enabled=1 faults=0 q=85 scale=1000\n')
        self.assertEqual(rows[0]['lateral_min_max'], [-0.2, -0.2])
        self.assertEqual(rows[0]['steering_min_max'], [-0.3, -0.3])
        self.assertEqual(rows[0]['speed_sample_count'], 1)
        self.assertEqual(rows[0]['speed_rows'][0]['pwm_R/L'], '220/240')
        self.assertEqual(rows[0]['run_rows'][0]['e'], -200)  # Source fields stay raw.

    def test_multiple_runs_timestamp_prefix_and_repeated_retained_stop(self):
        text = ('[12:00:01.000] [RUN] n=1 ms=1000 e=-0.2 h=0 u=0.3 pwm=0.145\n'
                '[SPEED] ms=1001 raw3_L=-15 raw4_R=16 pwm_R/L=190/200\n'
                '[RUN] n=2 ms=1100 e=0.1 h=0 u=0 pwm=0.145\n'
                '[LAST_STOP] seq=1 ms=1200 reason=1\n'
                '[LAST_STOP] seq=1 ms=1200 reason=1\n'
                '[RUN] n=3 ms=3000 e=0 h=0 u=0 pwm=0\n'
                '[LAST_STOP] seq=1 ms=1200 reason=1\n'
                '[DRIVE] state=0 faults=0\n')
        rows = module.split_segments(text)
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[0]['run_sample_count'], 2)
        self.assertEqual(rows[0]['speed_sample_count'], 1)
        self.assertEqual(rows[0]['run_rows'][0]['host_prefix'], '[12:00:01.000]')
        self.assertEqual(rows[0]['lateral_min_max'], [-0.2, 0.1])
        self.assertEqual(rows[1]['end_evidence'], 'drive_state_stopped_or_fault')
        self.assertFalse(rows[0]['physical_stop_confirmed'])
        self.assertFalse(rows[0]['whole_run_guaranteed'])

    def test_first_retained_stop_older_than_current_start(self):
        rows = module.split_segments('[RUN] ms=5000\n[LAST_STOP] seq=4 ms=1000\n[RUN] ms=5100\n')
        self.assertEqual(len(rows), 1)
        self.assertFalse(rows[0]['stop_observed'])

    def test_gaps_are_uncertain_and_capture_end_not_stop(self):
        rows = module.split_segments('[RUN] ms=1\n[RUN] ms=3000\n')
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[0]['end_evidence'], 'telemetry_gap_not_proven_stop')
        self.assertEqual(rows[1]['end_evidence'], 'capture_ended_without_stop')
        self.assertFalse(rows[0]['stop_observed'])

    def test_wrap_is_not_reset(self):
        rows = module.split_segments('[RUN] ms=4294967290\n[RUN] ms=20\n[LAST_STOP] seq=1 ms=40\n')
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['observed_run_span_ms'], 26)
        self.assertTrue(rows[0]['stop_observed'])

    def test_clock_reset_and_boot_groups(self):
        rows = module.split_segments('[RUN] ms=10000\n[RUN] ms=20\n[BOOT] stage=PWM_OFF\n[BOOT] stage=UART_READY\n[RUN] ms=200\n')
        self.assertEqual(len(rows), 3)
        self.assertEqual(rows[0]['end_evidence'], 'clock_reset_or_out_of_order')
        self.assertEqual(rows[1]['end_evidence'], 'firmware_reboot')
        self.assertEqual(rows[2]['boot_epoch'], 2)

    def test_reboot_detected_while_stopped_without_boot_banner(self):
        rows = module.split_segments('[SPEED] ms=10000\n[RUN] ms=10100\n'
                                     '[LAST_STOP] seq=1 ms=10200\n[SPEED] ms=500\n'
                                     '[RUN] ms=10100\n[LAST_STOP] seq=1 ms=10200\n')
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[1]['boot_epoch'], rows[0]['boot_epoch'] + 1)
        self.assertTrue(rows[1]['stop_observed'])

    def test_empty_stops_do_not_create_runs_and_no_stop_does_not_disappear(self):
        self.assertEqual(module.split_segments('[LAST_STOP] seq=0 ms=0\n[DRIVE] state=0\n'), [])
        self.assertEqual(len(module.split_segments('[RUN] ms=1')), 1)

    def test_import_keeps_bytes_and_trace_separate_and_never_overwrites(self):
        raw = ('[RUN] ms=1 e=0 u=0\r\n[LAST_STOP] seq=1 ms=100\r\n'
               '[TRACE_BEGIN] run=7 count=2 scale=1000\r\n'
               '[TRACE] run=7 i=0 ms=1 e=200 u=-300 v=1\r\n'
               '[TRACE] run=7 i=1 ms=100 e=0 u=0 v=1\r\n'
               '[TRACE_END] run=7 count=2\r\n').encode()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / '原始日志.txt'
            source.write_bytes(raw)
            output, result = module.import_log(source, root / '保存')
            self.assertEqual((output / 'source-original.bin').read_bytes(), raw)
            normalized = (output / 'session.log').read_bytes()
            self.assertEqual(normalized, raw)
            self.assertEqual(len(normalized.decode().splitlines()), len(raw.decode().splitlines()))
            self.assertEqual(result['sha256'], hashlib.sha256(raw).hexdigest())
            self.assertEqual(result['complete_trace_exports'], 1)
            self.assertEqual(len(result['segments']), 1)
            self.assertNotIn('[TRACE]', (output / 'segment-001/serial.log').read_text(encoding='utf-8'))
            self.assertTrue((output / 'segment-001/run.csv').exists())
            self.assertTrue((output / 'traces/trace-001-run-7.csv').exists())
            self.assertEqual(json.loads((output / 'summary.json').read_text(encoding='utf-8'))['source_bytes'], len(raw))
            second, _ = module.import_log(source, root / '保存')
            self.assertNotEqual(output, second)
            self.assertEqual(source.read_bytes(), raw)

    def test_truncated_trace_is_not_complete(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'truncated.log'
            source.write_text('[TRACE_BEGIN] run=1 count=2\n[TRACE] run=1 i=0 ms=1\n')
            _, result = module.import_log(source, Path(directory) / 'out')
            self.assertEqual(result['complete_trace_exports'], 0)
            self.assertEqual(result['segments'], [])

    def test_encodings_and_binary_rejection(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'serial.txt'
            for encoding in ('utf-8-sig', 'utf-16', 'gb18030'):
                source.write_bytes('接收：[RUN] ms=100 e=0\n'.encode(encoding))
                _, result = module.import_log(source, Path(directory) / 'out')
                self.assertEqual(len(result['segments']), 1)
            source.write_bytes(b'\x00\x00[RUN] ms=1')
            with self.assertRaises(ValueError):
                module.import_log(source, Path(directory) / 'out')


if __name__ == '__main__':
    unittest.main()
