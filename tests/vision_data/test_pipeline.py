"""Synthetic/mock tests only: no hardware, network or real human approval."""
from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import unittest
import zlib
from http.client import HTTPConnection

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from vision_data import scraw
from vision_data.annotations import (add_review, convert_export, export_canonical, load_bundle,
                                     make_tasks, qc, review_history, import_canonical)
from vision_data.archive import import_session, load_session, pixels, synthetic_session, verify_session
from vision_data.common import data_root, digest, encoded, inside, png_bytes, read, read_png, sha, write, loads
from vision_data.connector import MockTransport, batch
from vision_data.contracts import validate
from vision_data.evaluation import compare, evaluate, quality_metrics
from vision_data.exchange import asset_server, loopback_base, prediction_exchange
from vision_data.releases import create_release, load_release
from vision_data.sampling import load_split, load_view, make_split, make_view, validate_events


class PipelineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="smartcar-vds-test-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "data"
        self.source = self.base / "source"

    def imported(self, count=4, offset=0, sid="s1"):
        synthetic_session(self.source, sid, count, offset)
        self.source_hashes = {p.name: sha(p) for p in self.source.iterdir()}
        return import_session(self.source, self.root, True)

    def tasks(self, every=1, split="regression"):
        make_view(self.root, "v1", ["s1"], every)
        make_split(self.root, "split1", {"s1": split})
        self.exchange = self.base / "exchange"
        make_tasks(self.root, "v1", self.exchange, "split1")

    def annotation(self, fid="s1:1", exposure=None, level="T2"):
        return {"schema_version": "1.0", "id": "test-ann", "kind": "annotation", "frame_id": fid,
                "level": level, "status": "submitted", "origin": "human", "actor": "synthetic-annotator",
                "created_at": "2026-11-04T00:00:00+00:00", "source_tool": "synthetic-test-fixture",
                "version": 1, "scene": "straight", "environment": ["lighting_normal"],
                "exposure_algorithms": exposure or [], "prediction_lineage": [],
                "geometry": [{"id": "center", "role": "centerline", "visibility": "visible", "type": "polyline",
                              "points": [[93, 24], [93, 110]]}] if level != "T1" else []}

    def convert(self, exposure=None, level="T2", extra=None):
        _, _, frames = load_session(self.root, "s1")
        annotation = self.annotation(exposure=exposure, level=level)
        if extra:
            annotation.update(extra)
        external = export_canonical(annotation, frames[0]) | {
            "id": 9, "completed_by": "synthetic-annotator", "created_at": annotation["created_at"]}
        self.export = self.base / "export.json"
        write(self.export, [{"id": 1, "data": {"frame_id": "s1:1"}, "annotations": [external]}])
        result = convert_export(self.root, "v1", self.exchange, self.export, "mock-instance", "1")
        self.bid = result["bundle_id"]
        _, bundle = load_bundle(self.root, self.bid)
        self.aid = bundle["annotations"][0]["id"]
        return result

    def approve_fixture(self):
        # Explicitly simulated reviewers. These tests are NEVER evidence of real human review.
        for action in ["reviewed", "approved"]:
            add_review(self.root, self.bid, self.aid, action, "synthetic-reviewer", "synthetic test only", True)

    def test_import_preserves_source_and_exact_png(self):
        result = self.imported()
        self.assertEqual(result["status"], "complete")
        self.assertEqual(self.source_hashes, {p.name: sha(p) for p in self.source.iterdir()})
        self.assertEqual(verify_session(self.root, "s1")["frames"], 4)
        base, _, frames = load_session(self.root, "s1")
        self.assertEqual([f["source_sequence"] for f in frames], [1, 2, 3, 4])
        self.assertEqual(read_png(base / frames[0]["png_path"])[2], pixels(base, frames[0]))
        with self.assertRaises(ValueError):
            import_session(self.source, self.root, True)

    def test_draft_format_requires_explicit_opt_in(self):
        synthetic_session(self.source)
        with self.assertRaisesRegex(ValueError, "not frozen"):
            import_session(self.source, self.root)

    def test_crc_known_vector_and_literal_record_layout(self):
        self.assertEqual(zlib.crc32(b"123456789"), 0xcbf43926)
        wire = scraw.frame(7, 123, b"\x00\x7f\xff")
        self.assertEqual(wire[:8], b"FRM1" + struct.pack("<I", 35))
        self.assertEqual(struct.unpack_from("<QQI", wire, 8), (7, 123, zlib.crc32(b"\x00\x7f\xff")))
        self.assertEqual(struct.unpack("<I", wire[-4:])[0], zlib.crc32(wire[:-4]))

    def test_continuous_segments_and_unlisted_segment_rejection(self):
        meta = synthetic_session(self.source, "s1", 1)
        meta["segments"].append("000001.scraw")
        (self.source / "session.json").write_bytes(encoded(meta))
        (self.source / "000000.scraw").write_bytes(scraw.segment_bytes(scraw.metadata(meta, 0), [(1, 0, b"\0" * 22560)]))
        (self.source / "000001.scraw").write_bytes(scraw.segment_bytes(scraw.metadata(meta, 1), [(2, 20000, b"\1" * 22560)],
            {"capture_frames": 2, "vision_processed_frames": 1, "recorded_frames": 2}))
        self.assertEqual(import_session(self.source, self.root, True)["status"], "complete")
        (self.source / "000002.scraw").write_bytes(b"unlisted")
        with self.assertRaisesRegex(ValueError, "Unlisted"):
            import_session(self.source, self.base / "other-root", True)

    def test_corrupt_record_recovers_later_frame_without_zero_fill(self):
        synthetic_session(self.source, "s1", 4)
        p = self.source / "000000.scraw"
        data = bytearray(p.read_bytes())
        _, _, _, size, _ = scraw.HEADER.unpack(data[:scraw.HEADER.size])
        data[scraw.HEADER.size + size + scraw.PREFIX.size + scraw.FRAME.size + 100] ^= 1
        p.write_bytes(data)
        result = import_session(self.source, self.root, True)
        self.assertEqual(result["status"], "incomplete")
        base, _, frames = load_session(self.root, "s1")
        self.assertEqual([f["source_sequence"] for f in frames], [2, 3, 4])
        report = read(base / "integrity.json")
        self.assertTrue(any(i["kind"] == "record_crc" for i in report["segments"][0]["issues"]))

    def test_truncated_frame_and_footer_are_incomplete(self):
        synthetic_session(self.source, "s1", 4)
        p = self.source / "000000.scraw"
        p.write_bytes(p.read_bytes()[:-180])
        result = import_session(self.source, self.root, True)
        self.assertEqual(result["status"], "incomplete")
        self.assertGreater(result["frames"], 0)
        self.assertLess(result["frames"], 4)

    def test_reordered_duplicate_gap_and_clock_regression_reported(self):
        meta = synthetic_session(self.source, "s1", 1)
        p = self.source / "000000.scraw"
        raw = b"\x01" * 22560
        p.write_bytes(scraw.segment_bytes(scraw.metadata(meta, 0),
                      [(1, 100, raw), (1, 110, raw), (4, 90, raw)], None))
        result = import_session(self.source, self.root, True)
        self.assertEqual(result["frames"], 2)
        report = read(self.root / "sessions/s1/integrity.json")
        kinds = {i["kind"] for i in report["issues"]}
        self.assertTrue({"duplicate_or_reordered", "sequence_gap", "nonmonotonic_timestamp", "missing_session_end"} <= kinds)

    def test_header_geometry_and_identity_are_not_guessed(self):
        meta = synthetic_session(self.source, "s1", 1)
        (self.source / "000000.scraw").write_bytes(scraw.segment_bytes(
            scraw.metadata(meta, 1), [(1, 0, b"\0" * 22560)]))
        with self.assertRaisesRegex(ValueError, "metadata mismatch"):
            import_session(self.source, self.root, True)
        self.assertFalse((self.root / "sessions/s1").exists())

    def test_stride_padding_preserved_but_not_drawn(self):
        meta = synthetic_session(self.source, "s1", 1)
        meta["stride"] = 192
        (self.source / "session.json").write_bytes(encoded(meta))
        raw = (b"\x7f" * 188 + b"\xff" * 4) * 120
        (self.source / "000000.scraw").write_bytes(scraw.segment_bytes(scraw.metadata(meta, 0), [(1, 0, raw)],
            {"capture_frames": 1, "vision_processed_frames": 1, "recorded_frames": 1}))
        self.assertEqual(import_session(self.source, self.root, True)["status"], "complete")
        base, _, frames = load_session(self.root, "s1")
        self.assertEqual(pixels(base, frames[0]), b"\x7f" * 22560)
        self.assertEqual(frames[0]["payload_sha256"], hashlib.sha256(raw).hexdigest())

    def test_tampered_raw_or_preview_is_rejected(self):
        self.imported()
        base, _, frames = load_session(self.root, "s1")
        (base / frames[0]["png_path"]).write_bytes(b"not PNG")
        with self.assertRaisesRegex(ValueError, "PNG hash"):
            verify_session(self.root, "s1")

    def test_external_data_root_and_path_confinement(self):
        for name in ["../outside", "C:/outside", "x/../../z", "a\\b", "http://x"]:
            with self.subTest(name=name), self.assertRaises(ValueError):
                inside(self.base, name)
        with self.assertRaises(ValueError):
            data_root(ROOT / "raw-data")

    def test_sampling_reproducible_refs_and_event_context(self):
        self.imported(6)
        event = {"event_id": "e1", "session_id": "s1", "event_type": "intersection", "start_frame_id": "s1:3",
                 "end_frame_id": "s1:4", "phase_segments": [], "boundary_confidence": "uncertain", "review_status": "draft"}
        a = make_view(self.root, "a", ["s1"], 5, [event], 1)
        b = make_view(self.root, "b", ["s1"], 5, [event], 1)
        self.assertEqual(a["frames"], b["frames"])
        self.assertEqual([f["frame_id"] for f in a["frames"]], [f" s1:{i}".strip() for i in range(1, 7)])
        self.assertEqual(verify_session(self.root, "s1")["frames"], 6)

    def test_split_exact_duplicate_leakage_and_near_candidates(self):
        self.imported()
        source2 = self.base / "source2"
        synthetic_session(source2, "s2", 2)
        import_session(source2, self.root, True)
        with self.assertRaisesRegex(ValueError, "leak"):
            make_split(self.root, "bad", {"s1": "train", "s2": "test"})
        source3 = self.base / "source3"
        synthetic_session(source3, "s3", 2, 1)
        import_session(source3, self.root, True)
        result = make_split(self.root, "good", {"s1": "train", "s3": "test"})
        self.assertTrue(result["near_duplicate_candidates"])
        self.assertTrue(result["layout_split_overlap"])

    def test_annotation_conversion_does_not_approve_and_is_idempotent(self):
        self.imported(); self.tasks(); first = self.convert()
        _, bundle = load_bundle(self.root, self.bid)
        self.assertEqual(bundle["annotations"][0]["status"], "submitted")
        self.assertEqual(bundle["annotations"][0]["geometry"][0]["points"], [[93., 24.], [93., 110.]])
        again = convert_export(self.root, "v1", self.exchange, self.export, "mock-instance", "1")
        self.assertEqual(first["bundle_id"], again["bundle_id"])
        self.assertTrue(again["idempotent"])
        release = create_release(self.root, "no-approval", "v1", "split1", [self.bid])
        self.assertEqual(release["approved_labels"], 0)

    def test_canonical_native_fallback_and_direct_approval_rejection(self):
        self.imported(); self.tasks()
        path = self.base / "human-canonical.json"
        write(path, [self.annotation()])
        result = import_canonical(self.root, "v1", self.exchange, path)
        _, bundle = load_bundle(self.root, result["bundle_id"])
        self.assertEqual(bundle["annotations"][0]["geometry"], self.annotation()["geometry"])
        self.assertTrue(import_canonical(self.root, "v1", self.exchange, path)["idempotent"])
        path = self.base / "fake-approved.json"
        write(path, [self.annotation() | {"status": "approved"}])
        with self.assertRaisesRegex(ValueError, "Only human submissions"):
            import_canonical(self.root, "v1", self.exchange, path)

    def test_sidecar_cannot_silently_relabel_a_different_frame(self):
        self.imported(); self.tasks()
        _, _, frames = load_session(self.root, "s1")
        external = export_canonical(self.annotation(fid="s1:2"), frames[0]) | {
            "id": 1, "completed_by": "synthetic-annotator", "created_at": "2026-11-04T00:00:00Z"}
        path = self.base / "wrong-frame.json"
        write(path, [{"id": 1, "data": {"frame_id": "s1:1"}, "annotations": [external]}])
        with self.assertRaisesRegex(ValueError, "Sidecar Frame ID"):
            convert_export(self.root, "v1", self.exchange, path, "mock", "1")

    def test_review_requires_explicit_separate_actor_and_transition(self):
        self.imported(); self.tasks(); self.convert()
        for action, actor, attestation in [("approved", "reviewer", True), ("reviewed", "synthetic-annotator", True),
                                           ("reviewed", "reviewer", False)]:
            with self.subTest(action=action, actor=actor), self.assertRaises(ValueError):
                add_review(self.root, self.bid, self.aid, action, actor, "test only", attestation)
        self.approve_fixture()
        self.assertEqual(len(review_history(self.root, self.bid)), 2)

    def test_mask_classes_ignore_and_no_unannotated_mask_fabrication(self):
        self.imported(); self.tasks()
        mask = bytes(1 if 37 <= x <= 149 else (255 if x == 0 else 0) for y in range(120) for x in range(188))
        path = self.base / "mask.png"; path.write_bytes(png_bytes(mask, 188, 120))
        self.convert(level="T3", extra={"mask": {"path": "mask.png", "sha256": sha(path), "class_map_version": "track-1"}})
        base, bundle = load_bundle(self.root, self.bid)
        self.assertTrue(bundle["annotations"][0]["mask"])
        fake = {"binary_hex": bytes(255 if 37 <= x <= 149 else 0 for y in range(120) for x in range(188)).hex(),
                "left": [37] * 120, "right": [149] * 120, "center": [93] * 120, "row_flags": [3] * 120}
        _, _, frames = load_session(self.root, "s1")
        metrics = quality_metrics(fake, bundle["annotations"][0], frames[0], base)
        self.assertEqual(metrics["preprocessing_track_iou"], 1.)
        self.assertEqual(metrics["geometry"]["centerline"]["mae_px"], 0.)

    def test_invalid_geometry_and_unknown_mask_classes_rejected(self):
        self.imported()
        _, _, frames = load_session(self.root, "s1")
        for point in [[188, 0], [0, 120], [float("nan"), 50]]:
            bad = self.annotation(); bad["geometry"][0]["points"][0] = point
            with self.assertRaises(ValueError):
                qc(bad, frames[0])
        bad = self.annotation(level="T3")
        p = self.base / "bad.png"; p.write_bytes(png_bytes(b"\x03" * 22560, 188, 120))
        bad["mask"] = {"path": "bad.png", "sha256": sha(p), "class_map_version": "track-1"}
        with self.assertRaises(ValueError):
            qc(bad, frames[0], self.base)

    def test_cancelled_and_unsupported_export_are_not_silent_truth(self):
        self.imported(); self.tasks()
        p = self.base / "cancelled.json"
        write(p, [{"id": 1, "data": {"frame_id": "s1:1"}, "annotations": [{"id": 1, "was_cancelled": True}]}])
        result = convert_export(self.root, "v1", self.exchange, p, "mock", "1")
        self.assertEqual(result["annotations"], 0)
        self.assertEqual(result["skipped"], 1)
        with self.assertRaises(ValueError):
            qc(self.annotation() | {"kind": "prediction", "status": "approved"}, {"frame_id": "s1:1"})

    def test_holdout_blinded_and_exposure_rejected(self):
        self.imported(); self.tasks(split="holdout")
        task = read(self.exchange / "tasks.json")[0]
        self.assertTrue(task["meta"]["blinded"])
        self.assertNotIn("predictions", task)
        with self.assertRaisesRegex(ValueError, "Holdout exposure"):
            self.convert(exposure=["C31"])

    def test_release_lock_and_review_snapshot(self):
        self.imported(); self.tasks(); self.convert(); self.approve_fixture()
        result = create_release(self.root, "r1", "v1", "split1", [self.bid])
        self.assertEqual(result["approved_labels"], 1)
        self.assertEqual(len(load_release(self.root, "r1")[2]), 4)
        path = self.root / "releases/r1/release.json"
        path.write_bytes(path.read_bytes() + b" ")
        with self.assertRaisesRegex(ValueError, "lock"):
            load_release(self.root, "r1")

    def test_tampered_view_split_and_review_are_rejected(self):
        self.imported(); self.tasks(); self.convert(); self.approve_fixture()
        p = self.root / "reviews" / self.bid / "000002.json"
        item = read(p); item["previous_sha256"] = "f" * 64; p.write_bytes(encoded(item))
        with self.assertRaises(ValueError):
            review_history(self.root, self.bid)
        p = self.root / "views/v1/view.json"; p.write_bytes(p.read_bytes() + b" ")
        with self.assertRaises(ValueError):
            load_view(self.root, "v1")
        p = self.root / "splits/split1/split.json"; p.write_bytes(p.read_bytes() + b" ")
        with self.assertRaises(ValueError):
            load_split(self.root, "split1")

    def test_incomplete_session_cannot_publish_release(self):
        synthetic_session(self.source, "s1", 2)
        p = self.source / "000000.scraw"; p.write_bytes(p.read_bytes()[:-1])
        import_session(self.source, self.root, True); self.tasks()
        with self.assertRaisesRegex(ValueError, "Incomplete"):
            create_release(self.root, "bad", "v1", "split1", [])

    def test_mock_timeout_reconciles_without_duplicate_create(self):
        self.imported(); self.tasks()
        tasks = read(self.exchange / "tasks.json")
        transport = MockTransport(timeout_after_create=True)
        result = batch(transport, tasks, {"instance": "mock", "project": "1"}, self.base / "journals", "b1", False)
        self.assertEqual(transport.created, len(tasks))
        self.assertFalse(result["live_api_called"])
        batch(transport, tasks, {"instance": "mock", "project": "1"}, self.base / "journals", "b1", False)
        self.assertEqual(transport.created, len(tasks))
        transport.items[0]["content_sha256"] = "changed"
        with self.assertRaisesRegex(ValueError, "conflict"):
            batch(transport, tasks, {"instance": "mock", "project": "1"}, self.base / "journals", "b1", False)

    def test_mock_dry_run_has_no_remote_mutations(self):
        transport = MockTransport()
        result = batch(transport, [{"data": {"frame_id": "s1:1"}}], {"instance": "mock", "project": "1"},
                       self.base / "journals", "dry")
        self.assertEqual(transport.created, 0)
        self.assertEqual(result["items"][0]["state"], "would_create")

    def test_asset_server_has_only_selected_read_only_images(self):
        self.imported(); self.tasks(every=2)
        server = asset_server(self.exchange, 0)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            client = HTTPConnection("127.0.0.1", server.server_port, timeout=5)
            for method, path, expected in [("GET", "/assets/s1/1.png", 200), ("GET", "/assets/s1/2.png", 404),
                                           ("GET", "/../source/session.json", 404), ("GET", "/", 404),
                                           ("POST", "/assets/s1/1.png", 501)]:
                client.request(method, path)
                response = client.getresponse()
                self.assertEqual(response.status, expected)
                response.read()
            client.close()
        finally:
            server.shutdown(); server.server_close(); thread.join(timeout=5)
        for url in ["http://0.0.0.0:9876", "https://remote.example:8080", "http://token@localhost:8080"]:
            with self.assertRaises(ValueError):
                loopback_base(url)

    def test_schema_strict_types_and_version(self):
        meta = synthetic_session(self.source)
        validate("session", meta)
        for changes in [{"width": True}, {"schema_version": "future"}, {"pixel_format": "RGB"}, {"segments": ["../a.scraw"]}]:
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                validate("session", meta | changes)

    def test_duplicate_json_keys_cannot_hide_session_split_overlap(self):
        for value in ['{"s1":"train","s1":"test"}', '{"value":NaN}', '{"value":Infinity}']:
            with self.assertRaises(ValueError):
                loads(value)

    def test_c31_self_evaluation_is_rejected_before_compile(self):
        self.imported(); self.tasks(); self.convert(exposure=["C31"]); self.approve_fixture()
        create_release(self.root, "r1", "v1", "split1", [self.bid])
        with self.assertRaisesRegex(ValueError, "contamination"):
            evaluate(self.root, "r1", "blocked")

    @unittest.skipUnless(shutil.which("gcc"), "GCC C11 compiler not available")
    def test_end_to_end_real_production_c31_frame_and_sequence(self):
        self.imported(4); self.tasks(); self.convert(); self.approve_fixture()
        create_release(self.root, "r1", "v1", "split1", [self.bid])
        a = evaluate(self.root, "r1", "frame", "frame", 1, 2)
        b = evaluate(self.root, "r1", "sequence", "sequence", 0, 2)
        self.assertEqual(a["frames"], 4); self.assertEqual(b["labelled_frames"], 1)
        self.assertFalse(a["release_eligible"])
        run = read(self.root / "runs/sequence/run.json")
        self.assertEqual(run["sequence_gap_resets"], 0)
        self.assertEqual(run["host_compute_ns"]["count"], 8)
        self.assertTrue(run["synthetic"])
        self.assertEqual(len(read(self.root / "runs/sequence/predictions.json")), 4)
        self.assertEqual(run["classification_event_metrics"].split(":")[0], "NOT_MEASURED")
        assisted = self.base / "assisted"
        prediction_exchange(self.root / "runs/sequence", self.exchange, assisted)
        self.assertEqual(read(assisted / "tasks.json")[0]["meta"]["exposure_algorithms"], ["C31"])
        # Human export cannot erase exposure recorded by the locked task bundle.
        converted = convert_export(self.root, "v1", assisted, self.export, "assisted-mock", "1")
        _, bundle = load_bundle(self.root, converted["bundle_id"])
        self.assertEqual(bundle["annotations"][0]["exposure_algorithms"], ["C31"])

    def test_quality_failure_cannot_be_offset_by_faster_runtime(self):
        common = {"release_sha256": "a" * 64, "parameters": {"mode": "frame"}, "environment_id": "host",
                  "warmup": 1, "repetitions": 2, "frame_ids": ["s1:1"], "toolchain": {"version": "compiler"},
                  "git": {"sha": "a" * 40, "dirty": False}, "centerline_frame_mae_px": {"mean": 1},
                  "row_coverage": {"centerline": {"eligible_rows": 10, "detected_rows": 10}},
                  "host_compute_ns": {"median": 100}}
        b = self.base / "base.json"; c = self.base / "candidate.json"; approval = self.base / "approval.json"
        write(b, common); write(c, common | {"centerline_frame_mae_px": {"mean": 2}, "host_compute_ns": {"median": 20}})
        write(approval, {"reviewer": "synthetic-reviewer", "approved_at": "2026-11-04T00:00:00Z", "run_sha256": sha(b), "git_sha": "a" * 40})
        result = compare(b, c, approval)
        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(result["gates"], {"quality": "FAIL", "coverage": "PASS", "performance": "PASS"})
        self.assertEqual(compare(b, b)["status"], "INCONCLUSIVE")

    def test_cli_offline_import_chain_and_explicit_exit_codes(self):
        self.imported(2)
        completed = subprocess.run([sys.executable, str(ROOT / "tools/vision_data.py"), "verify-session",
                                    "--data-root", str(self.root), "--session-id", "s1"],
                                   capture_output=True, text=True, timeout=30)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(json.loads(completed.stdout)["frames"], 2)


if __name__ == "__main__":
    unittest.main()
