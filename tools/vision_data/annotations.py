"""Offline Label Studio exchange, canonical QC and explicit human review ledger."""
from __future__ import annotations

import copy
import hashlib
from pathlib import Path
import shutil

from .common import (checked_ref, digest, encoded, file_ref, finite, identifier, inside, read,
                     read_png, require, reservation, sha, utc, write, timestamp, loads)
from .sampling import SCENES, load_view, load_split
from .contracts import validate

ROLES = {"left_boundary", "right_boundary", "centerline", "inner_island", "outer_boundary",
         "entry_boundary", "exit_boundary", "junction_branch", "custom"}
VISIBILITY = {"visible", "partial", "occluded", "out_of_frame", "ambiguous"}
ENVIRONMENT = {f"{k}_{v}" for k, values in {
    "lighting": ["normal", "bright", "dim"], "shadow": ["none", "partial", "strong"],
    "contrast": ["high", "medium", "low"], "surface": ["clean", "worn", "reflective"],
    "blur": ["none", "mild", "severe"]}.items() for v in values}


def qc(annotation, frame, asset_root=None):
    validate(annotation.get("kind", "annotation"), annotation)
    require(annotation["schema_version"] == "1.0" and annotation["kind"] in {"annotation", "prediction"},
            "Canonical version/kind")
    identifier(annotation["id"])
    require(annotation["frame_id"] == frame["frame_id"], "Annotation frame mismatch")
    require(annotation["level"] in {"T1", "T2", "T3"}, "Annotation level")
    require(annotation["scene"] in SCENES and set(annotation.get("environment", [])).issubset(ENVIRONMENT),
            "Scene/environment vocabulary")
    require(annotation["status"] in {"draft", "submitted", "reviewed", "approved", "rejected"}, "Annotation status")
    require(annotation["actor"] and annotation["created_at"] and annotation["source_tool"], "Annotation provenance")
    timestamp(annotation["created_at"])
    if annotation.get("updated_at"):
        timestamp(annotation["updated_at"])
    require(type(annotation["version"]) is int and annotation["version"] > 0, "Annotation revision")
    require(isinstance(annotation.get("exposure_algorithms", []), list) and
            all(isinstance(a, str) and a for a in annotation.get("exposure_algorithms", [])), "Exposure provenance")
    if annotation["kind"] == "prediction":
        require(annotation["status"] == "draft" and annotation["algorithm"] and annotation["input_sha256"]
                and annotation["configuration"] is not None, "Prediction must remain a sourced machine draft")
    else:
        require(annotation.get("origin") == "human", "Human origin must be declared")
    object_ids = set()
    for obj in annotation.get("geometry", []):
        require(obj["id"] not in object_ids and obj["role"] in ROLES and obj["visibility"] in VISIBILITY,
                "Geometry identity/role/visibility")
        object_ids.add(obj["id"])
        require(obj["type"] in {"polyline", "polygon", "keypoint"}, "Geometry type")
        minimum = {"polyline": 2, "polygon": 3, "keypoint": 1}[obj["type"]]
        require(len(obj["points"]) >= minimum and
                (obj["type"] != "keypoint" or len(obj["points"]) == 1), "Geometry point count")
        if obj["role"] == "custom":
            require(bool(obj.get("description")), "Custom geometry requires description")
        for point in obj["points"]:
            require(len(point) == 2 and all(finite(v) for v in point) and
                    0 <= point[0] < frame["width"] and 0 <= point[1] < frame["height"], "Out-of-bounds geometry")
    if annotation["level"] == "T1":
        require(not annotation.get("geometry") and not annotation.get("mask"), "T1 geometry/mask mismatch")
    if annotation["level"] != "T1" and not annotation.get("geometry"):
        require(annotation.get("geometry_state") in {"unannotated", "explicit_empty"},
                "Empty geometry requires an explicit annotation state")
    if annotation.get("mask") is not None:
        require(annotation["level"] == "T3" and asset_root is not None, "Mask requires T3 and asset root")
        mask = annotation["mask"]
        require(mask["class_map_version"] == "track-1", "Unknown mask class map")
        w, h, raw = read_png(checked_ref(mask, asset_root))
        require((w, h) == (frame["width"], frame["height"]) and set(raw) <= {0, 1, 2, 255}, "Mask geometry/classes")
    if annotation.get("topology") is not None:
        require(annotation["level"] == "T3", "Topology requires T3")
        topology = annotation["topology"]
        require(topology["scope"] == "image_observation", "Route intent is not visual ground truth")
        nodes = [n["id"] for n in topology["nodes"]]
        require(len(set(nodes)) == len(nodes), "Duplicate topology node")
        require(all(e["from"] in nodes and e["to"] in nodes for e in topology["edges"]), "Unknown topology node")
    return annotation


