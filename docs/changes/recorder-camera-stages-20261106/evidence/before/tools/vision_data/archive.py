"""Read-only source import, immutable archive and reproducible full-frame views."""
from __future__ import annotations

import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import zlib

from . import VERSION
from .common import (ROOT, checked_ref, data_root, digest, encoded, file_ref, identifier,
                     inside, png_bytes, read, read_png, require, reservation, sha, utc, write, loads)
from . import scraw
from . import scraw_v1
from .contracts import validate


def validate_session(meta):
    validate("session", meta)
    require(meta["schema_version"] == "1.0" and meta["format"] in (scraw.FORMAT, scraw_v1.FORMAT), "Session format")
    identifier(meta["session_id"])
    require(type(meta["synthetic"]) is bool, "Explicit synthetic provenance required")
    for key in ("width", "height", "stride"):
        require(type(meta[key]) is int and meta[key] > 0, "Invalid geometry")
    require(meta["width"] <= meta["stride"] and meta["stride"] * meta["height"] <= scraw.MAX_PAYLOAD
            and meta["pixel_format"] == "GRAY8", "Unsupported geometry/pixels")
    require(meta["timestamp_unit"] == "us" and bool(meta["timestamp_semantics"]), "Timestamp semantics")
    require(isinstance(meta["segments"], list) and meta["segments"] and
            len(set(meta["segments"])) == len(meta["segments"]), "Unique ordered segments required")
    for index, name in enumerate(meta["segments"]):
        require(name.endswith(".scraw") and "/" not in name and "\\" not in name, "Segment filename")
        require(name == f"{index:06d}.scraw" or (meta['format'] == scraw_v1.FORMAT and
                name == f"{int(name[:-6]):06d}.scraw"), "Invalid segment names")
    require(meta['segments'] == sorted(meta['segments']), 'Segment order')


