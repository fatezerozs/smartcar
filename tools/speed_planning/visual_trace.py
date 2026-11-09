"""Acquire a finite full-image static trace; never clear data or command motion."""
import argparse
import json
import hashlib
from pathlib import Path
import re
import struct
import sys
import time
import zlib

VERSION = "PHASE2_VISUAL_TRACE_20261109_1"
FIELDS = ("sequence", "capture_ms", "process_start_ms", "vision_end_ms", "observe_end_ms",
          "record_begin_ms", "raw_valid", "raw_status", "quality", "near_band_support",
          "far_band_support", "control_valid", "control_far_valid", "cue_bits",
          "copy_failures", "record_end_ms")
PAYLOAD_SIZE = 64+188*120


def decode_frame(blob, generation, index):
    if len(blob) != 32+PAYLOAD_SIZE:
        raise ValueError("Frame size mismatch")
    magic, gen, at, size, crc, width, height, fields = struct.unpack_from("<8I", blob)
    if (magic, gen, at, size, width, height, fields) != (0x31465156, generation, index, PAYLOAD_SIZE, 188, 120, 16):
        raise ValueError("Frame identity or dimensions mismatch")
    payload = blob[32:]
    if zlib.crc32(payload) != crc:
        raise ValueError("Frame CRC32 mismatch")
    values = dict(zip(FIELDS, struct.unpack_from("<16I", payload)))
    for name in ("raw_valid", "near_band_support", "far_band_support", "control_valid", "control_far_valid"):
        if values[name] not in (0, 1):
            raise ValueError("Invalid boolean state")
    if values["quality"] > 100 or values["cue_bits"] > 3:
        raise ValueError("Out-of-range frame metadata")
    ordered = [values[k] for k in ("capture_ms", "process_start_ms", "vision_end_ms", "observe_end_ms", "record_begin_ms", "record_end_ms")]
    if any(((b-a) & 0xffffffff) > 30000 for a, b in zip(ordered, ordered[1:])):
        raise ValueError("Invalid time ordering")
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM14")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--start", action="store_true", help="Start an empty static buffer; requires operator readiness")
    parser.add_argument("--check", action="store_true", help="Identity/status only; do not start or export")
    args = parser.parse_args()
    if args.start and args.check:
        parser.error("--start and --check are mutually exclusive")
    args.output.mkdir(parents=True, exist_ok=False)
    try:
        import serial
    except ImportError:
        sys.path.insert(0, str(Path(__file__).resolve().parents[2]/".build/stop-trace-python-deps"))
        import serial
    with serial.Serial(args.port, 921600, timeout=.05) as port, (args.output/"control.bin").open("wb") as log:
        def exchange(command, duration=.5):
            log.write(("\nHOST_TX "+command+"\n").encode("ascii"))
            port.write((command+"\r\n").encode("ascii"))
            data = bytearray()
            deadline = time.monotonic()+duration
            while time.monotonic() < deadline:
                data.extend(port.read(port.in_waiting or 1))
            log.write(data)
            log.flush()
            return data.decode("ascii", errors="replace")

        def info():
            # A delayed/missing query reply must not require re-exporting images.
            # Only this read-only query is retried; never replay start or clear.
            match = None
            for _ in range(3):
                text = exchange("@vq_info")
                match = re.search(r"\[VQ_INFO\] ([^\r\n]+)", text)
                if match:
                    break
            if not match:
                raise RuntimeError("Missing VQ_INFO; buffer not cleared")
            result = {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", match[1])}
            if result.get("capacity") != 750 or result.get("payload") != PAYLOAD_SIZE or result.get("zero_pwm") != 1:
                raise RuntimeError("Unexpected capture contract")
            return result

        exchange("!s")
        if "port=usb owner=usb" not in exchange("x", 1):
            raise RuntimeError("USB ownership not confirmed")
        if f"build={VERSION};" not in exchange("?t", 1):
            raise RuntimeError("Wrong firmware identity")
        if "mode=OBSERVE" not in exchange("@status", 1):
            raise RuntimeError("OBSERVE not confirmed")
        initial = info()
        if args.check:
            (args.output/"identity.json").write_text(json.dumps({"version": VERSION, "info": initial}, indent=2)+"\n", encoding="utf-8")
            print(f"Identity verified: {VERSION}, USB921600, OBSERVE, count={initial['count']}", flush=True)
            return
        if args.start:
            if "[VQ] START " not in exchange("@vq_start"):
                raise RuntimeError("Start rejected; existing buffer retained")
            print("CAPTURE STARTED: first beep cover, second beep uncover, final beep frozen", flush=True)
        for _ in range(70):
            frozen = info()
            if not frozen["active"]:
                break
        if frozen["active"] or frozen["reason"] != 1 or frozen["count"] != 750:
            raise RuntimeError("Incomplete capture; retained, not accepted as complete")
        (args.output/"frozen-info.json").write_text(json.dumps(frozen, indent=2)+"\n", encoding="utf-8")
        print("750 frames frozen; exporting read-only", flush=True)
        records = []
        for index in range(frozen["count"]):
            port.write(f"@vq_frame {index}\r\n".encode("ascii"))
            header = bytearray()
            deadline = time.monotonic()+4
            while not header.endswith(b"VQF1"):
                byte = port.read(1)
                header.extend(byte)
                if len(header) > 8192 or time.monotonic() > deadline:
                    log.write(header)
                    raise RuntimeError(f"Missing frame header {index}; buffer retained")
            log.write(header[:-4])
            blob = bytearray(b"VQF1")
            while len(blob) < 32+PAYLOAD_SIZE:
                blob.extend(port.read(32+PAYLOAD_SIZE-len(blob)))
                if time.monotonic() > deadline:
                    (args.output/f"frame-{index:04d}.partial").write_bytes(blob)
                    raise RuntimeError(f"Frame {index} read timed out; buffer retained")
            path = args.output/f"frame-{index:04d}.vqf"
            path.write_bytes(blob)
            values = decode_frame(blob, frozen["gen"], index)
            if records:
                delta = (values["sequence"]-records[-1]["sequence"]) & 0xffffffff
                if not 0 < delta < 0x80000000:
                    raise RuntimeError("Non-forward source sequence")
            values.update(index=index, file=path.name, sha256=hashlib.sha256(blob).hexdigest())
            records.append(values)
            if (index+1) % 50 == 0:
                print(f"EXPORTED {index+1}/750", flush=True)
        if info() != frozen:
            raise RuntimeError("Frozen metadata changed during export")
        manifest = {"schemaVersion": "smartcar.visual-trace/1", "version": VERSION,
                    "mode": "OBSERVE", "sourceImageSize": [188, 120], "frozen": frozen,
                    "frames": records, "complete": True,
                    "limits": ["Static zero-PWM build; not MPC motion validation.",
                               "Band support is geometric diagnostic support independent of raw/control validity.",
                               "Buzzer cue timestamps are software prompts, not physical occlusion ground truth.",
                               "CSI completion clock1ms; exposure and physical PWM timing unmeasured."]}
        (args.output/"session.json").write_text(json.dumps(manifest, indent=2)+"\n", encoding="utf-8")
        print(f"COMPLETE 750 CRC-checked full images, buffer retained: {args.output}", flush=True)


if __name__ == "__main__":
    main()
