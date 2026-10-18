"""Fit replaceable Gaussian/HMM parameters from HUMAN-labelled feature sequences.

No third-party packages, no vehicle IO. CSV columns:
sequence,ms,state,heading,curvature,open_left,open_right,split,stripes
sequence identifies a recording; rows within it must have increasing ms.
Blank feature cells mean missing measurements, never implicit zero. State names
match tb_state_name(). Split train/validation by recording, not adjacent frames.
Output is a C header; enabling it never enables motors or full-track navigation.
"""
import argparse
import csv
import math
from pathlib import Path

STATES = ["UNKNOWN", "STRAIGHT", "LEFT", "RIGHT", "CROSS", "ZEBRA",
          "RING_L_ENTRY", "RING_L_IN", "RING_L_EXIT",
          "RING_R_ENTRY", "RING_R_IN", "RING_R_EXIT"]
FEATURES = ["heading", "curvature", "open_left", "open_right", "split", "stripes"]


def fit(rows, minimum=20, variance_floor=0.005, alpha=1.0, max_gap_ms=100):
    """Estimate emissions with Welford statistics and smoothed distributions.

    Transition counts only include changes of label in contiguous recordings:
    same-state persistence is represented separately by dwell_s in the runtime.
    Unobserved transition rows use a uniform distribution, not a zero row.
    All states/features require sufficient labelled samples; missing classes
    fail explicitly instead of silently pretending expert initial values trained.
    """
    if (minimum < 2 or not math.isfinite(variance_floor) or
            not 0.0001 <= variance_floor <= 100 or
            not math.isfinite(alpha) or not 0 < alpha <= 10000 or max_gap_ms < 1):
        raise ValueError("invalid fitting parameters")
    n = [[0] * len(FEATURES) for _ in STATES]
    mean = [[0.0] * len(FEATURES) for _ in STATES]
    m2 = [[0.0] * len(FEATURES) for _ in STATES]
    prior = [alpha] * len(STATES)
    transition = [[alpha] * len(STATES) for _ in STATES]
    previous = {}
    for line, row in enumerate(rows, 2):
        if row["state"] not in STATES:
            raise ValueError(f"line {line}: unknown state")
        state = STATES.index(row["state"])
        recording = row["sequence"].strip()
        ms = int(row["ms"])
        if not recording or ms < 0:
            raise ValueError(f"line {line}: invalid sequence/time")
        if recording in previous:
            old_ms, old_state = previous[recording]
            if ms <= old_ms:
                raise ValueError(f"line {line}: non-increasing timestamp")
            if ms - old_ms <= max_gap_ms and old_state != state:
                transition[old_state][state] += 1
        previous[recording] = ms, state
        prior[state] += 1
        for k, feature in enumerate(FEATURES):
            text = row[feature].strip()
            if not text:
                continue
            value = float(text)
            low, high = (-8, 8) if k < 2 else (0, 1)
            if not math.isfinite(value) or not low <= value <= high:
                raise ValueError(f"line {line}: invalid {feature}")
            n[state][k] += 1
            delta = value - mean[state][k]
            mean[state][k] += delta / n[state][k]
            m2[state][k] += delta * (value - mean[state][k])
    for state in range(len(STATES)):
        for k in range(len(FEATURES)):
            if n[state][k] < minimum:
                raise ValueError(f"insufficient samples: {STATES[state]}/{FEATURES[k]}={n[state][k]}")
    variance = [[min(100.0, max(variance_floor, m2[s][k] / (n[s][k] - 1)))
                 for k in range(len(FEATURES))] for s in range(len(STATES))]
    norm = lambda values: [x / sum(values) for x in values]
    return dict(mean=mean, variance=variance, prior=norm(prior),
                transition=[norm(row) for row in transition])


def header(model):
    """Render assignments, avoiding compiler-dependent JSON parsers on the MCU."""
    lines = ["/* GENERATED from labelled sequences; fitted != field validated. */",
             "#ifndef TRACK_MODEL_FITTED_H", "#define TRACK_MODEL_FITTED_H",
             "static void tb_apply_fitted_model(tb_model_t *m)", "{"]
    def number(x):
        return f"{x:.9e}f"
    for i in range(len(STATES)):
        lines.append(f"    m->prior[{i}] = {number(model['prior'][i])};")
        for key in ("mean", "variance", "transition"):
            for j, value in enumerate(model[key][i]):
                lines.append(f"    m->{key}[{i}][{j}] = {number(value)};")
    lines.extend(["    m->trained = 1;", "}", "#endif", ""])
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--minimum", type=int, default=20)
    parser.add_argument("--variance-floor", type=float, default=0.005)
    parser.add_argument("--alpha", type=float, default=1.0)
    parser.add_argument("--max-gap-ms", type=int, default=100)
    args = parser.parse_args()
    # Never silently replace a calibration file or a user's input.
    if args.output.exists():
        parser.error("output already exists; choose a new filename and review before replacing calibration")
    try:
        with args.csv.open(encoding="utf-8-sig", newline="") as stream:
            model = fit(csv.DictReader(stream), args.minimum, args.variance_floor, args.alpha, args.max_gap_ms)
        with args.output.open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(header(model))
    except (ValueError, KeyError, OSError) as exc:
        parser.exit(1, f"fit failed: {exc}\n")
    print("Fitted header written; review held-out sequences before deployment. Motors remain locked.")


if __name__ == "__main__":
    main()
