"""Verify and summarize a full-image static trace; never infer safety budgets."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

from tools.speed_planning.visual_trace import VERSION, decode_frame
from tools.speed_planning.analyze_latency_trace import distribution


def runs(rows, keys):
    result = []
    first = 0
    for index in range(1, len(rows)+1):
        if index < len(rows) and all(rows[index][key] == rows[first][key] for key in keys):
            continue
        result.append({"firstIndex": first, "lastIndex": index-1, "frames": index-first,
                       "firstCaptureElapsedMs": (rows[first]["capture_ms"]-rows[0]["capture_ms"]) & 0xffffffff,
                       "lastCaptureElapsedMs": (rows[index-1]["capture_ms"]-rows[0]["capture_ms"]) & 0xffffffff,
                       "state": {key: rows[first][key] for key in keys}})
        first = index
    return result


def analyze(directory):
    manifest = json.loads((directory/"session.json").read_text(encoding="utf-8"))
    if manifest.get("version") != VERSION or manifest.get("complete") is not True:
        raise ValueError("Complete supported session required")
    frozen = manifest["frozen"]
    if frozen["active"] or frozen["reason"] != 1 or frozen["count"] != 750 or len(manifest["frames"]) != 750:
        raise ValueError("Incomplete or aborted capture")
    rows = []
    for index, saved in enumerate(manifest["frames"]):
        path = directory/saved["file"]
        if path.parent.resolve() != directory.resolve():
            raise ValueError("Frame path leaves session directory")
        raw = path.read_bytes()
        if hashlib.sha256(raw).hexdigest() != saved["sha256"]:
            raise ValueError("Saved frame hash mismatch")
        row = decode_frame(raw, frozen["gen"], index)
        if any(row[key] != saved[key] for key in row):
            raise ValueError("Manifest and raw frame metadata differ")
        rows.append(row)
    delta = lambda a, b: (b-a) & 0xffffffff
    sequence_steps = [delta(a["sequence"], b["sequence"]) for a, b in zip(rows, rows[1:])]
    periods = [delta(a["capture_ms"], b["capture_ms"]) for a, b in zip(rows, rows[1:])]
    if any(not 0 < n < 0x80000000 for n in sequence_steps) or any(not 0 < n <= 30000 for n in periods):
        raise ValueError("Non-forward capture identity")
    counts = lambda keys: {"/".join(map(str, state)): count for state, count in
                           Counter(tuple(row[k] for k in keys) for row in rows).items()}
    stages = {"captureToObserveReturn": ("capture_ms", "observe_end_ms"),
              "visionProcessing": ("process_start_ms", "vision_end_ms"),
              "recordImageAndDiagnostics": ("record_begin_ms", "record_end_ms")}
    return {"schemaVersion": "smartcar.visual-trace-analysis/1", "version": VERSION,
            "frames": len(rows), "captureSpanMs": delta(rows[0]["capture_ms"], rows[-1]["capture_ms"]),
            "captureIntervalMs": distribution(periods),
            "missingCaptureSequenceNumbers": sum(n-1 for n in sequence_steps),
            "copyFailureCounterFirstLast": [rows[0]["copy_failures"], rows[-1]["copy_failures"]],
            "controlValidFarCounts": counts(("control_valid", "control_far_valid")),
            "independentBandSupportCounts": counts(("near_band_support", "far_band_support")),
            "rawValidCounts": counts(("raw_valid",)),
            "quality": distribution([r["quality"] for r in rows]),
            "softwareStagesMs": {key: distribution([delta(r[a], r[b]) for r in rows])
                                 for key, (a, b) in stages.items()},
            "controlRuns": runs(rows, ("control_valid", "control_far_valid")),
            "supportRuns": runs(rows, ("near_band_support", "far_band_support")),
            "cueRuns": runs(rows, ("cue_bits",)),
            "productionBudgetCalibrated": False,
            "limits": ["Actual physical occlusion intervals require original-image annotation; cue bits are not ground truth.",
                       "Independent band support is algorithm geometry, not independently measured scene truth.",
                       "Static zero-PWM instrumented build; no RUNNING/MPC or exposure-to-electrical-PWM validation.",
                       "1ms time quantization; empirical maxima do not establish guaranteed limits."]}


def analyze_partial(directory):
    """Validate a saved prefix without promoting a board count to host completeness."""
    frozen = json.loads((directory/"frozen-info.json").read_text(encoding="utf-8"))
    rows, rejected = [], []
    for index, path in enumerate(sorted(directory.glob("frame-*.vqf"))):
        if path.name != f"frame-{index:04d}.vqf" or index >= frozen["count"]:
            raise ValueError("Non-contiguous or out-of-range saved file index")
        raw = path.read_bytes()
        try:
            row = decode_frame(raw, frozen["gen"], index)
        except ValueError as error:
            rejected.append({"file": path.name, "reason": str(error), "bytes": len(raw),
                             "sha256": hashlib.sha256(raw).hexdigest()})
            continue
        if index != len(rows):
            raise ValueError("Valid frames after a corrupt gap require separate analysis")
        row.update(index=index, file=path.name, sha256=hashlib.sha256(raw).hexdigest())
        rows.append(row)
    if not rows:
        raise ValueError("No verified prefix")
    delta = lambda a, b: (b-a) & 0xffffffff
    steps = [delta(a["sequence"], b["sequence"]) for a, b in zip(rows, rows[1:])]
    if any(not 0 < n < 0x80000000 for n in steps):
        raise ValueError("Non-forward source sequence")
    return {"schemaVersion": "smartcar.visual-trace-partial/1", "version": VERSION,
            "complete": False, "frozen": frozen, "verifiedFrames": len(rows),
            "unverifiedIndexRangeInclusive": [len(rows), frozen["count"]-1],
            "invalidFiles": rejected, "frames": rows,
            "captureSpanMs": delta(rows[0]["capture_ms"], rows[-1]["capture_ms"]),
            "missingCaptureSequenceNumbersInExportedPrefix": sum(n-1 for n in steps),
            "copyFailureCounterFirstLast": [rows[0]["copy_failures"], rows[-1]["copy_failures"]],
            "controlRuns": runs(rows, ("control_valid", "control_far_valid")),
            "supportRuns": runs(rows, ("near_band_support", "far_band_support")),
            "cueRuns": runs(rows, ("cue_bits",)), "productionApproved": False,
            "limits": ["Partial host export; frozen count is not proof of all saved frames.",
                       "Software cues and algorithm bands are not physical ground truth.",
                       "Static zero-PWM build; no MPC budget or motion validation."]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("session", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--partial", action="store_true", help="Verify a saved prefix; always complete=false")
    args = parser.parse_args()
    result = analyze_partial(args.session) if args.partial else analyze(args.session)
    with args.output.open("x", encoding="utf-8") as out:
        json.dump(result, out, ensure_ascii=False, indent=2)
        out.write("\n")
    print(json.dumps({k: v for k, v in result.items() if not k.endswith("Runs") and k != "frames"}, ensure_ascii=False))


if __name__ == "__main__":
    main()
