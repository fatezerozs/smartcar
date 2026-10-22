"""Offline transport checks. No serial package or connected board is required."""
import contextlib
import io
from pathlib import Path
import runpy
import sys
import types
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[2] / "tools/restore_training_parameters.py"


class FakePort:
    def __init__(self, identity=True, owner=True, reject=None, pwm="0/0"):
        self.identity, self.owner, self.reject, self.pwm = identity, owner, reject, pwm
        self.is_open = False
        self.buffer = b""
        self.commands = []

    def open(self):
        self.is_open = True

    def close(self):
        self.is_open = False

    @property
    def in_waiting(self):
        return len(self.buffer)

    def flush(self):
        pass

    def read(self, size):
        data, self.buffer = self.buffer[:size], self.buffer[size:]
        return data

    def write(self, data):
        command = data.decode("ascii").strip()
        self.commands.append(command)
        if command == "x":
            reply = "[LINK] port=usb owner=" + ("usb" if self.owner else "radio")
        elif command == "?t":
            reply = "[FIELD] build=" + ("MIDTERM_TRAIN_1;" if self.identity else "OTHER;")
        elif command == self.reject:
            reply = "[FIELD] REJECT config"
        elif command == "@speed_status":
            reply = ("[ENCODER] sign3=-1 sign4=1 confirmed=1\n"
                     "[SPEED] enabled=1 max_ticks=40 kp=1.0000 ki=0.0469 "
                     f"pwm_R/L={self.pwm} faults=0\n")
        elif command == "@status":
            reply = ("[FIELD] version=MIDTERM_TRAIN_1 ready=0 "
                     "cam=0 hw=1 servo=1550/1500/1600 sign=-1 "
                     "cruise=0.0000 cap=0.1500\n"
                     "[BANDS] near=0.7800 far=0.6200 half=0.0350\n"
                     "[MOTOR] confirmed=1 dir=1/1 scale=1.0000/1.0000\n"
                     "[STARTUP] enabled=0 delay_ms=2000\n")
        elif command.startswith(("@encoder_sign", "@speed_cfg", "@speed_mode")):
            reply = "[SPEED] OK;"
        elif command.startswith("@"):
            reply = "[FIELD] OK;"
        else:
            reply = "[LINK] STOP"
        self.buffer += (reply + "\n").encode("ascii")
        return len(data)


class RestoreTests(unittest.TestCase):
    def execute(self, port):
        serial_module = types.ModuleType("serial")
        serial_module.Serial = lambda: port
        ticks = [0.0]

        def clock():
            ticks[0] += 0.1
            return ticks[0]

        with patch.dict(sys.modules, {"serial": serial_module}), \
             patch.object(sys, "argv", [str(SCRIPT), "--port", "TEST"]), \
             patch("time.monotonic", clock), \
             contextlib.redirect_stdout(io.StringIO()) as output:
            runpy.run_path(str(SCRIPT), run_name="__main__")
        return output.getvalue()

    def test_restoration_never_arms_and_claims_before_every_framed_command(self):
        port = FakePort()
        self.assertIn("RESTORE VERIFIED", self.execute(port))
        for index, command in enumerate(port.commands):
            self.assertNotIn(command, ("a", "h", "@save"))
            self.assertFalse(command.startswith(("@camera", "@probe", "@wheel")))
            if command.startswith("@"):
                self.assertEqual(port.commands[index - 1], "x")
        self.assertIn("@motor 0 0.15 0.1", port.commands)
        self.assertEqual(port.commands[-1], "!s")
        self.assertFalse(port.is_open)

    def test_wrong_firmware_sends_no_configuration(self):
        port = FakePort(identity=False)
        with self.assertRaisesRegex(RuntimeError, "identity"):
            self.execute(port)
        self.assertFalse(any(c.startswith("@") for c in port.commands))
        self.assertFalse(port.is_open)

    def test_missing_ownership_sends_no_configuration(self):
        port = FakePort(owner=False)
        with self.assertRaisesRegex(RuntimeError, "ownership"):
            self.execute(port)
        self.assertFalse(any(c.startswith("@") for c in port.commands))
        self.assertEqual(port.commands[-1], "!s")

    def test_rejected_configuration_aborts_remaining_updates(self):
        port = FakePort(reject="@balance 1 1")
        with self.assertRaisesRegex(RuntimeError, "rejected"):
            self.execute(port)
        self.assertNotIn("@speed_mode 1", port.commands)
        self.assertFalse(port.is_open)

    def test_nonzero_output_readback_is_not_reported_as_success(self):
        port = FakePort(pwm="1/0")
        with self.assertRaisesRegex(RuntimeError, "speed readback"):
            self.execute(port)
        self.assertEqual(port.commands[-1], "!s")
        self.assertFalse(port.is_open)


if __name__ == "__main__":
    unittest.main()
