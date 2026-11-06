"""Offline C31 consumer; separates frame quality, sequence behavior and Host cost."""
from __future__ import annotations

import hashlib
import json
import math
import platform
import re
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
import time

from . import VERSION
from .archive import pixels
from .common import (ROOT, checked_ref, digest, encoded, finite, identifier, read, read_png,
                     require, reservation, sha, utc, write, inside, timestamp)
from .releases import load_release
from .contracts import validate

DEFINES = ["VISION_ROUTE_SWITCH=1", "VISION_MIDTERM_ROUTE_TRIAL=1", "VISION_ENCODER_ENABLE=1",
           "VISION_MIDTERM_TRAINING=1", "VISION_MIDTERM_AUTONOMOUS=0"]


def git_identity():
    def git(*args):
        return subprocess.check_output(["git", *args], cwd=ROOT, text=True, timeout=15).strip()
    return {"sha": git("rev-parse", "HEAD"), "dirty": bool(git("status", "--porcelain"))}


def production_snapshot():
    paths = sorted([*ROOT.joinpath("demo/code").glob("*.h"), ROOT / "demo/code/vision_midterm.c",
                    ROOT / "demo/code/app_runtime.c", ROOT / "tools/vision_data.py",
                    *ROOT.joinpath("tools/vision_data").glob("*.py"),
                    *ROOT.joinpath("tools/vision_data").glob("*.json"),
                    *ROOT.joinpath("tools/vision_data").glob("*.xml"),
                    ROOT / "tools/vision_data/c31_adapter.c"])
    return {p.relative_to(ROOT).as_posix(): sha(p) for p in paths}


def compile_adapter(folder, compiler="gcc"):
    exe = shutil.which(compiler)
    require(exe is not None, "C11 Host compiler unavailable: " + compiler)
    version = subprocess.check_output([exe, "--version"], timeout=15, text=True).splitlines()[0]
    target = folder / ("c31-vision.exe" if sys.platform == "win32" else "c31-vision")
    command = [exe, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-misleading-indentation",
               "-Wno-unused-function", *["-D" + value for value in DEFINES],
               "-I", str(ROOT / "demo/code"), str(ROOT / "tools/vision_data/c31_adapter.c"),
               str(ROOT / "demo/code/vision_midterm.c"), "-lm", "-o", str(target)]
    completed = subprocess.run(command, capture_output=True, text=True, timeout=90)
    (folder / "compile.stdout.log").write_text(completed.stdout, "utf-8")
    (folder / "compile.stderr.log").write_text(completed.stderr, "utf-8")
    require(completed.returncode == 0, "Host adapter compile failed; see compile.stderr.log")
    return target, {"command": command, "version": version, "executable_sha256": sha(exe),
                    "adapter_sha256": sha(target), "macros": DEFINES, "optimization": "O2"}


def stats(values):
    if not values:
        return None
    require(all(finite(v) for v in values), "Nonfinite metric")
    values = sorted(values)
    def quantile(q):
        p = (len(values) - 1) * q
        low, high = math.floor(p), math.ceil(p)
        return values[low] + (values[high] - values[low]) * (p - low)
    return {"count": len(values), "mean": statistics.mean(values), "median": statistics.median(values),
            "min": values[0], "max": values[-1], "p95": quantile(.95), "p99": quantile(.99)}


def intersect(points, y):
    xs = []
    for (ax, ay), (bx, by) in zip(points, points[1:]):
        if ay == by:
            continue
        if min(ay, by) <= y <= max(ay, by):
            x = ax + (bx - ax) * (y - ay) / (by - ay)
            if not any(abs(x - old) < 1e-7 for old in xs):
                xs.append(x)
    return xs[0] if len(xs) == 1 else None