def make_tasks(root, view_id, output, split_id=None, image_base=None):
    if image_base:
        from .exchange import loopback_base
        image_base = loopback_base(image_base)
    view, frames = load_view(root, view_id)
    roles = {}
    split_sha = None
    if split_id:
        split_path, split = load_split(root, split_id)
        split_sha = sha(split_path)
        roles = {item["session_id"]: item["split"] for item in split["sessions"]}
    tasks = []
    for item in view["frames"]:
        base, f = frames[item["frame_id"]]
        require(not split_id or f["session_id"] in roles, "View Session missing from split")
        asset_path = f"assets/{f['session_id']}/{f['source_sequence']}.png"
        tasks.append({"data": {"image": image_base + "/" + asset_path if image_base else asset_path,
                               "frame_id": f["frame_id"]},
                      "meta": {"frame_id": f["frame_id"], "width": f["width"], "height": f["height"],
                               "png_sha256": f["png_sha256"], "pixels_sha256": f["pixels_sha256"],
                               "asset_path": asset_path, "context": item,
                               "view_id": view_id, "split": roles.get(f["session_id"]),
                               "blinded": roles.get(f["session_id"]) == "holdout",
                               "transform": {"scale_x": 1, "scale_y": 1, "offset_x": 0, "offset_y": 0},
                               "exposure_algorithms": []}})
    result = {"schema_version": "1.0", "view_sha256": digest(view), "split_sha256": split_sha,
              "tasks": tasks, "predictions_included": False,
              "image_uri_policy": "relative offline bundle; live local-files mapping must be separately configured"}
    output = Path(output)
    with reservation(output):
        output.mkdir(parents=True)
        for task in tasks:
            base, f = frames[task["data"]["frame_id"]]
            png = checked_ref({"path": f["png_path"], "sha256": f["png_sha256"]}, base)
            asset = inside(output, task["meta"]["asset_path"])
            asset.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(png, asset)
            require(sha(asset) == f["png_sha256"], "Task image copy mismatch")
        write(output / "tasks.json", tasks)
        write(output / "exchange.json", result)
        write(output / "exchange.lock.json", {"exchange_sha256": sha(output / "exchange.json"),
                                                "tasks_sha256": sha(output / "tasks.json")})
    return {"frames": len(tasks), "directory": str(output), "live_api_called": False}


