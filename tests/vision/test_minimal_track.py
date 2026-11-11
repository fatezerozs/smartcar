"""Public preparation and native control contracts for the minimal track build."""
import hashlib
import importlib
import json
from pathlib import Path
import tempfile
import shutil
import subprocess
import sys
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.speed_planning.replay import compile_command

CODE = ROOT / "demo/candidates/minimal-track/code"
REFERENCE = ROOT / "tests/vision/reference/r13-minimal"
CHAIN = ["vision_speed.c", "wheel_speed.c", "vision_motion.c", "vision_drive.c",
         "vision_actuator.c", "vision_guard.c", "vision_probe.c", "vision_startup.c",
         "vision_vehicle_profile.c", "vision_control.c", "midterm_open_loop.c",
         "app_mode.c", "vision_field.c", "vision_storage.c"]
DEFINES = ["VISION_ENCODER_ENABLE=1", "VISION_MIDTERM_TRAINING=1",
           "VISION_MIDTERM_AUTONOMOUS=0", "VISION_MIDTERM_ROUTE_TRIAL=1",
           "VISION_ROUTE_SWITCH=1", "VISION_DRIVE_ENABLE=1", "VISION_START_SWITCH_ENABLE=0"]


def native(folder, label, code, fixture, sources, defines=()):
    exe = folder / (label + ".exe")
    command = compile_command(shutil.which("gcc"), "gcc", "optimized",
        [ROOT / fixture] + [code / name for name in sources],
        [ROOT / "tests/vision/stubs", code], exe)
    command[1:1] = ["-Wno-unused-function"] + ["-D" + d for d in defines]
    built = subprocess.run(command, capture_output=True, text=True, timeout=90)
    if built.returncode:
        raise AssertionError(built.stderr)
    return subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)


