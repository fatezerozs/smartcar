"""Public, offline-only command line. Never connects to car or Label Studio."""
from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path
import sys

from .annotations import add_review, convert_export, make_tasks, import_canonical
from .archive import import_session, synthetic_session, verify_session
from .common import data_root, read, write
from .connector import MockTransport, batch
from .contracts import validate
from .evaluation import compare, evaluate
from .demo import demonstration
from .exchange import prediction_exchange, asset_server
from .releases import create_release, load_release
from .sampling import make_split, make_view


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    commands = p.add_subparsers(dest="command", required=True)
    def command(name, root=True):
        c = commands.add_parser(name)
        if root:
            c.add_argument("--data-root", required=True, type=Path)
        return c
    c = command("synthetic", False)
    c.add_argument("--output", required=True, type=Path)
    c.add_argument("--session-id", default="synthetic-001")
    c.add_argument("--count", type=int, default=12)
    c.add_argument("--offset", type=int, default=0)
    c = command("demo", False)
    c.add_argument("--output", required=True, type=Path)
    c.add_argument("--compiler", default="gcc")
    c = command("import")
    c.add_argument("--source", required=True, type=Path)
    c.add_argument("--allow-draft", action="store_true")
    c = command("verify-session")
    c.add_argument("--session-id", required=True)
    c = command("continuous")
    c.add_argument("--session-id", required=True)
    c.add_argument("--output", required=True, type=Path)
    c.add_argument("--every", type=int, default=10)
    c = command("view")
    c.add_argument("--view-id", required=True)
    c.add_argument("--sessions", required=True, nargs="+")
    c.add_argument("--every", type=int, default=1)
    c.add_argument("--events", type=Path)
    c.add_argument("--context", type=int, default=2)
    c = command("tasks")
    c.add_argument("--view-id", required=True)
    c.add_argument("--output", required=True, type=Path)
    c.add_argument("--split-id")
    c.add_argument("--image-base")
    c = command("prediction-exchange", False)
    c.add_argument("--run-dir", required=True, type=Path)
    c.add_argument("--exchange", required=True, type=Path)
    c.add_argument("--output", required=True, type=Path)
    c = command("serve", False)
    c.add_argument("--exchange", required=True, type=Path)
    c.add_argument("--port", type=int, default=9876)
    c.add_argument("--origin", default="http://127.0.0.1:8080")
    c = command("convert")
    c.add_argument("--view-id", required=True)
    c.add_argument("--exchange", required=True, type=Path)
    c.add_argument("--export", required=True, type=Path)
    c.add_argument("--instance", required=True)
    c.add_argument("--project", required=True)
    c = command("import-canonical")
    c.add_argument("--view-id", required=True)
    c.add_argument("--exchange", required=True, type=Path)
    c.add_argument("--input", required=True, type=Path)
    c = command("review")
    c.add_argument("--bundle-id", required=True)
    c.add_argument("--annotation-id", required=True)
    c.add_argument("--action", required=True, choices=["reviewed", "approved", "rejected"])
    c.add_argument("--reviewer", required=True)
    c.add_argument("--notes", required=True)
    c.add_argument("--human-attestation", action="store_true")
    c = command("split")
    c.add_argument("--split-id", required=True)
    c.add_argument("--assignments", required=True, type=Path)
    c = command("release")
    c.add_argument("--release-id", required=True)
    c.add_argument("--view-id", required=True)
    c.add_argument("--split-id", required=True)
    c.add_argument("--bundles", nargs="*", default=[])
    c.add_argument("--purpose", default="regression")
    c = command("verify-release")
    c.add_argument("--release-id", required=True)
    c = command("evaluate")
    c.add_argument("--release-id", required=True)
    c.add_argument("--run-id", required=True)
    c.add_argument("--mode", choices=["frame", "sequence"], default="frame")
    c.add_argument("--warmup", type=int, default=1)
    c.add_argument("--repetitions", type=int, default=3)
    c.add_argument("--compiler", default="gcc")
    c = command("compare", False)
    c.add_argument("--baseline", required=True, type=Path)
    c.add_argument("--candidate", required=True, type=Path)
    c.add_argument("--approval", type=Path)
    c.add_argument("--output", required=True, type=Path)
    c.add_argument("--max-mae-increase", type=float, default=0.)
    c.add_argument("--max-time-regression", type=float, default=.05)
    c.add_argument("--noise-fraction", type=float, default=.02)
    c = command("validate", False)
    c.add_argument("--type", required=True)
    c.add_argument("--input", required=True, type=Path)
    c = command("connector-dry-run", False)
    c.add_argument("--tasks", required=True, type=Path)
    c.add_argument("--journal-root", required=True, type=Path)
    c.add_argument("--batch-id", required=True)
    c.add_argument("--instance", required=True)
    c.add_argument("--project", required=True)
    return p


