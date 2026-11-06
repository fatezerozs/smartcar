"""SCRAW-1 training producer adapter; independent of the firmware serializer.

Header CRC is checked before trusting allocation sizes. Original bytes are never
rewritten. Resynchronization accepts only full header + payload + record CRCs.
"""
from __future__ import annotations
import struct
import zlib
from .common import loads, require

FORMAT = 'SCRAW-1'
MAGIC = b'SCRAW1\r\n'
HEADER = struct.Struct('<8sHHHHIHHHHBBH16sIIII')
RECORD = struct.Struct('<4sIQQIIII')
MAX_META = 4096
MAX_FRAME = 16_777_216


def header(stream):
    raw = stream.read(HEADER.size)
    require(len(raw) == 64 and zlib.crc32(raw[:60]) == int.from_bytes(raw[60:], 'little'),
            'SCRAW-1 header CRC/length')
    (magic, major, minor, size, flags, index, width, height, stride, pixels,
     unit, event, reserved, uuid, n, crc, reserved2, _) = HEADER.unpack(raw)
    require((magic, major, minor, size, flags, reserved, reserved2) == (MAGIC, 1, 0, 64, 0, 0, 0),
            'Unsupported SCRAW-1 header/version/flags')
    require(0 < width <= stride and height > 0 and stride * height <= MAX_FRAME and
            pixels == 1 and unit == 1 and event in (1, 2) and 0 < n <= MAX_META,
            'SCRAW-1 geometry/metadata bound')
    metadata = stream.read(n)
    require(len(metadata) == n and zlib.crc32(metadata) == crc, 'SCRAW-1 metadata CRC')
    meta = loads(metadata)
    require(isinstance(meta, dict) and meta.get('format') == FORMAT, 'SCRAW-1 metadata format')
    require((meta.get('width'), meta.get('height'), meta.get('stride'), meta.get('pixel_format')) ==
            (width, height, stride, 'GRAY8'), 'Binary/JSON geometry mismatch')
    require(meta.get('timestamp_unit') == 'us' and
            meta.get('session_uuid_hex') == uuid.hex(), 'SCRAW-1 time/UUID mismatch')
    require(type(meta.get('timestamp_resolution_us')) is int and meta['timestamp_resolution_us'] > 0,
            'Timestamp resolution required')
    require((event == 1 and meta.get('timestamp_semantics') == 'csi_irq_gpt1_ms_x1000') or
            (event == 2 and meta.get('timestamp_semantics') == 'completion_callback_us'),
            'Binary/JSON timestamp event mismatch')
    return meta, index


def discover(source):
    """Read-only fallback for a lost final session.json. Gaps are preserved."""
    paths = sorted(source.glob('*.scraw'))
    require(paths, 'No SCRAW segments')
    common, names = None, []
    for path in paths:
        with path.open('rb') as stream:
            meta, index = header(stream)
        require(path.name == f'{index:06d}.scraw', 'Segment filename/header index mismatch')
        if common is None: common = meta
        require(common == meta, 'Mixed session identities/configuration')
        names.append(path.name)
    return dict(common, segments=names)


def expected(meta, index):
    return {'metadata': {k: v for k, v in meta.items() if k != 'segments'}, 'index': index}


def scan(path, identity, report):
    with path.open('rb') as stream:
        meta, index = header(stream)
        require(meta == identity['metadata'] and index == identity['index'], 'SCRAW-1 segment identity mismatch')
        size = meta['stride'] * meta['height']
        ended, resync_start = False, None
        report['gaps'] = []
        while True:
            offset = stream.tell()
            raw = stream.read(RECORD.size)
            if not raw:
                if resync_start is not None:
                    report['issues'].append({'kind': 'unrecoverable_tail', 'offset': resync_start})
                break
            if len(raw) != RECORD.size:
                report['issues'].append({'kind': 'truncated_record_header', 'offset': offset})
                break
            tag, length, seq, at, n, crc, flags, hc = RECORD.unpack(raw)
            reasonable = (tag == b'FRM1' and n == size and flags == 0) or (
                tag in (b'GAP1', b'END1') and 2 <= n <= (4096 if tag == b'GAP1' else 16384) and
                flags == 1 and seq == 0 and at == 0)
            if not reasonable or length != 44 + n or zlib.crc32(raw[:36]) != hc:
                if resync_start is None: resync_start = offset
                stream.seek(offset + 1)
                continue
            payload, tail = stream.read(n), stream.read(4)
            if len(payload) != n or len(tail) != 4:
                report['issues'].append({'kind': 'truncated_record', 'offset': offset})
                if resync_start is None: resync_start = offset
                stream.seek(offset + 1)
                continue
            if zlib.crc32(payload) != crc or zlib.crc32(payload, zlib.crc32(raw)) != int.from_bytes(tail, 'little'):
                report['issues'].append({'kind': 'record_crc', 'offset': offset})
                if resync_start is None: resync_start = offset
                stream.seek(offset + 1)
                continue
            require(not ended, 'Data after END record')
            if resync_start is not None:
                report['issues'].append({'kind': 'resynchronized', 'start': resync_start, 'end': offset})
                resync_start = None
            if tag != b'FRM1':
                obj = loads(payload)
                require(isinstance(obj, dict), 'SCRAW-1 control record must be object')
                if tag == b'END1': report['ending'], ended = obj, True
                else: report['gaps'].append(obj)
                continue
            yield {'source_sequence': seq, 'timestamp_us': at, 'payload_offset': offset + 40,
                   'payload_size': n, 'payload_crc32': crc}, payload
