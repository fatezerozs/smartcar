import copy
import struct
import sys
from pathlib import Path
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.speed_planning.analyze_stop_trace import analyze
from tools.speed_planning.export_stop_trace import FIELDS


def fixture():
    rows = []
    total = 0
    for i in range(231):
        delta = 10 if i <= 30 else 0
        if i:
            total += delta
        rows.append(dict(zip(FIELDS, [i * 10, i, -total, total, -delta, delta,
                                     2000 if i <= 29 else 0, 2000 if i <= 29 else 0,
                                     2 if i <= 29 else 0, 0 if i <= 29 else 1])))
    info = {"start_ms": 0, "stop_ms": 300, "count": len(rows), "reason": 1}
    info["crc32"] = zlib.crc32(b"".join(struct.pack("<10I", *(r[k] & 0xFFFFFFFF for k in FIELDS)) for r in rows))
    return {"info": info, "rows": rows}


class StopAnalysisTests(unittest.TestCase):
    def test_wheel_scales_and_no_safety_promotion(self):
        result = analyze(fixture(), circumference_m=1, left_cpr=100, right_cpr=200)
        self.assertAlmostEqual(result["preZeroWheelSpeeds"][1]["leftWheelMps"], 10)
        self.assertAlmostEqual(result["preZeroWheelSpeeds"][1]["rightWheelMps"], 5)
        self.assertEqual(result["wheelDistances"]["left"]["sampleBracketDistanceM"], [0, 0])
        self.assertFalse(result["productionApproved"])
        self.assertIsNone(result["brakeDecelerationMps2"])

    def test_reject_nonzero_post_stop_submission(self):
        trace = fixture()
        trace["rows"][31]["pr"] = 1
        trace["info"]["crc32"] = zlib.crc32(b"".join(struct.pack("<10I", *(r[k] & 0xFFFFFFFF for k in FIELDS)) for r in trace["rows"]))
        with self.assertRaisesRegex(ValueError, "Nonzero"):
            analyze(trace)

    def test_wraparound_and_crc_corruption(self):
        trace = fixture()
        offset = 0xFFFFFF00
        for r in trace["rows"]:
            r["ms"] = (r["ms"] + offset) & 0xFFFFFFFF
        trace["info"]["stop_ms"] = (300 + offset) & 0xFFFFFFFF
        trace["info"]["crc32"] = zlib.crc32(b"".join(struct.pack("<10I", *(r[k] & 0xFFFFFFFF for k in FIELDS)) for r in trace["rows"]))
        self.assertEqual(analyze(trace)["stopOffsetFromCaptureStartMs"], 300)
        broken = copy.deepcopy(trace)
        broken["rows"][50]["tl"] -= 1
        with self.assertRaisesRegex(ValueError, "CRC"):
            analyze(broken)


if __name__ == "__main__":
    unittest.main()