def import_session(source, root, allow_draft=False):
    root = data_root(root)
    source = Path(source).resolve()
    require(not source.is_relative_to(root) and not root.is_relative_to(source), "Source/archive overlap")
    has_source_manifest = inside(source, "session.json").is_file()
    recovered = not has_source_manifest
    recovery_reason = 'missing_final_session_manifest'
    if has_source_manifest:
        try:
            meta = read(inside(source, 'session.json'))
        except ValueError:
            # A power cut can leave a partial final JSON file. Preserve those
            # exact bytes in A0, recover only from validated v1 segment headers.
            recovered, recovery_reason = True, 'invalid_final_session_manifest'
    if recovered:
        meta = scraw_v1.discover(source)
    validate_session(meta)
    require(meta['format'] == scraw_v1.FORMAT or allow_draft,
            "SCRAW draft format is not frozen; --allow-draft is required")
    require({p.name for p in source.glob("*.scraw")} == set(meta["segments"]), "Unlisted/missing SCRAW segment")
    session_id = meta["session_id"]
    target = inside(root, "sessions/" + session_id)
    with reservation(target):
        # Incomplete staging is retained for diagnosis, never exposed as a committed Session.
        staging = Path(tempfile.mkdtemp(prefix=".pending-" + session_id + "-", dir=target.parent))
        raw = staging / "A0"
        preview = staging / "A2"
        raw.mkdir(); preview.mkdir()
        source_refs = []
        source_names = (["session.json"] if has_source_manifest else []) + meta["segments"]
        for name in source_names:
            original = inside(source, name)
            require(original.is_file(), "Missing source file: " + name)
            before = sha(original)
            shutil.copyfile(original, raw / name)
            require(sha(original) == before == sha(raw / name), "Source changed during copy")
            source_refs.append(file_ref(raw / name, staging))
        if not recovered:
            require(read(raw / "session.json") == meta, "Session metadata changed during import")
        else:
            write(staging / 'recovered_session.json', meta)
        report = {"schema_version": "1.0", "session_id": session_id, "issues": [],
                  "segments": [], "recovered_frames": 0, "source_gap_count": 0,
                  "gap_cause": "unknown unless recorder counters provide independent evidence"}
        if recovered:
            report['issues'].append({'kind': 'recovered_without_final_session_manifest', 'reason': recovery_reason})
        if meta['segments'] != [f'{i:06d}.scraw' for i in range(len(meta['segments']))]:
            report['issues'].append({'kind': 'missing_segment_indices'})
        previous = previous_time = None
        frames_path = staging / "frames.jsonl"
        with frames_path.open("xb") as frames_out, (staging / "frame_manifest.csv").open(
                "x", newline="", encoding="utf-8") as csv_out:
            columns = ["frame_id", "source_sequence", "timestamp_us", "source_segment", "payload_offset",
                       "payload_size", "payload_sha256", "png_path"]
            csv_writer = csv.DictWriter(csv_out, fieldnames=columns, extrasaction="ignore")
            csv_writer.writeheader()
            for index, name in enumerate(meta["segments"]):
                segment_report = {"name": name, "issues": [], "ending": None}
                report["segments"].append(segment_report)
                actual_index = int(name[:-6]) if meta['format'] == scraw_v1.FORMAT else index
                for frame, payload in scraw.scan(raw / name, scraw.metadata(meta, actual_index), segment_report):
                    seq, at = frame["source_sequence"], frame["timestamp_us"]
                    if previous is not None and seq <= previous:
                        report["issues"].append({"kind": "duplicate_or_reordered", "sequence": seq})
                        continue
                    if previous is not None and seq != previous + 1:
                        report["source_gap_count"] += seq - previous - 1
                        report["issues"].append({"kind": "sequence_gap", "after": previous, "before": seq})
                    if previous_time is not None and at <= previous_time:
                        report["issues"].append({"kind": "nonmonotonic_timestamp", "sequence": seq})
                    frame_id = f"{session_id}:{seq}"
                    pixels = b"".join(payload[y * meta["stride"]:y * meta["stride"] + meta["width"]]
                                      for y in range(meta["height"]))
                    png = preview / f"{seq}.png"
                    png.write_bytes(png_bytes(pixels, meta["width"], meta["height"]))
                    frame.update(frame_id=frame_id, session_id=session_id,
                                 source_segment="A0/" + name,
                                 payload_sha256=hashlib.sha256(payload).hexdigest(),
                                 pixels_sha256=hashlib.sha256(pixels).hexdigest(),
                                 png_path=f"A2/{seq}.png", png_sha256=sha(png),
                                 width=meta["width"], height=meta["height"], stride=meta["stride"],
                                 timestamp_semantics=meta["timestamp_semantics"],
                                 a1_transform="identity_visible_rows_no_resize")
                    validate("frame", frame)
                    frames_out.write(encoded(frame) + b"\n")
                    csv_writer.writerow(frame)
                    report["recovered_frames"] += 1
                    previous, previous_time = seq, at
                if segment_report.get('gaps'):
                    report['issues'].append({'kind': 'recorder_gap_records', 'segment': actual_index,
                                             'gaps': segment_report['gaps']})
                if segment_report["ending"] is not None and index != len(meta["segments"]) - 1:
                    report["issues"].append({"kind": "premature_session_end", "segment": index})
        final = report["segments"][-1]["ending"]
        if final is None:
            report["issues"].append({"kind": "missing_session_end"})
        elif meta['format'] == scraw_v1.FORMAT:
            _check_v1_ending(final, report)
        else:
            for key in ("capture_frames", "vision_processed_frames", "recorded_frames"):
                require(type(final.get(key)) is int and final[key] >= 0, "END counter: " + key)
            if final["recorded_frames"] != report["recovered_frames"]:
                report["issues"].append({"kind": "recorded_count_mismatch"})
            if final["capture_frames"] < max(final["recorded_frames"], final["vision_processed_frames"]):
                report["issues"].append({"kind": "impossible_capture_counters"})
            if final["capture_frames"] != final["recorded_frames"]:
                report["issues"].append({"kind": "capture_recording_loss", "counters": final})
        report["status"] = "incomplete" if report["issues"] or any(
            item["issues"] for item in report["segments"]) else "complete"
        if report["recovered_frames"] == 0:
            report["status"] = "invalid"
        write(staging / "integrity.json", report)
        manifest = {"schema_version": "1.0", "tool_version": VERSION, "session_id": session_id,
                    "imported_at": utc(), "metadata": meta, "source_files": source_refs,
                    "frames": file_ref(frames_path, staging),
                    "csv": file_ref(staging / "frame_manifest.csv", staging),
                    "integrity": file_ref(staging / "integrity.json", staging)}
        write(staging / "session_manifest.json", manifest)
        write(staging / "archive.lock.json", {"manifest_sha256": sha(staging / "session_manifest.json")})
        for ref in source_refs:
            require(sha(inside(source, Path(ref["path"]).name)) == ref["sha256"], "Source changed during import")
        os.rename(staging, target)
    return {"session_id": session_id, "status": report["status"], "frames": report["recovered_frames"],
            "path": str(target), "synthetic": meta["synthetic"], "format_status":
            "TRAINING_V1" if meta['format'] == scraw_v1.FORMAT else "DRAFT"}


