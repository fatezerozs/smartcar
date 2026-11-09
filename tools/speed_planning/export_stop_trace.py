"""Read a frozen stop trace. Never start motion, configure outputs or release data."""
import argparse
import datetime as dt
import json
from pathlib import Path
import re
import struct
import sys
import time
import zlib

VERSION = "PHASE2_STOP_TRACE_20261109_1"
FIELDS = ("ms", "seq", "tl", "tr", "rl", "rr", "pr", "pl", "phase", "reason")
INFO = re.compile(r"^\[STOP_INFO\] gen=(\d+) start_ms=(\d+) stop_ms=(\d+) count=(\d+) recording=(\d+) frozen=(\d+) stop_seen=(\d+) overflow=(\d+) reason=(\d+) crc=([0-9A-Fa-f]{8})$")
ROW = re.compile(r"^\[STOP_ROW\] gen=(\d+) i=(\d+) ms=(\d+) seq=(\d+) tl=(-?\d+) tr=(-?\d+) rl=(-?\d+) rr=(-?\d+) pr=(\d+) pl=(\d+) phase=(\d+) reason=(\d+)$")


def parse_info(line):
    match = INFO.fullmatch(line)
    if not match:
        raise ValueError("Malformed STOP_INFO")
    keys = ("generation", "start_ms", "stop_ms", "count", "recording", "frozen", "stop_seen", "overflow", "reason")
    result = dict(zip(keys, map(int, match.groups()[:9])))
    result["crc32"] = int(match[10], 16)
    if not (2 <= result["count"] <= 640 and result["generation"] > 0):
        raise ValueError("Invalid trace size/generation")
    if result["recording"] or not result["frozen"] or not result["stop_seen"] or result["overflow"]:
        raise ValueError("Trace not frozen/complete, or overflowed")
    return result


def parse_row(line, generation, index):
    match = ROW.fullmatch(line)
    if not match:
        raise ValueError("Malformed STOP_ROW")
    values = list(map(int, match.groups()))
    if values[:2] != [generation, index]:
        raise ValueError("Generation/index mismatch")
    row = dict(zip(FIELDS, values[2:]))
    if not (-32768 <= row["rl"] <= 32767 and -32768 <= row["rr"] <= 32767):
        raise ValueError("Invalid raw counter sample")
    if not (0 <= row["pr"] <= 2000 and 0 <= row["pl"] <= 2000):
        raise ValueError("Submitted PWM exceeds isolated 20% cap")
    return row


def validate_trace(info, rows):
    if len(rows) != info["count"]:
        raise ValueError("Missing rows")
    canonical = b"".join(struct.pack("<10I", *(row[key] & 0xFFFFFFFF for key in FIELDS)) for row in rows)
    if zlib.crc32(canonical) != info["crc32"]:
        raise ValueError("Trace CRC32 mismatch")
    intervals = []
    for before, after in zip(rows, rows[1:]):
        interval = (after["ms"] - before["ms"]) & 0xFFFFFFFF
        sequence_delta = (after["seq"] - before["seq"]) & 0xFFFFFFFF
        if not (0 < interval <= 100 and sequence_delta == 1):
            raise ValueError("Nonconsecutive samples or implausible time gap")
        intervals.append(interval)
    post_duration = (rows[-1]["ms"] - info["stop_ms"]) & 0xFFFFFFFF
    if not 2000 <= post_duration <= 2100:
        raise ValueError("Missing two-second post-zero capture")
    return {"interval_min_ms": min(intervals), "interval_max_ms": max(intervals),
            "post_zero_capture_ms": post_duration, "crc_verified": True,
            "physical_pwm_or_mechanical_stop_verified": False}


class Link:
    def __init__(self, port, raw):
        try:
            import serial
        except ModuleNotFoundError:
            # Workspace-local dependency; do not modify the global Python install.
            sys.path.insert(0, str(Path(__file__).resolve().parents[2] / ".build" / "stop-trace-python-deps"))
            import serial
        self.port = serial.Serial(port, 115200, timeout=0.02, write_timeout=0.3)
        self.raw = raw
        self.pending = bytearray()

    def send(self, text):
        data = text.encode("ascii")
        self.raw.write(json.dumps({"direction": "TX", "monotonic_ns": time.monotonic_ns(), "hex": data.hex()}) + "\n")
        self.raw.flush()
        self.port.write(data)

    def wait_line(self, predicate, timeout=1.5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            chunk = self.port.read(min(max(self.port.in_waiting, 1), 2048))
            if not chunk:
                continue
            self.raw.write(json.dumps({"direction": "RX", "monotonic_ns": time.monotonic_ns(), "hex": chunk.hex()}) + "\n")
            self.raw.flush()
            self.pending.extend(chunk)
            if len(self.pending) > 8192:
                raise ValueError("Unbounded/damaged line")
            while b"\n" in self.pending:
                line, _, rest = self.pending.partition(b"\n")
                self.pending = bytearray(rest)
                try:
                    line = line.rstrip(b"\r").decode("ascii")
                except UnicodeDecodeError:
                    continue
                if "REJECT" in line or "RX_ERROR" in line:
                    raise ValueError("Board rejected query or reported RX error: " + line)
                if predicate(line):
                    return line
        raise TimeoutError("Reply timeout; no movement command was sent")

    def query(self, command, predicate):
        self.send(command + "\r\n")
        return self.wait_line(predicate)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM10")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    args.output.mkdir(parents=True)
    with (args.output / "serial-raw.jsonl").open("x", encoding="utf-8") as raw:
        link = Link(args.port, raw)
        try:
            link.send("!s\r\n")
            time.sleep(0.2)
            link.send("x\r\n")
            link.wait_line(lambda line: "[LINK] port=radio owner=radio " in line if args.port == "COM10" else "[LINK] port=usb owner=usb " in line)
            link.send("?t\r\n")
            link.wait_line(lambda line: "[FIELD] build=" + VERSION + ";" in line)
            info_line = link.query("@stop_info", lambda line: line.startswith("[STOP_INFO] "))
            info = parse_info(info_line)
            rows = []
            for index in range(info["count"]):
                line = link.query(f"@stop_row {index}", lambda line: line.startswith("[STOP_ROW] "))
                rows.append(parse_row(line, info["generation"], index))
            final_info = parse_info(link.query("@stop_info", lambda line: line.startswith("[STOP_INFO] ")))
            if final_info != info:
                raise ValueError("Trace changed while exporting")
            checks = validate_trace(info, rows)
            result = {"schemaVersion": "smartcar.stop-trace/1", "version": VERSION,
                      "exportedAtUtc": dt.datetime.now(dt.timezone.utc).isoformat(),
                      "info": info, "rows": rows, "checks": checks,
                      "board_trace_released": False, "productionApproved": False}
            with (args.output / "trace.json").open("x", encoding="utf-8") as out:
                json.dump(result, out, ensure_ascii=False, indent=2)
                out.write("\n")
            print(json.dumps({"rows": len(rows), "crc_verified": True, "trace": str(args.output / "trace.json")}, ensure_ascii=False))
        finally:
            link.port.close()


if __name__ == "__main__":
    main()
