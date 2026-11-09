"""Collect a static timing trace from the dedicated zero-PWM firmware only."""
import argparse
import json
from pathlib import Path
import re
import struct
import sys
import time

VERSION = "PHASE2_LATENCY_STATIC_20261109_1"
FIELDS = ("seq", "cap", "copy", "process", "end", "observed", "flags", "drops")


def validate(rows, info):
    if int(info["active"]) or int(info["reason"]) != 1 or len(rows) != 256:
        raise ValueError("Incomplete trace; retain raw log, do not report complete statistics")
    checksum = 2166136261
    for index, row in enumerate(rows):
        if row["i"] != index or row["gen"] != int(info["gen"]):
            raise ValueError("Row order or generation mismatch")
        if row["flags"] & 3 != 3:
            raise ValueError("Capture stamp or observe-call timestamp missing")
        for value in struct.pack("<8I", *(row[name] for name in FIELDS)):
            checksum = ((checksum ^ value) * 16777619) & 0xffffffff
        times = [row[name] for name in FIELDS[1:6]]
        if any((b-a) & 0xffffffff > 30000 for a, b in zip(times, times[1:])):
            raise ValueError("Invalid timestamp order or excessive gap")
        if index and not 0 < (row["seq"]-rows[index-1]["seq"]) & 0xffffffff < 0x80000000:
            raise ValueError("Non-forward frame sequence")
    if checksum != int(info["fnv1a"], 16):
        raise ValueError("FNV checksum mismatch")
    return checksum


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM14")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--start", action="store_true", help="Start only an empty static buffer; never clears existing data")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    try:
        import serial
    except ImportError:
        sys.path.insert(0, str(Path(__file__).resolve().parents[2]/".build/stop-trace-python-deps"))
        import serial
    with (args.output/"serial.jsonl").open("w", encoding="utf-8") as log, serial.Serial(args.port, 115200, timeout=0.05) as port:
        def exchange(command, duration=0.3):
            port.write((command+"\r\n").encode("ascii"))
            log.write(json.dumps({"tx": command, "hostMonotonic": time.monotonic()})+"\n")
            data = bytearray()
            deadline = time.monotonic()+duration
            while time.monotonic() < deadline:
                chunk = port.read(port.in_waiting or 1)
                if chunk:
                    data.extend(chunk)
                    log.write(json.dumps({"rxHex": chunk.hex(), "hostMonotonic": time.monotonic()})+"\n")
            log.flush()
            return data.decode("ascii", errors="replace")

        exchange("!s")
        claim = exchange("x", 1)
        if "port=usb owner=usb" not in claim and "port=radio owner=radio" not in claim:
            raise RuntimeError("Port ownership not acknowledged")
        identity = exchange("?t", 1)
        if f"build={VERSION};" not in identity:
            raise RuntimeError("Dedicated zero-PWM firmware identity missing")
        status = exchange("@status", 1)
        if "mode=OBSERVE" not in status:
            raise RuntimeError("OBSERVE mode not confirmed")
        if args.start:
            ack = exchange("@timing_start")
            if "[TIMING] START " not in ack:
                raise RuntimeError("Static capture not started; existing buffer is preserved")
        info = None
        for _ in range(80):
            response = exchange("@timing_info", 0.5)
            match = re.search(r"\[TIMING_INFO\] ([^\r\n]+)", response)
            if match:
                info = dict(re.findall(r"(\w+)=(\w+)", match[1]))
                if info.get("active") == "0":
                    break
        if not info or info.get("active") != "0" or info.get("count") != "256":
            raise RuntimeError("No complete frozen buffer; retain serial log")
        rows = []
        for index in range(256):
            response = exchange(f"@timing_row {index}", 0.18)
            match = re.search(r"\[TIMING_ROW\] ([^\r\n]+)", response)
            if not match:
                raise RuntimeError(f"Missing row {index}; frozen buffer is retained for another export")
            rows.append({k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", match[1])})
        final = exchange("@timing_info")
        match = re.search(r"\[TIMING_INFO\] ([^\r\n]+)", final)
        if not match or dict(re.findall(r"(\w+)=(\w+)", match[1])) != info:
            raise RuntimeError("Frozen buffer metadata changed during export")
        validate(rows, info)
        result = {"version": VERSION, "info": info, "rows": rows,
                  "scope": "CSI completion to software observe return in OBSERVE, millisecond clock; not exposure or electrical PWM latency"}
        (args.output/"trace.json").write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
        print(f"Validated {len(rows)} rows; buffer retained; {args.output}")


if __name__ == "__main__":
    main()