def _check_v1_ending(final, report):
    for key in ('observed_capture', 'vision_processed_frames', 'accepted', 'recorded_frames',
                'queue_full', 'pool_full', 'capture_ambiguous', 'storage_errors', 'base_source_sequence',
                'first_source_sequence', 'last_source_sequence', 'synced_before_end'):
        require(type(final.get(key)) is int and final[key] >= 0, 'SCRAW-1 END counter: '+key)
    require(final.get('capture_frames') is None and type(final.get('unknown_source_loss')) is bool,
            'SCRAW-1 observed events must not be relabeled exact sensor frames')
    if final['recorded_frames'] != report['recovered_frames'] or final['accepted'] != final['recorded_frames']:
        report['issues'].append({'kind': 'recorded_count_mismatch'})
    if final['observed_capture'] < max(final['accepted'], final['vision_processed_frames']) or (
            final['last_source_sequence'] and
            final['last_source_sequence'] - final['base_source_sequence'] != final['observed_capture']):
        report['issues'].append({'kind': 'impossible_capture_counters'})
    if (final['observed_capture'] != final['recorded_frames'] or final['unknown_source_loss'] or
            any(final[k] for k in ('queue_full','pool_full','capture_ambiguous','storage_errors')) or
            final.get('status') != 'complete_observed_window'):
        report['issues'].append({'kind': 'capture_recording_loss', 'counters': final})
    require(final['synced_before_end'] <= final['recorded_frames'], 'SCRAW-1 sync counter')


def load_session(root, session_id, verify=True):
    base = inside(root, "sessions/" + identifier(session_id))
    manifest_path = base / "session_manifest.json"
    require(sha(manifest_path) == read(base / "archive.lock.json")["manifest_sha256"], "Archive lock mismatch")
    manifest = read(manifest_path)
    require(manifest["session_id"] == session_id, "Archive identity mismatch")
    validate_session(manifest["metadata"])
    if verify:
        for ref in [*manifest["source_files"], manifest["frames"], manifest["csv"], manifest["integrity"]]:
            checked_ref(ref, base)
    frames = [loads(line) for line in checked_ref(manifest["frames"], base).read_text("utf-8").splitlines()]
    require(len({f["frame_id"] for f in frames}) == len(frames), "Duplicate Frame ID")
    require(all(f["frame_id"] == f'{session_id}:{f["source_sequence"]}' and
                f["session_id"] == session_id for f in frames), "Frame identity mismatch")
    for frame in frames:
        validate("frame", frame)
    return base, manifest, frames


def pixels(base, frame):
    with inside(base, frame["source_segment"]).open("rb") as stream:
        stream.seek(frame["payload_offset"])
        raw = stream.read(frame["payload_size"])
    require(hashlib.sha256(raw).hexdigest() == frame["payload_sha256"] and
            zlib.crc32(raw) == frame["payload_crc32"], "Raw frame hash/CRC mismatch")
    return b"".join(raw[y * frame["stride"]:y * frame["stride"] + frame["width"]]
                    for y in range(frame["height"]))


def verify_session(root, session_id):
    base, manifest, frames = load_session(root, session_id)
    for frame in frames:
        visible = pixels(base, frame)
        require(hashlib.sha256(visible).hexdigest() == frame["pixels_sha256"], "Visible pixels hash")
        png = inside(base, frame["png_path"])
        require(sha(png) == frame["png_sha256"], "PNG hash mismatch")
        require(read_png(png) == (frame["width"], frame["height"], visible), "PNG pixel mismatch")
    return {"session_id": session_id, "frames": len(frames),
            "integrity": read(checked_ref(manifest["integrity"], base))["status"]}


def synthetic_session(path, session_id="synthetic-001", count=12, offset=0, width=188, height=120):
    """Explicitly synthetic fixture generator, not camera evidence or ground truth."""
    path = Path(path)
    with reservation(path):
        path.mkdir()
        meta = {"schema_version": "1.0", "format": scraw.FORMAT, "synthetic": True,
                "session_id": identifier(session_id), "width": width, "height": height, "stride": width,
                "pixel_format": "GRAY8", "timestamp_unit": "us", "timestamp_semantics": "synthetic_clock",
                "firmware_git_sha": None, "camera": "synthetic", "configured_fps": 50,
                "observed_capture_fps": None, "track_layout": "synthetic-layout",
                "segments": ["000000.scraw"]}
        require(type(count) is int and 1 <= count <= 10000, "Synthetic count bound")
        write(path / "session.json", meta)
        frames = [(n + 1, n * 20000, bytes(210 if width // 5 + offset <= x < width * 4 // 5 + offset
                                          else 30 for y in range(height) for x in range(width))) for n in range(count)]
        ending = {"capture_frames": count, "vision_processed_frames": count // 2,
                  "recorded_frames": count, "drops": {"queue_full": 0, "storage": 0}}
        (path / meta["segments"][0]).write_bytes(scraw.segment_bytes(scraw.metadata(meta, 0), frames, ending))
    return meta
