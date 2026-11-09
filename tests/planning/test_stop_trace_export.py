"""Pure parsing/integrity tests; no serial access or vehicle commands."""
import copy
import struct
import sys
from pathlib import Path
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.speed_planning.export_stop_trace import FIELDS, parse_info, parse_row, validate_trace


class StopTraceExportTests(unittest.TestCase):
    def fixture(self):
        rows = [dict(zip(FIELDS, [i * 10, i, -i, i, -1, 1, 0, 0, 0, 1])) for i in range(201)]
        data = b"".join(struct.pack("<10I", *(r[k] & 0xFFFFFFFF for k in FIELDS)) for r in rows)
        info = parse_info(f"[STOP_INFO] gen=1 start_ms=0 stop_ms=0 count=201 recording=0 frozen=1 stop_seen=1 overflow=0 reason=1 crc={zlib.crc32(data):08X}")
        return info, rows

    def test_complete_crc_and_timestamps(self):
        info, rows = self.fixture()
        self.assertTrue(validate_trace(info, rows)["crc_verified"])
        self.assertFalse(validate_trace(info, rows)["physical_pwm_or_mechanical_stop_verified"])

    def test_corruption_and_missing_rows(self):
        info, rows = self.fixture()
        broken = copy.deepcopy(rows)
        broken[30]["tl"] -= 1
        with self.assertRaisesRegex(ValueError, "CRC"):
            validate_trace(info, broken)
        with self.assertRaisesRegex(ValueError, "Missing"):
            validate_trace(info, rows[:-1])

    def test_reject_unfrozen_overflow_and_missing_stop(self):
        base = "[STOP_INFO] gen=1 start_ms=0 stop_ms=0 count=201 recording=0 frozen=1 stop_seen=1 overflow=0 reason=1 crc=00000000"
        for bad in [base.replace("recording=0", "recording=1"), base.replace("frozen=1", "frozen=0"), base.replace("overflow=0", "overflow=1"), base.replace("stop_seen=1", "stop_seen=0")]:
            with self.assertRaises(ValueError):
                parse_info(bad)

    def test_reject_wrong_generation_index_and_output(self):
        line = "[STOP_ROW] gen=1 i=3 ms=30 seq=3 tl=-3 tr=3 rl=-1 rr=1 pr=2000 pl=2000 phase=2 reason=0"
        self.assertEqual(parse_row(line, 1, 3)["pr"], 2000)
        for bad, generation, index in [(line, 2, 3), (line, 1, 4), (line.replace("pr=2000", "pr=2001"), 1, 3), (line.replace("rl=-1", "rl=-32769"), 1, 3)]:
            with self.assertRaises(ValueError):
                parse_row(bad, generation, index)

    def test_counter_wrap_with_full_post_capture(self):
        info, rows = self.fixture()
        offset = 0xFFFFFF00
        for row in rows:
            row["ms"] = (row["ms"] + offset) & 0xFFFFFFFF
            row["seq"] = (row["seq"] + offset) & 0xFFFFFFFF
        info["stop_ms"] = offset
        info["crc32"] = zlib.crc32(b"".join(struct.pack("<10I", *(r[k] & 0xFFFFFFFF for k in FIELDS)) for r in rows))
        self.assertEqual(validate_trace(info, rows)["post_zero_capture_ms"], 2000)


if __name__ == "__main__":
    unittest.main()