def quality_metrics(result, annotation, frame, bundle_root):
    errors, eligible, detected = {}, {}, {}
    for role, field in [("left_boundary", "left"), ("right_boundary", "right"), ("centerline", "center")]:
        per_role = [o for o in annotation.get("geometry", []) if o["role"] == role and
                    o["visibility"] in {"visible", "partial"} and o["type"] == "polyline"]
        for y in range(frame["height"]):
            hits = [value for o in per_role if (value := intersect(o["points"], y)) is not None]
            if len(hits) != 1:
                continue
            eligible[role] = eligible.get(role, 0) + 1
            flag = result["row_flags"][y]
            measured = flag == 3 or (field == "left" and flag == 1) or (field == "right" and flag == 2)
            # Center includes inferred rows, separately observable from boundary measurement.
            if field == "center":
                measured = flag in (1, 2, 3)
            if measured and result[field][y] != 255:
                errors.setdefault(role, []).append(result[field][y] - hits[0])
                detected[role] = detected.get(role, 0) + 1
    metrics = {role: {"mae_px": statistics.mean(abs(v) for v in values),
                      "rmse_px": math.sqrt(statistics.mean(v * v for v in values)), "rows": len(values)}
               for role, values in errors.items()}
    iou = None
    if annotation.get("mask"):
        _, _, mask = read_png(checked_ref(annotation["mask"], bundle_root))
        binary = bytes.fromhex(result["binary_hex"])
        require(len(binary) == len(mask), "Prediction mask dimensions")
        intersection = sum(a == 1 and b != 0 for a, b in zip(mask, binary))
        union = sum(a != 255 and (a == 1 or b != 0) for a, b in zip(mask, binary))
        iou = intersection / union if union else None  # Empty is undefined, not a perfect score.
    return {"geometry": metrics, "eligible_rows": eligible, "detected_rows": detected,
            "preprocessing_track_iou": iou, "scene_accuracy": None,
            "scene_accuracy_reason": "C31 has no output taxonomy equivalent to canonical T1"}