def export_canonical(annotation, frame):
    """Lossless JSON sidecar plus LS Choices/ordered KeyPointLabels projection.

    Open polylines are NOT mislabeled as closed polygons. Ordered point groups
    round-trip through our adapter metadata. Native LS brush RLE is not guessed.
    """
    result = [{"id": "scene", "from_name": "scene", "to_name": "image", "type": "choices",
               "value": {"choices": [annotation["scene"]]}}]
    if annotation.get("environment"):
        result.append({"id": "environment", "from_name": "environment", "to_name": "image",
                       "type": "choices", "value": {"choices": annotation["environment"]}})
    for obj in annotation.get("geometry", []):
        for index, (x, y) in enumerate(obj["points"]):
            result.append({"id": f"p{len(result)}", "from_name": "geometry", "to_name": "image",
                           "type": "keypointlabels", "original_width": frame["width"],
                           "original_height": frame["height"],
                           "value": {"x": x / frame["width"] * 100, "y": y / frame["height"] * 100,
                                     "width": .5, "keypointlabels": [obj["role"]]},
                           "meta": {"smartcar_object": obj["id"], "vertex": index,
                                    "object": {k: v for k, v in obj.items() if k != "points"}}})
    # The sidecar retains masks/topology/visibility, IDs, and non-UI metadata.
    return {"result": result, "smartcar_canonical": copy.deepcopy(annotation)}


