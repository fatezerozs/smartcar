"""Exercise the real PowerShell decoder with offline bytes; never open a COM port."""
import csv
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'tools' / 'capture_vision.ps1'
HEADER = bytes.fromhex('AA 49 02 02 5E 00 3C 00 01 00 00 00')


@unittest.skipUnless(shutil.which('pwsh'), 'PowerShell 7 required')
class CaptureTests(unittest.TestCase):
    def setUp(self):
        scratch = ROOT / '.build' / 'capture-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def capture(self, data, *args, success=True):
        source = self.root / 'input.bin'
        source.write_bytes(data)
        output = self.root / 'output'
        result = subprocess.run(
            ['pwsh', '-NoProfile', '-NonInteractive', '-File', str(SCRIPT),
             '-ReplayPath', str(source), '-OutputDirectory', str(output), *args],
            capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=30)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        sessions = list(output.iterdir())
        self.assertEqual(len(sessions), 1)
        folder = sessions[0]
        metadata = json.loads((folder / 'session.json').read_text(encoding='utf-8-sig'))
        self.assertEqual(metadata['source'], 'replay')
        self.assertIsNone(metadata['port'])
        return folder, metadata

    def test_fragmented_header_noise_and_embedded_header_pixels(self):
        pixels = (bytes(range(256)) * 23)[:5640]
        second = HEADER + bytes([37]) * (5640-len(HEADER))
        data = b'X' * 8187 + HEADER + pixels + b'noise' + HEADER + second
        folder, meta = self.capture(data, '-Frames', '0', '-Preview')
        self.assertEqual(meta['status'], 'replay_eof')
        self.assertEqual(meta['frames'], 2)
        self.assertEqual(meta['discarded_bytes'], 8192)
        self.assertEqual(meta['buffered_bytes'], 0)
        self.assertEqual((folder / 'received.bin').read_bytes(), data)
        for index, expected in enumerate((pixels, second), 1):
            stem = f'frame-{index:06}'
            self.assertEqual((folder / (stem + '.gray')).read_bytes(), expected)
            for suffix, dimensions in (('.png', (94, 60)), ('.preview.png', (752, 480))):
                png = (folder / (stem + suffix)).read_bytes()
                self.assertEqual(png[:8], b'\x89PNG\r\n\x1a\n')
                self.assertEqual(struct.unpack('>II', png[16:24]), dimensions)
        with (folder / 'frames.csv').open(encoding='utf-8-sig', newline='') as stream:
            rows = list(csv.DictReader(stream))
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[0]['sequence'], meta['sequence'])
        for row in rows:
            for key in ('state', 'heading', 'curvature', 'open_left', 'open_right', 'split', 'stripes'):
                self.assertEqual(row[key], '')

    def test_default_single_frame_stops_without_preview(self):
        folder, meta = self.capture((HEADER + bytes(5640)) * 2)
        self.assertEqual(meta['status'], 'frame_limit')
        self.assertEqual(meta['frames'], 1)
        self.assertEqual(len(list(folder.glob('*.gray'))), 1)
        self.assertFalse(list(folder.glob('*.preview.png')))

    def test_multiple_packets_completed_in_one_read_and_frame_limit(self):
        payloads = [bytes([value]) * 5640 for value in range(4)]
        data = b'X' * 2600 + b''.join(HEADER + pixels for pixels in payloads)
        folder, meta = self.capture(data, '-Frames', '3')
        self.assertEqual(meta['status'], 'frame_limit')
        self.assertEqual(meta['frames'], 3)
        for index, expected in enumerate(payloads[:3], 1):
            self.assertEqual((folder / f'frame-{index:06}.gray').read_bytes(), expected)
        self.assertFalse((folder / 'frame-000004.gray').exists())

    def test_incomplete_final_frame_is_retained_not_saved(self):
        data = HEADER + bytes([255]) * 5640 + HEADER + bytes(100)
        folder, meta = self.capture(data, '-Frames', '0')
        self.assertEqual(meta['frames'], 1)
        self.assertEqual(meta['buffered_bytes'], 112)
        self.assertEqual((folder / 'received.bin').read_bytes(), data)

    def test_wrong_size_header_is_not_a_frame(self):
        wrong = HEADER[:4] + bytes([95]) + HEADER[5:]
        data = wrong + bytes(5640)
        folder, meta = self.capture(data, '-Frames', '0', success=False)
        self.assertEqual(meta['status'], 'error')
        self.assertEqual(meta['frames'], 0)
        self.assertFalse((folder / 'frames.csv').exists())
        self.assertEqual((folder / 'received.bin').read_bytes(), data)

    def test_empty_recording_reports_error_and_keeps_session(self):
        _, meta = self.capture(b'', '-Frames', '0', success=False)
        self.assertEqual(meta['status'], 'error')
        self.assertEqual(meta['received_bytes'], 0)
        self.assertIn('No complete', meta['error'])


if __name__ == '__main__':
    unittest.main()
