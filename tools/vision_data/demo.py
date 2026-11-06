"""Synthetic demonstration only: mock annotation/review actors, not actual human GT."""
from pathlib import Path

from .annotations import add_review, convert_export, export_canonical, load_bundle, make_tasks
from .archive import import_session, load_session, synthetic_session, verify_session
from .common import data_root, read, require, reservation, utc, write
from .connector import MockTransport, batch
from .evaluation import compare, evaluate
from .releases import create_release, load_release
from .sampling import make_split, make_view


def demonstration(output, compiler="gcc"):
    output = Path(output).resolve()
    data_root(output.parent)
    with reservation(output):
        output.mkdir()
        root = output / "dataset"
        imports = []
        for sid, count, offset in [("demo-reg", 6, 0), ("demo-holdout", 4, 12)]:
            source = output / "sources" / sid
            synthetic_session(source, sid, count, offset)
            imports.append(import_session(source, root, True))
            verify_session(root, sid)
        make_view(root, "demo-full", ["demo-reg", "demo-holdout"])
        make_split(root, "demo-split", {"demo-reg": "regression", "demo-holdout": "holdout"})
        make_tasks(root, "demo-full", output / "label-exchange", "demo-split")
        tasks = read(output / "label-exchange/tasks.json")
        mock = MockTransport(timeout_after_create=True)
        dry = batch(mock, tasks, {"instance": "synthetic", "project": "demo"}, output / "journals", "dry")
        applied = batch(mock, tasks, {"instance": "synthetic", "project": "demo"}, output / "journals", "mock-only", False)
        again = batch(mock, tasks, {"instance": "synthetic", "project": "demo"}, output / "journals", "mock-only", False)
        require(mock.created == len(tasks), "Mock idempotence failure")
        export = []
        for number, task in enumerate(tasks):
            fid = task["data"]["frame_id"]
            sid = fid.split(":")[0]
            _, _, frames = load_session(root, sid)
            frame = next(f for f in frames if f["frame_id"] == fid)
            offset = 12 if sid == "demo-holdout" else 0
            annotation = {"schema_version": "1.0", "id": "demo-label-" + str(number),
                          "kind": "annotation", "origin": "human", "frame_id": fid, "level": "T2",
                          "status": "submitted", "actor": "synthetic-annotator", "created_at": utc(),
                          "source_tool": "SYNTHETIC-SIMULATION-NOT-HUMAN-GT", "version": 1,
                          "scene": "straight", "environment": ["lighting_normal"], "exposure_algorithms": [],
                          "synthetic_review_simulation": True,
                          "geometry": [{"id": "center", "type": "polyline", "role": "centerline",
                                        "visibility": "visible", "points": [[93 + offset, 24], [93 + offset, 110]]}]}
            external = export_canonical(annotation, frame) | {"id": number + 1,
                       "completed_by": "synthetic-annotator", "created_at": annotation["created_at"]}
            export.append({"id": number + 1, "data": task["data"], "annotations": [external]})
        write(output / "synthetic-export.json", export)
        converted = convert_export(root, "demo-full", output / "label-exchange", output / "synthetic-export.json",
                                   "synthetic", "demo")
        bid = converted["bundle_id"]
        _, bundle = load_bundle(root, bid)
        for annotation in bundle["annotations"]:
            for action in ("reviewed", "approved"):
                add_review(root, bid, annotation["id"], action, "synthetic-reviewer",
                           "SIMULATED review for end-to-end software validation; no human evidence", True)
        released = create_release(root, "demo-reg-v1", "demo-full", "demo-split", [bid])
        held = create_release(root, "demo-holdout-v1", "demo-full", "demo-split", [bid], "holdout")
        load_release(root, held["release_id"])
        frame_run = evaluate(root, "demo-reg-v1", "demo-frame", "frame", 1, 3, compiler)
        sequence_run = evaluate(root, "demo-reg-v1", "demo-sequence", "sequence", 1, 3, compiler)
        observation = compare(root / "runs/demo-frame/run.json", root / "runs/demo-frame/run.json")
        write(output / "comparison-without-baseline-approval.json", observation)
        result = {"schema_version": "1.0", "status": "SYNTHETIC_SOFTWARE_CHAIN_PASS", "synthetic": True,
                  "created_at": utc(), "imports": imports, "mock_dry_run": dry, "mock_apply": applied,
                  "mock_retry": again, "release": released, "holdout": held,
                  "frame_run": frame_run, "sequence_run": sequence_run,
                  "baseline_comparison_status": observation["status"], "real_human_reviews": 0,
                  "real_track_sessions": 0, "live_api_calls": 0, "hardware_accesses": 0,
                  "limits": ["draft SCRAW only", "mock Label Studio exchange", "no actual human ground truth",
                             "no approved performance baseline", "no MCU/vehicle acceptance"]}
        write(output / "demo-report.json", result)
    return {"status": result["status"], "report": str(output / "demo-report.json"),
            "dataset": str(root), "real_track_sessions": 0}