def convert_export(root, view_id, exchange_dir, export_path, instance, project):
    """Never trust LS completion/approved flags as approval; preserve export bytes."""
    identifier(instance); identifier(str(project))
    exchange_dir = Path(exchange_dir)
    from .exchange import load_exchange
    exchange = load_exchange(exchange_dir)
    lock = read(exchange_dir / "exchange.lock.json")
    view, frames = load_view(root, view_id)
    require(exchange["view_sha256"] == digest(view), "Export uses different sampling view")
    tasks = {t["data"]["frame_id"]: t for t in exchange["tasks"]}
    source_bytes = Path(export_path).read_bytes()
    source_sha256 = hashlib.sha256(source_bytes).hexdigest()
    exported = loads(source_bytes)
    require(isinstance(exported, list), "Expected Label Studio export list")
    bundle_id = digest({"view": digest(view), "exchange": lock, "export": source_sha256,
                        "instance": instance, "project": str(project)})
    target = inside(root, "annotations/" + bundle_id)
    if target.exists():
        load_bundle(root, bundle_id)
        return {"bundle_id": bundle_id, "idempotent": True}
    converted, skipped, ids, assets = [], [], set(), {}
    for task in exported:
        fid = task["data"]["frame_id"]
        require(fid in tasks and fid in frames, "Unknown exported frame")
        source_task = tasks[fid]
        require("image" not in task["data"] or task["data"]["image"] == source_task["data"]["image"],
                "Export task image identity mismatch")
        _, frame = frames[fid]
        t = source_task["meta"]["transform"]
        require(all(finite(t[k]) for k in ("scale_x", "scale_y", "offset_x", "offset_y")) and
                t["scale_x"] > 0 and t["scale_y"] > 0, "Invalid inverse transform")
        for external in task.get("annotations", []):
            key = f"{instance}:{project}:{task['id']}:{external['id']}"
            require(key not in ids, "Duplicate external annotation identity")
            ids.add(key)
            if external.get("was_cancelled") or external.get("draft"):
                skipped.append({"external_id": key, "reason": "cancelled_or_draft"})
                continue
            require(external.get("completed_by") is not None and external.get("created_at"),
                    "Missing declared human annotator/time")
            canonical = copy.deepcopy(external.get("smartcar_canonical", {}))
            require("frame_id" not in canonical or canonical["frame_id"] == fid, "Sidecar Frame ID mismatch")
            require(canonical.get("kind") != "prediction", "Prediction cannot be imported as human Annotation")
            objects, scene, environment = {}, None, []
            for result in external.get("result", []):
                value = result["value"]
                if result["type"] == "choices" and result["from_name"] == "scene":
                    require(scene is None and len(value["choices"]) == 1, "One scene required")
                    scene = value["choices"][0]
                elif result["type"] == "choices" and result["from_name"] == "environment":
                    environment = value["choices"]
                elif result["type"] == "keypointlabels" and result["from_name"] == "geometry":
                    require(len(value["keypointlabels"]) == 1 and
                            result["original_width"] == frame["width"] * t["scale_x"] and
                            result["original_height"] == frame["height"] * t["scale_y"], "Keypoint dimensions/role")
                    m = result.get("meta", {})
                    # Plain new points are independent keypoints; never guess a polyline ordering.
                    oid = m.get("smartcar_object", "keypoint-" + str(result["id"]))
                    objmeta = m.get("object", {"id": oid, "type": "keypoint", "role": value["keypointlabels"][0],
                                               "visibility": "visible"})
                    require(objmeta["id"] == oid and objmeta["role"] == value["keypointlabels"][0], "Object identity/role mismatch")
                    obj = objects.setdefault(oid, {"metadata": objmeta, "vertices": {}})
                    require(obj["metadata"] == objmeta, "Conflicting object metadata")
                    n = m.get("vertex", 0)
                    require(type(n) is int and n >= 0 and n not in obj["vertices"], "Duplicate vertex")
                    obj["vertices"][n] = [
                        (value["x"] * result["original_width"] / 100 - t["offset_x"]) / t["scale_x"],
                        (value["y"] * result["original_height"] / 100 - t["offset_y"]) / t["scale_y"]]
                else:
                    raise ValueError("Unsupported Label Studio result; retained source is required, no lossy conversion: " + result["type"])
            geometry = []
            originals = {o["id"]: o for o in canonical.get("geometry", [])}
            require(not originals or set(originals) <= set(objects),
                    "Geometry grouping lost: explicit canonical revision required, no inferred polyline")
            for obj in objects.values():
                require(sorted(obj["vertices"]) == list(range(len(obj["vertices"]))), "Missing polyline vertex")
                converted_object = obj["metadata"] | {"points": [obj["vertices"][n] for n in sorted(obj["vertices"])]}
                original = originals.get(converted_object["id"])
                # Percent-coordinate round trips introduce floating-point noise.
                # Preserve exact sidecar coordinates only for unchanged projections.
                if (original and len(original["points"]) == len(converted_object["points"]) and
                    all(abs(a - b) <= 1e-10 for p, q in zip(original["points"], converted_object["points"])
                        for a, b in zip(p, q))):
                    converted_object["points"] = copy.deepcopy(original["points"])
                geometry.append(converted_object)
            require(scene is not None, "Explicit scene required (empty is not background)")
            canonical.update(schema_version="1.0", kind="annotation", id="ann-" + digest(key)[:32], frame_id=fid,
                             status="submitted", origin="human", actor=str(external["completed_by"]),
                             created_at=external["created_at"], source_tool="label-studio-file",
                             source_tool_version=external.get("source_tool_version", "unknown"),
                             version=int(canonical.get("version", 1)), scene=scene,
                             environment=environment, geometry=geometry, external_id=key,
                             prediction_lineage=sorted(set(source_task["meta"].get("prediction_ids", []) +
                                 external.get("prediction_lineage", canonical.get("prediction_lineage", [])))),
                             exposure_algorithms=sorted(set(source_task["meta"]["exposure_algorithms"] +
                                                            external.get("exposure_algorithms", []) +
                                                            canonical.get("exposure_algorithms", []))))
            canonical["updated_at"] = external.get("updated_at", canonical["created_at"])
            canonical["review_status"] = "unreviewed"
            require(not source_task["meta"]["blinded"] or not canonical["exposure_algorithms"], "Holdout exposure")
            canonical.setdefault("level", "T2" if geometry else "T1")
            qc(canonical, frame, Path(export_path).parent)
            if canonical.get("mask"):
                ref = canonical["mask"]
                asset = checked_ref(ref, Path(export_path).parent)
                name = ref["sha256"] + ".png"
                assets[name] = asset
                canonical["mask"] = ref | {"path": "masks/" + name}
            converted.append(canonical)
    result = {"schema_version": "1.0", "bundle_id": bundle_id, "view_id": view_id,
              "view_sha256": digest(view), "source_sha256": source_sha256, "exchange_lock": lock,
              "instance_id": instance, "project_id": str(project), "annotations": converted,
              "skipped": skipped, "review_policy": "submission is never approval"}
    with reservation(target):
        target.mkdir()
        (target / "original-export.json").write_bytes(source_bytes)
        require(sha(export_path) == source_sha256, "Export changed during conversion")
        for name, asset in assets.items():
            (target / "masks").mkdir(exist_ok=True)
            shutil.copyfile(asset, target / "masks" / name)
            require(sha(target / "masks" / name) == name[:-4], "Mask changed during copy")
        write(target / "canonical.json", result)
        write(target / "bundle.lock.json", {"canonical_sha256": sha(target / "canonical.json"),
                                              "export_sha256": sha(target / "original-export.json")})
    return {"bundle_id": bundle_id, "annotations": len(converted), "skipped": len(skipped)}


