"""File-based Prediction exchange and optional strictly local, read-only image view."""
from __future__ import annotations

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import shutil
from urllib.parse import unquote, urlsplit

from .annotations import export_canonical
from .common import inside, read, require, reservation, sha, write
from .contracts import validate


def loopback_base(url):
    parsed = urlsplit(url)
    require(parsed.scheme == "http" and parsed.hostname in {"127.0.0.1", "localhost", "::1"} and
            parsed.port is not None and not parsed.username and not parsed.password and
            parsed.path in {"", "/"} and not parsed.query and not parsed.fragment, "Explicit loopback HTTP base required")
    return url.rstrip("/")


def load_exchange(folder):
    folder = Path(folder)
    lock = read(folder / "exchange.lock.json")
    require(sha(folder / "exchange.json") == lock["exchange_sha256"] and
            sha(folder / "tasks.json") == lock["tasks_sha256"], "Exchange lock mismatch")
    exchange = read(folder / "exchange.json")
    require(read(folder / "tasks.json") == exchange["tasks"], "Task/exchange disagreement")
    for task in exchange["tasks"]:
        path = inside(folder, task["meta"].get("asset_path", task["data"]["image"]))
        require(sha(path) == task["meta"]["png_sha256"], "Exchange image changed")
    return exchange


def prediction_exchange(run_dir, source_exchange, output):
    """Attach separate machine Predictions, recording exposure in the new task bundle."""
    run_dir, source_exchange, output = Path(run_dir), Path(source_exchange), Path(output)
    lock = read(run_dir / "run.lock.json")
    require(sha(run_dir / "run.json") == lock["run_sha256"] and
            sha(run_dir / "predictions.json") == lock["predictions_sha256"], "Prediction run lock")
    run = read(run_dir / "run.json")
    exchange = load_exchange(source_exchange)
    predictions = {p["frame_id"]: p for p in read(run_dir / "predictions.json")}
    count = 0
    for task in exchange["tasks"]:
        fid = task["data"]["frame_id"]
        if fid not in predictions:
            continue
        require(not task["meta"]["blinded"], "Holdout predictions are forbidden")
        prediction = validate("prediction", predictions[fid])
        require(prediction["input_sha256"] == task["meta"]["pixels_sha256"] and
                prediction["algorithm"] == run["algorithm_id"], "Prediction input/algorithm identity")
        frame = {"width": task["meta"]["width"], "height": task["meta"]["height"]}
        projected = export_canonical(prediction, frame)
        task["predictions"] = [{"model_version": run["algorithm_id"] + ":" + run["git"]["sha"],
                                "result": projected["result"], "smartcar_prediction": prediction}]
        task["meta"]["exposure_algorithms"] = sorted(set(task["meta"]["exposure_algorithms"] + [prediction["algorithm"]]))
        task["meta"]["prediction_ids"] = [prediction["id"]]
        count += 1
    require(count > 0, "No matching Predictions")
    exchange["predictions_included"] = True
    exchange["prediction_run_sha256"] = lock["run_sha256"]
    with reservation(output):
        output.mkdir(parents=True)
        for task in exchange["tasks"]:
            relative = task["meta"].get("asset_path", task["data"]["image"])
            dest = inside(output, relative); dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(inside(source_exchange, relative), dest)
        write(output / "tasks.json", exchange["tasks"])
        write(output / "exchange.json", exchange)
        write(output / "exchange.lock.json", {"exchange_sha256": sha(output / "exchange.json"),
                                               "tasks_sha256": sha(output / "tasks.json")})
    return {"predictions": count, "live_api_called": False, "output": str(output)}


def asset_server(folder, port=9876, origin="http://127.0.0.1:8080"):
    """Construct server only; no directory listing, write verbs or arbitrary paths.

    This is an explicit CLI action; ordinary pipeline runs never start a server.
    """
    require(type(port) is int and 0 <= port <= 65535, "Port range")
    origin = loopback_base(origin)
    folder = Path(folder).resolve()
    exchange = load_exchange(folder)
    assets = {"/" + t["meta"].get("asset_path", t["data"]["image"]): t["meta"]["png_sha256"] for t in exchange["tasks"]}
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            self.read_asset(False)

        def do_HEAD(self):
            self.read_asset(True)

        def read_asset(self, head):
            route = urlsplit(self.path)
            relative = unquote(route.path)
            if route.query or relative not in assets:
                self.send_error(404); return
            try:
                path = inside(folder, relative[1:])
                require(sha(path) == assets[relative], "Asset changed")
                content = path.read_bytes()
            except (ValueError, OSError):
                self.send_error(409); return
            self.send_response(200)
            self.send_header("Content-Type", "image/png")
            self.send_header("Content-Length", str(len(content)))
            self.send_header("Access-Control-Allow-Origin", origin)
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            if not head:
                self.wfile.write(content)

        def log_message(self, *args):
            pass  # Do not log request URLs, query strings or authentication material.
    return ThreadingHTTPServer(("127.0.0.1", port), Handler)