def main(argv=None):
    args = parser().parse_args(argv)
    try:
        root = data_root(args.data_root) if hasattr(args, "data_root") else None
        cmd = args.command
        if cmd == "synthetic":
            # Explicit output must be outside the repo, like every generated data artifact.
            data_root(args.output.parent)
            result = synthetic_session(args.output, args.session_id, args.count, args.offset)
        elif cmd == "demo":
            result = demonstration(args.output, args.compiler)
        elif cmd == "import":
            result = import_session(args.source, root, args.allow_draft)
        elif cmd == "verify-session":
            result = verify_session(root, args.session_id)
        elif cmd == "continuous":
            from .continuous import build_report
            result = build_report(root, args.session_id, args.output, args.every)
        elif cmd == "view":
            result = make_view(root, args.view_id, args.sessions, args.every,
                               read(args.events) if args.events else None, args.context)
        elif cmd == "tasks":
            data_root(args.output.parent)
            result = make_tasks(root, args.view_id, args.output, args.split_id, args.image_base)
        elif cmd == "prediction-exchange":
            data_root(args.output.parent)
            result = prediction_exchange(args.run_dir, args.exchange, args.output)
        elif cmd == "serve":
            with asset_server(args.exchange, args.port, args.origin) as server:
                print("Read-only selected assets at http://127.0.0.1:" + str(server.server_port), flush=True)
                try:
                    server.serve_forever()
                except KeyboardInterrupt:
                    pass
            return 0
        elif cmd == "convert":
            result = convert_export(root, args.view_id, args.exchange, args.export, args.instance, args.project)
        elif cmd == "import-canonical":
            result = import_canonical(root, args.view_id, args.exchange, args.input)
        elif cmd == "review":
            result = add_review(root, args.bundle_id, args.annotation_id, args.action,
                                args.reviewer, args.notes, args.human_attestation)
        elif cmd == "split":
            result = make_split(root, args.split_id, read(args.assignments))
        elif cmd == "release":
            result = create_release(root, args.release_id, args.view_id, args.split_id, args.bundles, args.purpose)
        elif cmd == "verify-release":
            _, release, frames = load_release(root, args.release_id)
            result = {"release_id": args.release_id, "frames": len(frames), "synthetic": release["synthetic"], "verified": True}
        elif cmd == "evaluate":
            result = evaluate(root, args.release_id, args.run_id, args.mode, args.warmup, args.repetitions, args.compiler)
        elif cmd == "compare":
            result = compare(args.baseline, args.candidate, args.approval, args.max_mae_increase,
                             args.max_time_regression, args.noise_fraction)
            write(args.output, result)
        elif cmd == "validate":
            validate(args.type, read(args.input)); result = {"schema": args.type, "valid": True}
        elif cmd == "connector-dry-run":
            data_root(args.journal_root)
            result = batch(MockTransport(), read(args.tasks), {"instance": args.instance, "project": args.project},
                           args.journal_root, args.batch_id, dry_run=True)
        else:
            raise ValueError("Unknown command")
        print(json.dumps(result, ensure_ascii=False, sort_keys=True, allow_nan=False))
        if cmd == "compare" and result["status"] != "PASS":
            return 1
        if cmd in ("import", "continuous") and result["status"] != "complete":
            return 1
        if cmd == "verify-session" and result["integrity"] != "complete":
            return 1
        return 0
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError, subprocess.SubprocessError) as error:
        # Do not echo arbitrary annotation text, API exceptions or secret-bearing URLs.
        print(json.dumps({"status": "ERROR", "type": type(error).__name__,
                          "message": str(error) if isinstance(error, ValueError) else "Input or filesystem error; inspect local artifacts"}), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