def evaluate(root, release_id, run_id, mode="frame", warmup=1, repetitions=3, compiler="gcc"):
    require(mode in {"frame", "sequence"} and type(warmup) is int and 0 <= warmup <= 100 and
            type(repetitions) is int and 1 <= repetitions <= 1000, "Evaluation configuration bounds")
    root = Path(root)
    release_base, release, frames = load_release(root, release_id)
    require(release["purpose"] != "training", "Training release is not an evaluation set")
    truths = {a["annotation"]["frame_id"]: a for a in release["ground_truth"]}
    for entry in truths.values():
        a = entry["annotation"]
        require(not a.get("prediction_lineage") and all("c31" not in alg.lower() for alg in a.get("exposure_algorithms", [])),
                "C31 self-evaluation contamination: independent reannotation required")
    ordered = sorted(frames, key=lambda fid: (frames[fid][1]["session_id"], frames[fid][1]["source_sequence"]))
    require(all((f[1]["width"], f[1]["height"]) == (188, 120) for f in frames.values()),
            "C31 needs actual 188x120 input; previews are not upscaled")
    target = inside(root, "runs/" + identifier(run_id))
    before = production_snapshot()
    with reservation(target):
        target.mkdir()
        identity = git_identity()
        runtime = (ROOT / "demo/code/app_runtime.c").read_text("utf-8")
        gap_define = re.search(r"^#define HISTORY_GAP_MS\s+(\d+)U", runtime, re.MULTILINE)
        require(gap_define is not None, "Runtime history-gap policy needs explicit adapter review")
        history_gap_us = int(gap_define[1]) * 1000  # Offline reset policy, not MCU replay.
        executable, toolchain = compile_adapter(target, compiler)
        resets, previous, prev_time = [], None, None
        input_path = target / "input.graystream"
        with input_path.open("xb") as stream:
            for fid in ordered:
                base, frame = frames[fid]
                current = frame["session_id"], frame["source_sequence"]
                reset = mode == "frame" or previous is None or current[0] != previous[0] or current[1] != previous[1] + 1
                if previous is not None and current[0] == previous[0] and (
                        frame["timestamp_us"] <= prev_time or frame["timestamp_us"] - prev_time > history_gap_us):
                    reset = True
                resets.append({"frame_id": fid, "reset": reset, "reason": "independent_frame" if mode == "frame" else
                               ("session_or_gap_boundary" if reset else "continuous")})
                stream.write(b"R" if reset else b"C"); stream.write(pixels(base, frame))
                previous, prev_time = current, frame["timestamp_us"]
        observations, durations, walls, first = [], [], [], None
        measurement_hashes = []
        for repetition in range(-warmup, repetitions):
            output = target / f"output-{repetition}.jsonl"
            command = [str(executable), str(input_path), str(output)]
            start = time.perf_counter_ns()
            completed = subprocess.run(command, capture_output=True, timeout=120)
            wall = time.perf_counter_ns() - start
            require(completed.returncode == 0 and not completed.stderr, "C31 adapter run failed")
            results = [json.loads(line) for line in output.read_text("utf-8").splitlines()]
            require(len(results) == len(ordered), "Adapter output count")
            for index, item in enumerate(results):
                require(item["index"] == index and type(item["compute_ns"]) is int and item["compute_ns"] >= 0,
                        "Adapter result ordering/timing")
                require(all(len(item[key]) == 120 for key in ("left", "right", "center", "row_flags")) and
                        len(bytes.fromhex(item["binary_hex"])) == 22560, "Adapter result geometry")
            semantic = [{k: v for k, v in item.items() if k != "compute_ns"} for item in results]
            measurement_hashes.append(digest(semantic))
            if first is None:
                first = results
            else:
                require(digest(semantic) == measurement_hashes[0], "Nondeterministic outputs across repeats")
            if repetition >= 0:
                walls.append(wall)
                for fid, item in zip(ordered, results):
                    durations.append(item["compute_ns"])
                    observations.append({"frame_id": fid, "repetition": repetition,
                                         "compute_ns": item["compute_ns"]})
        quality, mae, ious = [], [], []
        for fid, result in zip(ordered, first):
            if fid in truths:
                entry = truths[fid]; frame = frames[fid][1]
                metrics = quality_metrics(result, entry["annotation"], frame,
                                          root / "annotations" / entry["bundle_id"])
                quality.append({"frame_id": fid, "annotation_id": entry["annotation"]["id"], **metrics})
                if "centerline" in metrics["geometry"]:
                    mae.append(metrics["geometry"]["centerline"]["mae_px"])
                if metrics["preprocessing_track_iou"] is not None:
                    ious.append(metrics["preprocessing_track_iou"])
        deltas = []
        for i in range(1, len(ordered)):
            if not resets[i]["reset"] and first[i]["valid"] and first[i - 1]["valid"]:
                deltas.append(abs(first[i]["error_px"] - first[i - 1]["error_px"]))
        coverage = {}
        for role in {role for item in quality for role in item["eligible_rows"]}:
            eligible = sum(item["eligible_rows"].get(role, 0) for item in quality)
            detected = sum(item["detected_rows"].get(role, 0) for item in quality)
            coverage[role] = {"eligible_rows": eligible, "detected_rows": detected,
                              "fraction": detected / eligible if eligible else None}
        gap_count = sum(r["reset"] for r in resets) - len({frames[f][1]["session_id"] for f in ordered})
        # T1 classification/event stage has no one-to-one C31 output contract.
        # Retain raw states, but never turn them into invented truth scores.
        report = {"schema_version": "1.0", "run_id": run_id, "created_at": utc(), "tool_version": VERSION,
                  "algorithm_id": "C31", "git": identity,
                  "input_tree_sha256": digest({"files": before, "release": sha(release_base / "release.json")}),
                  "input_files": before, "toolchain": toolchain,
                  "environment_id": digest({"system": platform.platform(), "machine": platform.machine(),
                                            "processor": platform.processor(), "host": platform.node()}),
                  "release_id": release_id, "release_sha256": sha(release_base / "release.json"),
                  "parameters": {"mode": mode, "macros": DEFINES, "history_gap_us": history_gap_us},
                  "warmup": warmup, "repetitions": repetitions,
                  "frame_ids": ordered, "reset_boundaries": resets, "measurements": observations,
                  "host_compute_ns": stats(durations), "host_process_wall_ns_including_io": stats(walls),
                  "quality": quality, "row_coverage": coverage,
                  "centerline_frame_mae_px": stats(mae), "preprocessing_track_iou": stats(ious),
                  "sequence_error_step_px": stats(deltas) if mode == "sequence" else None,
                  "sequence_gap_resets": max(0, gap_count) if mode == "sequence" else None,
                  "synthetic": release["synthetic"], "mixed_provenance": release["mixed_provenance"],
                  "quality_status": "OBSERVED_APPROVED_LABELS" if quality else "NO_APPROVED_GROUND_TRUTH",
                  "classification_event_metrics": "NOT_MEASURED: output taxonomy / reviewed Event adapter required",
                  "status": "SOFTWARE_RUN_COMPLETED", "release_eligible": False,
                  "measurement_scope": "Host only; no MCU timing or physical vehicle inference"}
        require(production_snapshot() == before, "Production inputs changed during evaluation")
        load_release(root, release_id)  # Recheck dataset/labels after the actual run.
        validate("run", report)
        write(target / "run.json", report)
        predictions = []
        for fid, result in zip(ordered, first):
            center = [[result["center"][y], y] for y in range(120) if result["row_flags"][y] in (1, 2, 3)]
            predictions.append({"schema_version": "1.0", "id": "pred-" + digest([run_id, fid])[:32],
                                "kind": "prediction", "frame_id": fid, "status": "draft", "algorithm": "C31",
                                "level": "T2", "scene": "unknown", "actor": "machine:C31", "created_at": utc(),
                                "source_tool": "c31-offline-adapter", "version": 1,
                                "input_sha256": frames[fid][1]["pixels_sha256"], "configuration": report["parameters"],
                                "run_id": run_id, "exposure_algorithms": ["C31"],
                                "geometry": [{"id": "center", "role": "centerline", "type": "polyline",
                                              "visibility": "partial", "points": center}] if len(center) >= 2 else []})
        for prediction in predictions:
            validate("prediction", prediction)
        write(target / "predictions.json", predictions)
        write(target / "run.lock.json", {"run_sha256": sha(target / "run.json"),
                                           "predictions_sha256": sha(target / "predictions.json")})
        lines = [f"# {run_id}", "", f"Release: {release_id}; SHA256 {report['release_sha256']}",
                 f"Git: {identity['sha']}; dirty={identity['dirty']}", f"Mode: {mode}; synthetic={report['synthetic']}",
                 f"Frames: {len(ordered)}; labelled: {len(quality)}; repeats={repetitions}; warmup={warmup}",
                 f"Host compute ns: {report['host_compute_ns']}", f"Centerline frame MAE px: {report['centerline_frame_mae_px']}",
                 f"Sequence error step px: {report['sequence_error_step_px']}", "", report["classification_event_metrics"],
                 "No quality release gate or hardware acceptance is implied. Predictions are never ground truth."]
        (target / "summary.md").write_text("\n".join(lines) + "\n", "utf-8")
    return {"run_id": run_id, "frames": len(ordered), "labelled_frames": len(quality),
            "synthetic": report["synthetic"], "status": report["status"], "release_eligible": False}


