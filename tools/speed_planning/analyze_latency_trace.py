"""Offline analysis of a validated static software timing trace."""
import argparse
import json
from pathlib import Path

from tools.speed_planning.export_latency_trace import VERSION, validate


def distribution(values):
    ordered = sorted(values)

    def percentile(q):
        index = (len(ordered)-1)*q
        low = int(index)
        high = min(low+1, len(ordered)-1)
        return ordered[low]+(ordered[high]-ordered[low])*(index-low)

    return {"n": len(ordered), "min": ordered[0], "p50": percentile(.5),
            "p95": percentile(.95), "p99": percentile(.99), "max": ordered[-1]}


def analyze(trace):
    if trace["version"] != VERSION:
        raise ValueError("Unsupported firmware version")
    rows = trace["rows"]
    validate(rows, trace["info"])
    delta = lambda a, b: (b-a) & 0xffffffff
    stages = {"capture_to_copy_start": ("cap", "copy"),
              "copy_to_process_start": ("copy", "process"),
              "vision_and_configured_bayes": ("process", "end"),
              "post_vision_to_observe_return": ("end", "observed"),
              "capture_to_observe_return": ("cap", "observed")}
    gaps = [delta(a["cap"], b["cap"]) for a, b in zip(rows, rows[1:])]
    if not all(0 < gap < 30000 for gap in gaps):
        raise ValueError("Capture timestamps not strictly forward within bounds")
    return {"schemaVersion": "smartcar.static-latency-analysis/1", "version": VERSION,
            "generation": int(trace["info"]["gen"]), "frames": len(rows),
            "clockResolutionMs": 1, "captureSpanMs": delta(rows[0]["cap"], rows[-1]["cap"]),
            "captureIntervalMs": distribution(gaps),
            "stagesMs": {name: distribution([delta(row[a], row[b]) for row in rows])
                         for name, (a, b) in stages.items()},
            "missingCaptureSequenceNumbers": sum(delta(a["seq"], b["seq"])-1
                                                  for a, b in zip(rows, rows[1:])),
            "copyFailureCounterFirst": rows[0]["drops"],
            "copyFailureCounterLast": rows[-1]["drops"],
            "productionLatencyCalibrated": False,
            "percentileMethod": "Linear interpolation at index (n-1)*q; empirical only",
            "limits": ["OFF/OBSERVE, private zero-PWM driver; RUNNING and MPC execution not measured.",
                       "CSI completion is not exposure time; observe return is not physical PWM reload.",
                       "1ms clock: zero stage delta means unresolved below clock granularity, not zero physical duration.",
                       "One 256-frame session; empirical maxima and percentiles are not guaranteed bounds."]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = analyze(json.loads(args.trace.read_text(encoding="utf-8")))
    with args.output.open("x", encoding="utf-8") as out:
        json.dump(result, out, ensure_ascii=False, indent=2)
        out.write("\n")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
