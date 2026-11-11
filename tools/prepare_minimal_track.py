"""Reproduce an isolated R13 track candidate from a pinned release; never flash.

The versioned minimal-track application is the only application input. Its
R13 provenance and shared SDK dependencies are pinned; no .build or release
archive is needed to reproduce it and the dirty main application is not read.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
POLICY = ROOT / "demo/candidates/minimal-track"
sys.path.insert(0, str(ROOT / "tools/testing"))
from process_runner import run_process


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def prepare(output):
    out = Path(output).resolve()
    if out.exists() or out == ROOT / ".build" or not out.is_relative_to(ROOT / ".build"):
        raise ValueError("A NEW directory below repository .build is required")
    policy = json.loads((POLICY / "baseline.json").read_text(encoding="utf-8"))
    manifest = json.loads((POLICY / "sdk-lock.json").read_text(encoding="utf-8"))
    for name, expected in manifest.items():
        relative = PurePosixPath(name)
        if relative.is_absolute() or ".." in relative.parts or ":" in name:
            raise ValueError("Unsafe SDK member: " + name)
        if digest(ROOT / name) != expected:
            raise ValueError("Pinned SDK source mismatch: " + name)
    workspace = out / "workspace"
    for name in manifest:
        path = workspace.joinpath(*PurePosixPath(name).parts)
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / name, path)
    code = workspace / "demo/code"
    shutil.copytree(POLICY / "code", code)
    project_source = workspace / policy["project"]
    project_source.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(POLICY / "source-project.uvprojx", project_source)
    tree = ET.parse(project_source)
    targets = tree.findall("./Targets/Target")
    if len(targets) != 1:
        raise ValueError("Exactly one C26 target required")
    target = targets[0]
    switches = {"VISION_PLANNER_RUNTIME": "0", "VISION_PLANNER_ACTIVE": "0",
                "VISION_PLANNER_PROFILE_REVIEWED": "0", "VISION_MINIMAL_TRACK": "1"}
    definitions = target.find(".//Cads/VariousControls/Define")
    existing = [d.strip() for d in definitions.text.split(",") if d.strip()]
    for macro in ["VISION_MIDTERM_AUTONOMOUS=0", "VISION_ROUTE_SWITCH=1", "VISION_ENCODER_ENABLE=1"]:
        if macro not in existing:
            raise ValueError("Wrong frozen target: " + macro)
    definitions.text = ",".join([d for d in existing if d.split("=")[0].strip() not in switches] +
                                [k + "=" + v for k, v in switches.items()])
    groups = target.find("Groups")
    for group in list(groups):
        if group.findtext("GroupName", "").startswith("Phase3 planner"):
            groups.remove(group)
            continue
        files = group.find("Files")
        if files is not None:
            for item in list(files):
                if item.findtext("FileName") in ("vision_planner_shadow_diag.c", "vision_planner_shadow_diag.h", "phase3_shadow_profile.h"):
                    files.remove(item)
    common = target.find("TargetOption/TargetCommonOption")
    for section in ("BeforeMake", "BeforeCompile", "AfterMake"):
        for node in common.find(section):
            if node.tag.startswith(("RunUserProg", "nStop")):
                node.text = "0"
            elif "Name" in node.tag:
                node.text = ""
    for key, folder in (("OutputDirectory", out / "Objects"), ("ListingPath", out / "Listings")):
        folder.mkdir(parents=True, exist_ok=True)
        common.find(key).text = str(folder) + os.sep
    target.find(".//AdsLmap").text = "1"
    target.find(".//ldLst").text = "1"
    directory = project_source.parent
    project = directory / "MPC_R13_Minimal.uvprojx"
    download = directory / "MPC_R13_Minimal_Download.uvprojx"
    target.find(".//UpdateFlashBeforeDebugging").text = "0"
    tree.write(project, encoding="utf-8", xml_declaration=True)
    download_tree = copy.deepcopy(tree)
    download_tree.find(".//UpdateFlashBeforeDebugging").text = "1"
    download_tree.write(download, encoding="utf-8", xml_declaration=True)
    project_source.unlink()
    options = workspace / "demo/mdk/base/rt1064.uvoptx"
    option_tree = ET.parse(options)
    for listing in option_tree.findall(".//ListingPath"):
        listing.text = str(out / "Listings") + os.sep
    for entry in (project, download):
        option_tree.write(entry.with_suffix(".uvoptx"), encoding="utf-8", xml_declaration=True)
    info = {"schemaVersion": "smartcar.minimal-track-candidate/1", "version": policy["version"],
            "sourceRelease": policy["sourceRelease"], "sourceArchiveSha256": policy["archiveSha256"],
            "baselinePolicySha256": digest(POLICY / "baseline.json"),
            "generatorSha256": digest(__file__),
            "sourceProjectSha256": digest(POLICY / "source-project.uvprojx"),
            "sdkLockSha256": digest(POLICY / "sdk-lock.json"),
            "verifiedFrozenMembers": len(manifest), "output": str(out),
            "project": str(project), "downloadProject": str(download), "target": target.findtext("TargetName"),
            "defines": definitions.text, "controlStrategy": "R13", "startup": "C26",
            "oneRunPerLoad": False, "experimentCutoffMs": None,
            "buildPassed": False, "flashed": False, "physicalTrackValidated": False,
            "startupGuardPhysicalValidation": "pending",
            "generatedCodeSha256": {p.name: digest(p) for p in sorted(code.iterdir()) if p.is_file()}}
    (out / "candidate.json").write_text(json.dumps(info, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (out / "README.md").write_text(
        "# " + info["version"] + "\n\nR13 control; C26 start/stop; planner runtime/active disabled.\n\n"
        "Build: " + str(project) + "\n\nDownload: " + str(download) +
        "\n\nTarget: " + info["target"] + "\n\nNot flashed; track and startup guard require vehicle validation.\n",
        encoding="utf-8")
    return info


def build(info, uv4, arm_bin):
    out = Path(info["output"])
    project = Path(info["project"])
    uv4 = Path(uv4).resolve()
    fromelf = Path(arm_bin).resolve() / "fromelf.exe"
    if not uv4.is_file() or not fromelf.is_file():
        raise ValueError("Existing UV4/fromelf tools required")
    processes = []
    commands = [[uv4, "-r", project, "-t", info["target"], "-j0", "-o", out / "build.log"],
                [fromelf, "--bincombined", "--output", out / "minimal-track.bin", out / "Objects/rt1064.axf"],
                [fromelf, "--text", "-c", "-s", "--output", out / "disassembly.txt", out / "Objects/rt1064.axf"]]
    with (out / "stdout.log").open("wb") as stdout, (out / "stderr.log").open("wb") as stderr:
        for command in commands:
            result = run_process(list(map(str, command)), project.parent, dict(os.environ), 600, stdout, stderr)
            processes.append(result)
            if result["exitCode"] or result["timedOut"] or not result["cleanupVerified"]:
                raise RuntimeError("Pure build failed: " + str(out / "build.log"))
    if "0 Error(s), 0 Warning(s)" not in (out / "build.log").read_text(encoding="utf-8", errors="replace"):
        raise RuntimeError("Zero errors AND warnings required")
    artifacts = [project, Path(info["downloadProject"]), out / "Objects/rt1064.axf",
                 out / "minimal-track.bin", out / "build.log", out / "disassembly.txt"]
    artifacts += [project.with_suffix(".uvoptx"), Path(info["downloadProject"]).with_suffix(".uvoptx")]
    maps = sorted((out / "Listings").glob("*.map"))
    if not maps:
        raise RuntimeError("Linker map is required for a reviewable build")
    artifacts += maps
    info.update(buildPassed=True, processes=processes,
                artifactsSha256={p.relative_to(out).as_posix(): digest(p) for p in artifacts})
    (out / "candidate.json").write_text(json.dumps(info, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return info


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--uv4", type=Path, default=Path("E:/ElecTronic/Keil/UV4/UV4.exe"))
    parser.add_argument("--arm-bin", type=Path, default=Path("E:/ElecTronic/Keil/ARM/ARMCLANG/bin"))
    args = parser.parse_args()
    result = prepare(args.output)
    if args.build:
        result = build(result, args.uv4, args.arm_bin)
    print(json.dumps({k: result[k] for k in ("version", "project", "downloadProject", "buildPassed", "flashed")}, indent=2))
