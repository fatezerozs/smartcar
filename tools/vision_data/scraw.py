"""Provisional PC fixture format SCRAW-DRAFT-1, NEVER an approved firmware format."""
from __future__ import annotations

import struct
import zlib
from .common import encoded, require, loads

MAGIC = b"SCRAWD01"
HEADER = struct.Struct("<8sHHII")
PREFIX = struct.Struct("<4sI")
FRAME = struct.Struct("<QQI")
MAX_PAYLOAD = 16_777_216
FORMAT = "SCRAW-DRAFT-1"


def record(tag, body):
    header = PREFIX.pack(tag, PREFIX.size + len(body) + 4)
    return header + body + struct.pack("<I", zlib.crc32(header + body))


def frame(sequence, timestamp_us, payload):
    return record(b"FRM1", FRAME.pack(sequence, timestamp_us, zlib.crc32(payload)) + payload)


def segment_bytes(metadata, frames, ending=None):
    meta = encoded(metadata)
    head = HEADER.pack(MAGIC, 1, 0, len(meta), zlib.crc32(meta))
    return head + meta + b"".join(frame(*item) for item in frames) + (
        record(b"END1", encoded(ending)) if ending is not None else b"")


def scan(path, expected, report):
    from . import scraw_v1
    with path.open('rb') as stream:
        magic = stream.read(8)
    if magic == scraw_v1.MAGIC:
        yield from scraw_v1.scan(path, expected, report)
        return
    yield from _scan_draft(path, expected, report)


def _scan_draft(path, expected, report):
    """Yield intact records only; byte resynchronization after corruption, no zero fill."""
    import json
    with path.open("rb") as stream:
        header = stream.read(HEADER.size)
        require(len(header) == HEADER.size, "Truncated SCRAW header")
        magic, version, flags, size, crc = HEADER.unpack(header)
        require(magic == MAGIC and version == 1 and flags == 0 and 0 < size <= 65536,
                "Unknown/provisional SCRAW header")
        raw = stream.read(size)
        require(len(raw) == size and zlib.crc32(raw) == crc, "SCRAW metadata CRC/length")
        metadata = loads(raw)
        require(metadata == expected, "Segment identity/geometry metadata mismatch")
        payload_size = expected["stride"] * expected["height"]
        require(0 < payload_size <= MAX_PAYLOAD, "Frame allocation bound")
        ended, resync_start = False, None
        while True:
            offset = stream.tell()
            prefix = stream.read(PREFIX.size)
            if not prefix:
                if resync_start is not None:
                    report["issues"].append({"kind": "unrecoverable_tail", "offset": resync_start})
                break
            if len(prefix) != PREFIX.size:
                report["issues"].append({"kind": "truncated_prefix", "offset": offset})
                break
            tag, length = PREFIX.unpack(prefix)
            reasonable = ((tag == b"FRM1" and length == 12 + FRAME.size + payload_size) or
                          (tag == b"END1" and 14 <= length <= 65548))
            if not reasonable:
                if resync_start is None:
                    resync_start = offset
                stream.seek(offset + 1)
                continue
            tail = stream.read(length - PREFIX.size)
            if len(tail) != length - PREFIX.size:
                report["issues"].append({"kind": "truncated_record", "offset": offset})
                # Do not assume a corrupt length has consumed the rest of the file.
                stream.seek(offset + 1)
                if resync_start is None:
                    resync_start = offset
                continue
            crc, = struct.unpack("<I", tail[-4:])
            if zlib.crc32(prefix + tail[:-4]) != crc:
                report["issues"].append({"kind": "record_crc", "offset": offset})
                if resync_start is None:
                    resync_start = offset
                stream.seek(offset + 1)
                continue
            if resync_start is not None:
                report["issues"].append({"kind": "resynchronized", "start": resync_start, "end": offset})
                resync_start = None
            require(not ended, "Data after END record")
            if tag == b"END1":
                ending = loads(tail[:-4])
                require(isinstance(ending, dict), "END counters object")
                report["ending"] = ending
                ended = True
                continue
            sequence, timestamp, payload_crc = FRAME.unpack(tail[:FRAME.size])
            pixels = tail[FRAME.size:-4]
            if zlib.crc32(pixels) != payload_crc:
                report["issues"].append({"kind": "payload_crc", "offset": offset})
                continue
            yield {"source_sequence": sequence, "timestamp_us": timestamp,
                   "payload_offset": offset + PREFIX.size + FRAME.size,
                   "payload_size": len(pixels), "payload_crc32": payload_crc}, pixels


def metadata(session, segment_index):
    from . import scraw_v1
    if session['format'] == scraw_v1.FORMAT:
        return scraw_v1.expected(session, segment_index)
    return {key: session[key] for key in ("session_id", "width", "height", "stride",
                                         "pixel_format", "timestamp_semantics")} | {
        "segment_index": segment_index, "format": FORMAT}
