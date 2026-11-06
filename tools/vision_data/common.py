"""Deterministic identities, confined paths and non-overwriting publication."""
from __future__ import annotations

from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import zlib

ROOT = Path(__file__).resolve().parents[2]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def utc():
    return datetime.now(timezone.utc).isoformat()


def encoded(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"),
                      allow_nan=False).encode("utf-8")


def digest(value):
    return hashlib.sha256(encoded(value)).hexdigest()


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read(path):
    return loads(Path(path).read_text(encoding="utf-8"))


def loads(text):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, "Duplicate JSON key")
            result[key] = value
        return result
    def constant(value):
        raise ValueError("Nonfinite JSON number")
    return json.loads(text, object_pairs_hook=pairs, parse_constant=constant)


def timestamp(value):
    require(isinstance(value, str), "Timestamp string required")
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    require(parsed.tzinfo is not None and parsed.utcoffset() is not None, "Timestamp must include timezone")
    return parsed


def write(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("xb") as stream:
        stream.write(encoded(value) + b"\n")


def identifier(value):
    require(isinstance(value, str) and re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,95}", value)
            and value.upper().split(".")[0] not in
            {"CON", "PRN", "AUX", "NUL", *(f"COM{i}" for i in range(10)),
             *(f"LPT{i}" for i in range(10))}, "Unsafe identifier")
    return value


def inside(root, relative):
    root = Path(root).resolve()
    require(isinstance(relative, str) and relative and "\\" not in relative,
            "Expected portable relative path")
    require(not Path(relative).is_absolute() and not any(
        p in ("..", ".", "") or ":" in p for p in relative.split("/")), "Path escape")
    path = root.joinpath(relative).resolve()
    require(path.is_relative_to(root), "Path escapes root (including symlinks)")
    return path


def data_root(path):
    path = Path(path).resolve()
    require(not path.is_relative_to(ROOT), "Data root must be outside the Git repository")
    path.mkdir(parents=True, exist_ok=True)
    return path


@contextmanager
def reservation(target):
    """Exclusive creation, never stale-lock guessing or overwriting prior data."""
    target = Path(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    lock = target.with_name(target.name + ".reserve")
    with lock.open("xb") as lock_file:
        try:
            require(not target.exists(), f"Already exists: {target.name}")
            yield
        finally:
            lock_file.close()  # Windows does not allow unlinking an open file.
            lock.unlink()


def file_ref(path, root):
    path = Path(path)
    return {"path": path.relative_to(root).as_posix(), "sha256": sha(path)}


def checked_ref(ref, root):
    path = inside(root, ref["path"])
    require(sha(path) == ref["sha256"], "Artifact hash mismatch: " + ref["path"])
    return path


def chunk(tag, payload):
    return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", zlib.crc32(tag + payload))


def png_bytes(pixels, width, height):
    require(len(pixels) == width * height, "PNG pixel length")
    rows = b"".join(b"\0" + pixels[y * width:(y + 1) * width] for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def read_png(path):
    """Decode GRAY8 PNG, including all five standard filters; bounded decompression."""
    require(Path(path).stat().st_size <= 64 * 1024 * 1024, "PNG file size bound")
    data = Path(path).read_bytes()
    require(len(data) <= 64 * 1024 * 1024, "PNG file changed/size bound")
    require(data[:8] == b"\x89PNG\r\n\x1a\n", "Not a PNG")
    offset, width, height, packed, ended = 8, None, None, bytearray(), False
    while offset < len(data):
        require(offset + 12 <= len(data), "Truncated PNG chunk")
        size, = struct.unpack_from(">I", data, offset)
        tag = data[offset + 4:offset + 8]
        body = data[offset + 8:offset + 8 + size]
        require(len(body) == size and offset + size + 12 <= len(data), "PNG chunk length")
        crc, = struct.unpack_from(">I", data, offset + 8 + size)
        require(zlib.crc32(tag + body) == crc, "PNG CRC")
        if tag == b"IHDR":
            require(width is None and offset == 8 and size == 13, "PNG IHDR")
            width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", body)
            require(0 < width * height <= 16_777_216 and depth == 8 and color == 0 and
                    compression == filtering == interlace == 0, "Only bounded noninterlaced GRAY8 PNG supported")
        elif tag == b"IDAT":
            packed.extend(body)
        elif tag == b"IEND":
            require(size == 0, "PNG IEND")
            ended = True
        else:
            require(not tag[:1].isupper(), "Unsupported critical PNG chunk")
        offset += size + 12
        if ended:
            break
    require(ended and offset == len(data) and width is not None, "Incomplete/trailing PNG")
    decoder = zlib.decompressobj()
    raw = decoder.decompress(packed, (width + 1) * height + 1)
    require(decoder.eof and not decoder.unused_data and len(raw) == (width + 1) * height,
            "PNG decompressed length")
    output = bytearray()
    prior = bytearray(width)
    for y in range(height):
        mode = raw[y * (width + 1)]
        require(mode <= 4, "PNG filter")
        row = bytearray(raw[y * (width + 1) + 1:(y + 1) * (width + 1)])
        for x in range(width):
            a, b, c = row[x - 1] if x else 0, prior[x], prior[x - 1] if x else 0
            if mode == 1:
                row[x] = (row[x] + a) & 255
            elif mode == 2:
                row[x] = (row[x] + b) & 255
            elif mode == 3:
                row[x] = (row[x] + (a + b) // 2) & 255
            elif mode == 4:
                p = a + b - c
                d = [abs(p - a), abs(p - b), abs(p - c)]
                row[x] = (row[x] + [a, b, c][d.index(min(d))]) & 255
        output.extend(row)
        prior = row
    return width, height, bytes(output)


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)
