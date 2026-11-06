"""Reference-only sampling and whole-Session partitioning, with leakage checks."""
from __future__ import annotations

from pathlib import Path
from .archive import load_session, pixels
from .common import (checked_ref, digest, file_ref, identifier, read, require,
                     reservation, sha, utc, write, inside)
from .contracts import validate

SPLITS = {"train", "validation", "test", "regression", "performance", "challenge", "holdout"}
SCENES = {"straight", "curve_left", "curve_right", "roundabout", "tangent_entry",
          "intersection", "line_loss", "occlusion", "complex", "unknown"}
PHASES = {"approach", "entry", "inside", "exit", "recovery", "unknown"}


def validate_events(events, by_id):
    require(isinstance(events, list), "Events list required")
    require(len({e["event_id"] for e in events}) == len(events), "Duplicate Event ID")
    for event in events:
        validate("event", event)
        identifier(event["event_id"])
        require(event["event_type"] in SCENES, "Unknown event type")
        first, last = by_id[event["start_frame_id"]], by_id[event["end_frame_id"]]
        require(first["session_id"] == last["session_id"] == event["session_id"] and
                first["source_sequence"] <= last["source_sequence"], "Event range/session")
        require(event["review_status"] in {"draft", "submitted", "reviewed", "approved", "rejected"},
                "Event review status")
        require(event.get("boundary_confidence") in {"exact", "uncertain"}, "Event boundary confidence")
        previous = first["source_sequence"] - 1
        for phase in event.get("phase_segments", []):
            a, b = by_id[phase["start_frame_id"]], by_id[phase["end_frame_id"]]
            require(phase["phase"] in PHASES and a["session_id"] == b["session_id"] == event["session_id"]
                    and previous < a["source_sequence"] <= b["source_sequence"] <= last["source_sequence"],
                    "Overlapping/out-of-range event phases")
            previous = b["source_sequence"]
    return events


def make_view(root, view_id, sessions, every=1, events=None, context=2):
    require(type(every) is int and every > 0 and type(context) is int and context >= 0, "Sampling interval/context")
    require(len(set(sessions)) == len(sessions) and sessions, "Unique Sessions required")
    all_frames, refs = [], []
    for sid in sorted(sessions):
        base, _, frames = load_session(root, sid)
        refs.append({"session_id": sid, "manifest_sha256": sha(base / "session_manifest.json")})
        all_frames.extend(frames)
    by_id = {f["frame_id"]: f for f in all_frames}
    events = validate_events(events or [], by_id)
    selected = {}
    for sid in sorted(sessions):
        frames = [f for f in all_frames if f["session_id"] == sid]
        for n, frame in enumerate(frames):
            if n % every == 0:
                selected.setdefault(frame["frame_id"], []).append("uniform")
        for event in [e for e in events if e["session_id"] == sid]:
            indexes = {f["frame_id"]: i for i, f in enumerate(frames)}
            start, end = indexes[event["start_frame_id"]], indexes[event["end_frame_id"]]
            for frame in frames[max(0, start - context):end + context + 1]:
                selected.setdefault(frame["frame_id"], []).append("event:" + event["event_id"])
    records = []
    for i, frame in enumerate(all_frames):
        if frame["frame_id"] not in selected:
            continue
        sid = frame["session_id"]
        records.append({"frame_id": frame["frame_id"], "reasons": sorted(set(selected[frame["frame_id"]])),
                        "previous": all_frames[i - 1]["frame_id"] if i and all_frames[i - 1]["session_id"] == sid else None,
                        "next": all_frames[i + 1]["frame_id"] if i + 1 < len(all_frames) and all_frames[i + 1]["session_id"] == sid else None})
    result = {"schema_version": "1.0", "view_id": identifier(view_id), "created_at": utc(),
              "policy": {"id": "uniform-plus-events", "version": "1", "every": every,
                         "context_frames": context, "order": "session-id/source-sequence"},
              "sessions": refs, "events": events, "frames": records}
    target = inside(root, "views/" + view_id)
    with reservation(target):
        target.mkdir()
        validate("view", result)
        write(target / "view.json", result)
        write(target / "view.lock.json", {"view_sha256": sha(target / "view.json")})
    return result


