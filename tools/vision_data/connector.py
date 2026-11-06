"""Offline/mock batch connector; no HTTP client and no live write capability.

    Timeout reconciliation and conflict handling are exercised with an injected
    transport, never with the user's Label Studio. Tokens are not accepted here.
"""
from __future__ import annotations

from pathlib import Path
from .common import digest, identifier, read, require, utc, write


class MockTransport:
    def __init__(self, items=None, timeout_after_create=False):
        self.items = list(items or [])
        self.timeout_after_create = timeout_after_create
        self.created = 0

    def page(self, offset, limit):
        return {"count": len(self.items), "results": self.items[offset:offset + limit]}

    def create(self, item):
        self.created += 1
        result = item | {"id": len(self.items) + 1}
        self.items.append(result)
        if self.timeout_after_create:
            self.timeout_after_create = False
            raise TimeoutError("mock ambiguous create")
        return result


def inventory(transport):
    all_items, expected, offset = [], None, 0
    while True:
        page = transport.page(offset, 100)
        require(type(page["count"]) is int and page["count"] >= 0, "Pagination count")
        if expected is None:
            expected = page["count"]
        require(page["count"] == expected, "Remote count changed during snapshot")
        items = page["results"]
        require(isinstance(items, list) and len(items) <= 100, "Pagination shape")
        all_items.extend(items); offset += len(items)
        require(offset <= expected, "Pagination exceeds count")
        if offset == expected:
            break
        require(items, "Pagination ended early")
    require(len({item["id"] for item in all_items}) == len(all_items), "Duplicate remote IDs")
    return all_items


def batch(transport, tasks, namespace, journal_root, batch_id, dry_run=True):
    identifier(batch_id)
    require(isinstance(namespace, dict) and set(namespace) == {"instance", "project"}, "Connector namespace")
    identifier(str(namespace["instance"])); identifier(str(namespace["project"]))
    path = Path(journal_root) / identifier(batch_id)
    path.mkdir(parents=True, exist_ok=True)
    input_id = digest({"tasks": tasks, "namespace": namespace})
    if (path / "input.json").exists():
        require(read(path / "input.json")["input_sha256"] == input_id, "Batch identity reused for different input")
    else:
        write(path / "input.json", {"input_sha256": input_id, "namespace": namespace})
    # No writes to a remote unless injected by tests/caller; public CLI only creates a dry-run plan.
    with (path / ".running").open("xb") as lock_file:
        try:
            results = []
            for task in tasks:
                fid = task["data"]["frame_id"]
                payload_hash = digest(task)
                key = digest({"namespace": namespace, "frame_id": fid, "operation": "task", "content": payload_hash})
                matches = [i for i in inventory(transport) if i.get("frame_id") == fid and i.get("namespace") == namespace]
                require(len(matches) <= 1, "Duplicate remote mapping")
                if matches:
                    require(matches[0].get("content_sha256") == payload_hash, "Remote content/human result conflict")
                    state, remote_id = "existing", matches[0]["id"]
                elif dry_run:
                    state, remote_id = "would_create", None
                else:
                    item = {"frame_id": fid, "namespace": namespace, "content_sha256": payload_hash,
                            "idempotency_key": key, "task": task}
                    try:
                        created = transport.create(item)
                    except TimeoutError:
                        # A timeout is ambiguous. Query first; never blind replay.
                        found = [i for i in inventory(transport) if i.get("idempotency_key") == key]
                        require(len(found) == 1, "Ambiguous timeout; manual reconciliation required before retry")
                        created = found[0]
                    state, remote_id = "created_or_reconciled", created["id"]
                result = {"frame_id": fid, "state": state, "remote_id": remote_id,
                          "idempotency_key": key, "content_sha256": payload_hash}
                results.append(result)
                # Append-only per-item checkpoint: all transport errors are deliberately sanitized.
                log_id = len(list(path.glob("checkpoint-*.json"))) + 1
                write(path / f"checkpoint-{log_id:06d}.json", result | {"at": utc()})
            return {"batch_id": batch_id, "dry_run": dry_run, "namespace": namespace,
                    "items": results, "live_api_called": False}
        finally:
            lock_file.close()
            (path / ".running").unlink()