class MinimalPreparation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT / ".build").mkdir(exist_ok=True)

    def test_isolated_r13_build_has_one_controller_and_no_experiment_lease(self):
        module = importlib.import_module("tools.prepare_minimal_track")
        policy = ROOT / "demo/candidates/minimal-track/baseline.json"
        before = hashlib.sha256(policy.read_bytes()).hexdigest()
        (ROOT / ".build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / ".build") as folder:
            out = Path(folder) / "candidate"
            info = module.prepare(out)
            self.assertFalse(info["buildPassed"])
            self.assertFalse(info["flashed"])
            self.assertEqual(info["controlStrategy"], "R13")
            self.assertEqual(info["startup"], "C26")
            project = ET.parse(info["project"])
            self.assertEqual(len(project.findall("./Targets/Target")), 1)
            definitions = project.findtext(".//Cads/VariousControls/Define").split(",")
            for macro in ["VISION_PLANNER_RUNTIME=0", "VISION_PLANNER_ACTIVE=0",
                          "VISION_MIDTERM_AUTONOMOUS=0", "VISION_ROUTE_SWITCH=1"]:
                self.assertIn(macro, [d.strip() for d in definitions])
            sources = [p.text for p in project.findall(".//FilePath")]
            self.assertFalse(any("planner_shadow" in p or "vision_metric_" in p for p in sources))
            self.assertFalse(any("speed_planner.c" in p for p in sources))
            self.assertEqual(project.findtext(".//UpdateFlashBeforeDebugging"), "0")
            download = ET.parse(info["downloadProject"])
            self.assertEqual(download.findtext(".//UpdateFlashBeforeDebugging"), "1")
            for section in ["BeforeMake", "BeforeCompile", "AfterMake"]:
                for node in project.findall(".//" + section + "/*"):
                    if node.tag.startswith(("RunUserProg", "nStop")):
                        self.assertEqual(node.text, "0")
            self.assertFalse(info["oneRunPerLoad"])
            self.assertIsNone(info["experimentCutoffMs"])
            saved = json.loads((out / "candidate.json").read_text(encoding="utf-8"))
            self.assertEqual(saved["baselinePolicySha256"], before)
            for name, expected in saved["generatedCodeSha256"].items():
                self.assertEqual(hashlib.sha256((CODE / name).read_bytes()).hexdigest(), expected)
            self.assertTrue((out / "workspace/demo/mdk/scf/MIMXRT1064xxxxx_flexspi_nor.scf").is_file())
            with self.assertRaises(ValueError):
                module.prepare(out)
        self.assertEqual(hashlib.sha256(policy.read_bytes()).hexdigest(), before)

    def test_grayscale_ring_order_and_reacquisition(self):
        module = importlib.import_module("tools.prepare_minimal_track")
        with tempfile.TemporaryDirectory(dir=ROOT / ".build") as folder:
            out = Path(folder) / "candidate"
            module.prepare(out)
            code = out / "workspace/demo/code"
            exe = out / "minimal-route.exe"
            command = compile_command(shutil.which("gcc"), "gcc", "optimized",
                [ROOT / "tests/vision/test_minimal_route.c", code / "vision_midterm.c"], [code], exe)
            command[1:1] = ["-Wno-unused-function", "-DVISION_ROUTE_SWITCH=1", "-DVISION_MINIMAL_TRACK=1"]
            built = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS minimal route", result.stdout)
            print(result.stdout.strip())

    def test_frozen_r13_sources(self):
        manifest = json.loads((REFERENCE / "manifest.json").read_text("utf-8"))
        for name, expected in manifest.items():
            self.assertEqual(hashlib.sha256((REFERENCE / name).read_bytes()).hexdigest(), expected, name)

    def test_route_regression_rejects_missing_completion_event(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".build") as tmp:
            code = Path(tmp) / "mutant"
            shutil.copytree(CODE, code)
            header = code / "vision_reference_route.h"
            text = header.read_text("utf-8")
            event = "if(reference_rings_completed<2)++reference_rings_completed;"
            self.assertEqual(text.count(event), 1)
            header.write_text(text.replace(event, "/* deliberately lost exit event */"), "utf-8")
            result = native(Path(tmp), "missing-event", code, "tests/vision/test_minimal_route.c",
                            ["vision_midterm.c"], ["VISION_ROUTE_SWITCH=1"])
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("completed() == ring + 1", result.stderr)
            print("Mutation check: dropping a completed ring event is detected by the public grayscale regression")

    def test_r13_complete_control_chain_parity(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".build") as tmp:
            outputs = []
            for label, code in [("reference", REFERENCE), ("subject", CODE)]:
                result = native(Path(tmp), label, code, "tests/vision/fixtures/minimal_r13_chain.c",
                                CHAIN, DEFINES + (["SUBJECT=1"] if label == "subject" else []))
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                outputs.append(result.stdout.splitlines())
            self.assertEqual(len(outputs[0]), 10480)
            self.assertEqual(len(outputs[0]), len(outputs[1]))
            cache_clear = 0
            for row, (expected, actual) in enumerate(zip(*outputs)):
                if expected == actual:
                    continue
                x, y = expected.split(), actual.split()
                self.assertIn(int(x[0]), (10, 11), (row, expected, actual))
                self.assertIn(int(x[1]), range(101, 105), (row, expected, actual))
                self.assertEqual(x[:13], y[:13], (row, expected, actual))
                self.assertEqual(x[17:20], ["0"] * 3)
                self.assertEqual(y[13:20], ["0"] * 7)
                self.assertEqual(x[20], y[20])
                cache_clear += 1
            self.assertEqual(cache_clear, 8)
            print("R13 parity: 10480 steps / 28 constructed scenarios; 10472 identical rows, "
                  "8 documented absent-road cache clears; final PWM/servo equal at every step")

    def test_startup_disturbances_and_physical_stop_contract(self):
        with tempfile.TemporaryDirectory(dir=ROOT / ".build") as tmp:
            result = native(Path(tmp), "startup", CODE, "tests/vision/test_minimal_startup.c", CHAIN, DEFINES)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("ADVERSE INPUT PASS", result.stdout)
            print(result.stdout.strip())


if __name__ == "__main__":
    unittest.main(verbosity=2)
