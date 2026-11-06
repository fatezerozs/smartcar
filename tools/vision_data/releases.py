"""Versioned, content-locked releases. A review snapshot is frozen at publication."""
from __future__ import annotations

from pathlib import Path
import shutil

from .annotations import load_bundle, review_history
from .archive import load_session, verify_session
from .common import (checked_ref, digest, file_ref, identifier, read, require,
                     reservation, sha, utc, write, inside)
from .sampling import load_view, load_split
from .contracts import validate


def create_release(root, release_id, view_id, split_id, bundle_ids, purpose="regression"):
    require(purpose in {"training", "validation", "test", "regression", "performance", "challenge", "holdout"}, "Release purpose")
    root = Path(root)
    view, frames = load_view(root, view_id)
    split_path, split = load_split(root, split_id)
    require(split["split_id"] == split_id, "Split identity")
    require(len({s["session_id"] for s in split["sessions"]}) == len(split["sessions"]), "Session leakage")
    assigned = {s["session_id"]: s for s in split["sessions"]}
    sessions = []
    # Purpose partitions are explicit; held-out or engineering data never silently trains.
    role = "train" if purpose == "training" else purpose
    chosen = {sid for sid, s in assigned.items() if s["split"] == role}
    selected = {fid: pair for fid, pair in frames.items() if pair[1]["session_id"] in chosen}
    require(selected, "No sampled frames for requested release purpose")
    for sid in sorted({f[1]["session_id"] for f in selected.values()}):
        base, manifest, _ = load_session(root, sid)
        require(sha(base / "session_manifest.json") == assigned[sid]["manifest_sha256"], "Split source changed")
        require(verify_session(root, sid)["integrity"] == "complete", "Incomplete/corrupt Session cannot be released")
        sessions.append({"session_id": sid, "manifest_sha256": sha(base / "session_manifest.json"),
                         "synthetic": manifest["metadata"]["synthetic"], "split": role,
                         "format": manifest["metadata"]["format"]})
    annotations, bundle_refs, reviews = [], [], []
    for bid in sorted(set(bundle_ids)):
        base, bundle = load_bundle(root, bid)
        require(bundle["view_sha256"] == digest(view), "Annotation sampling mismatch")
        history = review_history(root, bid)
        latest = {}
        for item in history:
            latest[item["annotation_id"]] = item
        for a in bundle["annotations"]:
            if a["frame_id"] not in selected:
                continue
            review = latest.get(a["id"])
            if review is None or review["action"] != "approved":
                continue
            require(review["human_attestation"] and review["annotation_sha256"] == digest(a), "Review/annotation mismatch")
            if purpose == "holdout":
                require(not a.get("exposure_algorithms") and not a.get("prediction_lineage"), "Contaminated holdout")
            annotations.append({"bundle_id": bid, "annotation": a, "review": review})
        bundle_refs.append({"bundle_id": bid, "canonical_sha256": sha(base / "canonical.json"),
                            "export_sha256": sha(base / "original-export.json")})
        reviews.extend(history)
    require(len({a["annotation"]["frame_id"] for a in annotations}) == len(annotations),
            "Multiple approved annotations per frame: adjudication required")
    manifest = {"schema_version": "1.0", "release_id": identifier(release_id), "created_at": utc(),
                "purpose": purpose, "sessions": sessions, "view_id": view_id,
                "view_sha256": digest(view), "split_id": split_id, "split_sha256": sha(split_path),
                "frame_ids": list(selected), "bundles": bundle_refs, "ground_truth": annotations,
                "review_snapshot": reviews, "labelled_frames": len(annotations),
                "unlabelled_frames": len(selected) - len(annotations),
                "events": [e for e in view["events"] if e["session_id"] in chosen],
                "synthetic": all(s["synthetic"] for s in sessions),
                "mixed_provenance": len({s["synthetic"] for s in sessions}) > 1,
                "format_status": "DRAFT", "authentication": "content hashes and operator-declared review, not signatures"}
    target = inside(root, "releases/" + identifier(release_id))
    with reservation(target):
        target.mkdir()
        validate("release", manifest)
        write(target / "release.json", manifest)
        write(target / "dataset.lock.json", {"algorithm": "sha256", "release_sha256": sha(target / "release.json")})
    return {"release_id": release_id, "frames": len(selected), "approved_labels": len(annotations),
            "synthetic": manifest["synthetic"], "release_sha256": sha(target / "release.json")}


def load_release(root, release_id):
    root = Path(root)
    target = inside(root, "releases/" + identifier(release_id))
    require(sha(target / "release.json") == read(target / "dataset.lock.json")["release_sha256"], "Dataset lock mismatch")
    release = read(target / "release.json")
    validate("release", release)
    require(release["release_id"] == release_id, "Release identity")
    view, available = load_view(root, release["view_id"])
    require(digest(view) == release["view_sha256"], "Released view changed")
    split_path, _ = load_split(root, release["split_id"])
    require(sha(split_path) == release["split_sha256"],
            "Released split changed")
    for item in release["sessions"]:
        base, _, _ = load_session(root, item["session_id"])
        require(sha(base / "session_manifest.json") == item["manifest_sha256"], "Released source changed")
        verify_session(root, item["session_id"])
    for item in release["bundles"]:
        base, _ = load_bundle(root, item["bundle_id"])
        require(sha(base / "canonical.json") == item["canonical_sha256"] and
                sha(base / "original-export.json") == item["export_sha256"], "Released annotation changed")
    require(len(set(release["frame_ids"])) == len(release["frame_ids"]) and
            all(fid in available for fid in release["frame_ids"]), "Release frame identity")
    return target, release, {fid: available[fid] for fid in release["frame_ids"]}