def load_bundle(root, bundle_id):
    identifier(bundle_id)
    base = inside(root, "annotations/" + bundle_id)
    lock = read(base / "bundle.lock.json")
    require(sha(base / "canonical.json") == lock["canonical_sha256"] and
            sha(base / "original-export.json") == lock["export_sha256"], "Annotation bundle changed")
    bundle = read(base / "canonical.json")
    view, frames = load_view(root, bundle["view_id"])
    require(bundle["view_sha256"] == digest(view) and bundle["bundle_id"] == bundle_id, "Annotation source identity")
    for annotation in bundle["annotations"]:
        qc(annotation, frames[annotation["frame_id"]][1], base)
        require(annotation["status"] == "submitted" and annotation["kind"] == "annotation", "Bundle is not human submission")
    return base, bundle


def import_canonical(root, view_id, exchange_dir, input_path):
    """Native file fallback for polylines, Masks and topology, without a live UI."""
    from .exchange import load_exchange
    exchange = load_exchange(exchange_dir)
    view, frames = load_view(root, view_id)
    require(exchange["view_sha256"] == digest(view), "Canonical exchange/view mismatch")
    tasks = {t["data"]["frame_id"]: t for t in exchange["tasks"]}
    source_bytes = Path(input_path).read_bytes()
    original_sha = hashlib.sha256(source_bytes).hexdigest()
    items = loads(source_bytes)
    require(isinstance(items, list), "Canonical submission list required")
    require(len({a["id"] for a in items}) == len(items), "Duplicate Annotation ID")
    bid = digest({"source": original_sha, "exchange": sha(Path(exchange_dir) / "exchange.json"), "format": "canonical-file"})
    target = inside(root, "annotations/" + bid)
    if target.exists():
        load_bundle(root, bid)
        return {"bundle_id": bid, "idempotent": True}
    converted, assets = [], {}
    for source in items:
        a = copy.deepcopy(source)
        a.setdefault("source_tool_version", "unknown")
        a.setdefault("updated_at", a["created_at"])
        a["review_status"] = "unreviewed"
        require(a["kind"] == "annotation" and a["status"] == "submitted", "Only human submissions, never Prediction/approval")
        task = tasks[a["frame_id"]]
        a["exposure_algorithms"] = sorted(set(a.get("exposure_algorithms", []) + task["meta"]["exposure_algorithms"]))
        a["prediction_lineage"] = sorted(set(a.get("prediction_lineage", []) + task["meta"].get("prediction_ids", [])))
        require(not task["meta"]["blinded"] or not (a["exposure_algorithms"] or a["prediction_lineage"]), "Holdout exposure")
        qc(a, frames[a["frame_id"]][1], Path(input_path).parent)
        if a.get("mask"):
            asset = checked_ref(a["mask"], Path(input_path).parent)
            name = a["mask"]["sha256"] + ".png"
            assets[name] = asset
            a["mask"]["path"] = "masks/" + name
        converted.append(a)
    bundle = {"schema_version": "1.0", "bundle_id": bid, "view_id": view_id, "view_sha256": digest(view),
              "source_sha256": original_sha, "source_format": "canonical-file",
              "exchange_lock": read(Path(exchange_dir) / "exchange.lock.json"), "annotations": converted,
              "skipped": [], "review_policy": "submission is never approval"}
    with reservation(target):
        target.mkdir()
        (target / "original-export.json").write_bytes(source_bytes)
        require(sha(input_path) == original_sha, "Canonical input changed during conversion")
        for name, asset in assets.items():
            (target / "masks").mkdir(exist_ok=True)
            shutil.copyfile(asset, target / "masks" / name)
            require(sha(target / "masks" / name) == name[:-4], "Mask changed during copy")
        write(target / "canonical.json", bundle)
        write(target / "bundle.lock.json", {"canonical_sha256": sha(target / "canonical.json"),
                                              "export_sha256": sha(target / "original-export.json")})
    return {"bundle_id": bid, "annotations": len(converted)}