def load_view(root, view_id):
    path = inside(root, "views/" + identifier(view_id) + "/view.json")
    require(sha(path) == read(path.with_name("view.lock.json"))["view_sha256"], "Sampling view changed")
    view = read(path)
    validate("view", view)
    frames = {}
    for item in view["sessions"]:
        base, _, current = load_session(root, item["session_id"])
        require(sha(base / "session_manifest.json") == item["manifest_sha256"], "Sampling source changed")
        frames.update({f["frame_id"]: (base, f) for f in current})
    require(len({f["frame_id"] for f in view["frames"]}) == len(view["frames"]), "Duplicate sampled frame")
    require(all(f["frame_id"] in frames for f in view["frames"]), "Unknown sampled frame")
    validate_events(view["events"], {key: value[1] for key, value in frames.items()})
    return view, {f["frame_id"]: frames[f["frame_id"]] for f in view["frames"]}


def average_hash(raw, width, height):
    values = [raw[min(height - 1, (2 * y + 1) * height // 16) * width +
                  min(width - 1, (2 * x + 1) * width // 16)] for y in range(8) for x in range(8)]
    total = sum(values)
    return sum((v * 64 >= total) << i for i, v in enumerate(values))


def make_split(root, split_id, assignments):
    require(isinstance(assignments, dict) and assignments and all(v in SPLITS for v in assignments.values()),
            "Explicit Session -> one split assignment required")
    exact, signatures, buckets, near, layouts, sessions = {}, {}, {}, [], {}, []
    for sid, split in sorted(assignments.items()):
        base, manifest, frames = load_session(root, sid)
        layouts.setdefault(str(manifest["metadata"].get("track_layout")), set()).add(split)
        sessions.append({"session_id": sid, "split": split,
                         "manifest_sha256": sha(base / "session_manifest.json")})
        seen_in_session = set()
        for frame in frames:
            key = frame["pixels_sha256"]
            require(key not in exact or exact[key]["split"] == split,
                    "Exact duplicate pixels leak across Session splits: " + frame["frame_id"])
            exact[key] = {"split": split, "frame_id": frame["frame_id"]}
            if key in seen_in_session:
                continue
            seen_in_session.add(key)
            h = average_hash(pixels(base, frame), frame["width"], frame["height"])
            key = split, h
            # Six bit bands: <=5 differing bits imply at least one unchanged band.
            bands = [(n, (h >> start) & ((1 << width) - 1))
                     for n, (start, width) in enumerate([(0, 11), (11, 11), (22, 11), (33, 11), (44, 10), (54, 10)])]
            if key not in signatures:
                candidates = set().union(*(buckets.get(band, set()) for band in bands))
                for old in sorted(candidates):
                    if len(near) >= 1000:
                        break
                    if old[0] != split and (h ^ old[1]).bit_count() <= 5:
                        near.append({"a": signatures[old], "b": frame["frame_id"],
                                     "hamming": (h ^ old[1]).bit_count()})
                signatures[key] = frame["frame_id"]
                for band in bands:
                    buckets.setdefault(band, set()).add(key)
    result = {"schema_version": "1.0", "split_id": identifier(split_id), "created_at": utc(),
              "sessions": sessions, "near_duplicate_candidates": near,
              "near_duplicate_method": "8x8 average hash <=5 bits, six bands; one representative per split/hash; <=1000 pairs",
              "layout_split_overlap": {k: sorted(v) for k, v in layouts.items() if len(v) > 1},
              "policy": "Session-disjoint; engineering/holdout never training; exact pixels disjoint across all roles"}
    target = inside(root, "splits/" + identifier(split_id))
    with reservation(target):
        target.mkdir(); validate("split", result); write(target / "split.json", result)
        write(target / "split.lock.json", {"split_sha256": sha(target / "split.json")})
    return result


def load_split(root, split_id):
    path = inside(root, "splits/" + identifier(split_id) + "/split.json")
    require(sha(path) == read(path.with_name("split.lock.json"))["split_sha256"], "Split changed")
    result = validate("split", read(path))
    require(result["split_id"] == split_id and len({s["session_id"] for s in result["sessions"]}) == len(result["sessions"]),
            "Split identity/session uniqueness")
    require(all(s["split"] in SPLITS for s in result["sessions"]), "Unknown split")
    return path, result
