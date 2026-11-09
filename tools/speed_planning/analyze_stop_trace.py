"""Offline wheel-domain stop metrics. No production calibration or serial access."""
import argparse
import json
from pathlib import Path

from tools.speed_planning.export_stop_trace import validate_trace


def analyze(trace, circumference_m=0.189, left_cpr=2335.8, right_cpr=2358.6):
    if min(circumference_m, left_cpr, right_cpr) <= 0:
        raise ValueError("Positive wheel conversion inputs required")
    checks = validate_trace(trace["info"], trace["rows"])
    rows = trace["rows"]
    origin = rows[0]["ms"]
    elapsed = [((row["ms"] - origin) & 0xFFFFFFFF) for row in rows]
    stop = (trace["info"]["stop_ms"] - origin) & 0xFFFFFFFF
    before = [i for i, timestamp in enumerate(elapsed) if timestamp <= stop]
    after = [i for i, timestamp in enumerate(elapsed) if timestamp >= stop]
    if not before or not after:
        raise ValueError("Stop event not bracketed")
    ib, ia = before[-1], after[0]
    scales = {"left": circumference_m / left_cpr, "right": circumference_m / right_cpr}

    def displacement(i, j, side):
        field, sign = ("tl", -1) if side == "left" else ("tr", 1)
        return sign * (rows[j][field] - rows[i][field]) * scales[side]

    if any(row["pr"] or row["pl"] for i, row in enumerate(rows) if elapsed[i] > stop):
        raise ValueError("Nonzero submitted output after stop event")
    speeds = []
    for requested_ms in (50, 100, 200):
        candidates = [i for i in before if elapsed[i] <= elapsed[ib] - requested_ms]
        if not candidates:
            continue
        start = candidates[-1]
        duration = (elapsed[ib] - elapsed[start]) / 1000
        speeds.append({"requestedWindowMs": requested_ms,
                       "actualWindowMs": duration * 1000,
                       "endOffsetFromSoftwareZeroMs": elapsed[ib] - stop,
                       "leftWheelMps": displacement(start, ib, "left") / duration,
                       "rightWheelMps": displacement(start, ib, "right") / duration})
    coast = {}
    for side in scales:
        values = [displacement(ia, len(rows) - 1, side), displacement(ib, len(rows) - 1, side)]
        coast[side] = {"sampleBracketDistanceM": [min(values), max(values)],
                       "signedWholeCaptureDistanceM": displacement(0, len(rows) - 1, side)}
    sensitivity = []
    for threshold in (0, 1, 2):
        over = [i for i in after if abs(rows[i]["rl"]) > threshold or abs(rows[i]["rr"]) > threshold]
        last_over = over[-1] if over else None
        first = next((i for i in after if last_over is None or i > last_over), None)
        tail = elapsed[-1] - (elapsed[last_over] if last_over is not None else stop)
        sensitivity.append({"toleranceCountsPer10ms": threshold,
                            "lastAboveToleranceOffsetMs": elapsed[last_over] - stop if last_over is not None else None,
                            "firstFinalTailSampleOffsetMs": elapsed[first] - stop if first is not None else None,
                            "capturedTailDurationMs": tail,
                            "tailAtLeast200ms": tail >= 200,
                            "physicalStopVerified": False})
    reverse_events = [{"offsetMs": elapsed[i] - stop, "rawLeft": r["rl"], "rawRight": r["rr"]}
                      for i, r in enumerate(rows) if elapsed[i] >= stop and (r["rl"] > 0 or r["rr"] < 0)]
    return {"schemaVersion": "smartcar.stop-wheel-analysis/1", "checks": checks,
            "conversion": {"circumferenceM": circumference_m, "leftCpr": left_cpr,
                           "rightCpr": right_cpr, "forwardRawMultipliers": [-1, 1],
                           "uncertaintyVerified": False},
            "stopReason": trace["info"]["reason"], "captureSpanMs": elapsed[-1],
            "stopOffsetFromCaptureStartMs": stop,
            "bracketingSamplesOffsetMs": [elapsed[ib] - stop, elapsed[ia] - stop],
            "preZeroWheelSpeeds": speeds, "wheelDistances": coast,
            "settlingToleranceSensitivity": sensitivity,
            "postZeroReverseSignedSamples": reverse_events,
            "productionApproved": False, "physicalStoppingDistanceM": None,
            "brakeDecelerationMps2": None, "jerkLimitMps3": None,
            "limits": ["Wheel-domain observation, not independently verified ground velocity/distance.",
                       "Software PWM submissions, not measured electrical reload timing.",
                       "Sampling bracket excludes scale, slip and timing uncertainty.",
                       "Tolerance sensitivity is descriptive; no production zero-speed threshold adopted.",
                       "One run and unmeasured battery/load variation cannot establish a safe brake bound."]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = analyze(json.loads(args.trace.read_text(encoding="utf-8")))
    with args.output.open("x", encoding="utf-8") as out:
        json.dump(result, out, ensure_ascii=False, indent=2)
        out.write("\n")
    print(json.dumps({"output": str(args.output), "crc_verified": result["checks"]["crc_verified"]}))


if __name__ == "__main__":
    main()