def review_history(root, bundle_id):
    folder = inside(root, "reviews/" + identifier(bundle_id))
    _, bundle = load_bundle(root, bundle_id)
    annotations = {a["id"]: a for a in bundle["annotations"]}
    history, previous, states = [], None, {}
    if folder.exists():
        for index, path in enumerate(sorted(folder.glob("*.json"))):
            item = read(path)
            validate("review", item)
            timestamp(item["at"])
            require(path.name == f"{index + 1:06d}.json" and item["previous_sha256"] == previous
                    and item["revision"] == index + 1 and item["bundle_id"] == bundle_id,
                    "Broken review ledger")
            require(item["annotation_id"] in annotations, "Review references unknown Annotation")
            annotation = annotations[item["annotation_id"]]
            require(item["annotation_sha256"] == digest(annotation) and item["reviewer"] != annotation["actor"],
                    "Review source/reviewer mismatch")
            status = states.get(item["annotation_id"], "submitted")
            require((status == "submitted" and item["action"] in {"reviewed", "rejected"}) or
                    (status == "reviewed" and item["action"] in {"approved", "rejected"}), "Invalid review history")
            states[item["annotation_id"]] = item["action"]
            previous = sha(path)
            history.append(item)
    return history


def add_review(root, bundle_id, annotation_id, action, reviewer, notes, human_attestation=False):
    require(human_attestation and reviewer and notes, "Explicit human attestation, reviewer and notes required")
    base, bundle = load_bundle(root, bundle_id)
    matches = [a for a in bundle["annotations"] if a["id"] == annotation_id]
    require(len(matches) == 1, "Unknown/duplicate annotation")
    require(action in {"reviewed", "approved", "rejected"}, "Review action")
    folder = inside(root, "reviews/" + bundle_id)
    folder.mkdir(parents=True, exist_ok=True)
    with (folder / ".writing").open("xb") as lock_file:
        try:
            history = review_history(root, bundle_id)
            prior = [h for h in history if h["annotation_id"] == annotation_id]
            status = prior[-1]["action"] if prior else "submitted"
            require((status == "submitted" and action in {"reviewed", "rejected"}) or
                    (status == "reviewed" and action in {"approved", "rejected"}), "Illegal review transition")
            require(reviewer != matches[0]["actor"], "Reviewer must differ from annotator")
            item = {"schema_version": "1.0", "bundle_id": bundle_id, "annotation_id": annotation_id,
                    "annotation_sha256": digest(matches[0]), "revision": len(history) + 1,
                    "previous_sha256": sha(folder / f"{len(history):06d}.json") if history else None,
                    "action": action, "reviewer": reviewer, "notes": notes, "at": utc(),
                    "human_attestation": True, "identity_authentication": "operator-declared; not cryptographically authenticated"}
            validate("review", item)
            write(folder / f"{len(history) + 1:06d}.json", item)
        finally:
            lock_file.close()
            (folder / ".writing").unlink()
    return item