def compare(baseline_path, candidate_path, approval_path=None, max_mae_increase=0.,
            max_time_regression=.05, noise_fraction=.02):
    base, current = read(baseline_path), read(candidate_path)
    for limit in (max_mae_increase, max_time_regression, noise_fraction):
        require(finite(limit) and limit >= 0, "Nonnegative finite gate limits")
    for key in ("release_sha256", "parameters", "environment_id", "warmup", "repetitions", "frame_ids"):
        require(base[key] == current[key], "Comparison condition mismatch: " + key)
    require(base["toolchain"]["version"] == current["toolchain"]["version"], "Compiler mismatch")
    approved = False
    if approval_path:
        approval = read(approval_path)
        timestamp(approval["approved_at"])
        require(approval["reviewer"] and approval["approved_at"] and
                approval["run_sha256"] == sha(baseline_path) and
                approval["git_sha"] == base["git"]["sha"] and not base["git"]["dirty"],
                "Baseline approval must bind a clean, exact prior run")
        approved = True
    gates = {}
    if base["centerline_frame_mae_px"] and current["centerline_frame_mae_px"]:
        gates["quality"] = "PASS" if current["centerline_frame_mae_px"]["mean"] <= base["centerline_frame_mae_px"]["mean"] + max_mae_increase else "FAIL"
    else:
        gates["quality"] = "INCONCLUSIVE"
    base_coverage, current_coverage = base.get("row_coverage", {}), current.get("row_coverage", {})
    gates["coverage"] = "PASS" if base_coverage else "INCONCLUSIVE"
    for role, values in base_coverage.items():
        newer = current_coverage.get(role)
        if not newer or newer["eligible_rows"] != values["eligible_rows"] or newer["detected_rows"] < values["detected_rows"]:
            gates["coverage"] = "FAIL"
    if base.get("preprocessing_track_iou"):
        gates["mask_quality"] = "PASS" if current.get("preprocessing_track_iou") and (
            current["preprocessing_track_iou"]["mean"] >= base["preprocessing_track_iou"]["mean"]) else "FAIL"
    old, new = base["host_compute_ns"]["median"], current["host_compute_ns"]["median"]
    if old <= 0:
        gates["performance"] = "INCONCLUSIVE"
    else:
        change = new / old - 1
        gates["performance"] = ("INCONCLUSIVE" if abs(change - max_time_regression) <= noise_fraction else
                                ("PASS" if change <= max_time_regression else "FAIL"))
    if not approved:
        gates["approved_baseline"] = "INCONCLUSIVE"
    status = "FAIL" if "FAIL" in gates.values() else ("INCONCLUSIVE" if "INCONCLUSIVE" in gates.values() else "PASS")
    return {"schema_version": "1.0", "status": status, "gates": gates,
            "baseline_sha256": sha(baseline_path), "candidate_sha256": sha(candidate_path),
            "thresholds": {"max_mae_increase_px": max_mae_increase, "max_time_regression": max_time_regression,
                           "noise_fraction": noise_fraction},
            "quality_and_performance_independent": True, "release_eligible": False}
