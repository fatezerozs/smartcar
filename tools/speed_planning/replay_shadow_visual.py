"""Offline target-shadow replay over verified visual metadata; synthetic motion/geometry."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

from tools.speed_planning.analyze_visual_trace import analyze
from tools.speed_planning.replay_visual_supervisor import run as run_supervisor
from tools.speed_planning.replay import compile_command
from tools.speed_planning.vehicle_model import ROOT


def run(session, output, compiler=None):
    session, output = Path(session).resolve(), Path(output).resolve()
    analyze(session)  # CRC/SHA/complete-session checks before creating artifacts.
    if output.exists():
        raise ValueError('Output must be a new directory')
    family = os.environ.get('SMARTCAR_TEST_COMPILER', 'gcc')
    mode = os.environ.get('SMARTCAR_TEST_MODE', 'optimized')
    if family not in ('gcc', 'msvc') or mode not in ('optimized', 'asan'):
        raise ValueError('Unsupported compiler family/mode')
    compiler = compiler or os.environ.get('SMARTCAR_TEST_COMPILER_PATH') or shutil.which('cl' if family == 'msvc' else 'gcc')
    if not compiler:
        raise ValueError('Existing compiler required')
    output.mkdir(parents=True)
    # Reuses source verification + independent Python oracle, not a second
    # implementation of production supervisor rules or image recognition.
    baseline = output/'supervisor'
    reference = run_supervisor(session, baseline, compiler)
    sources = [ROOT/'tools/speed_planning/shadow_visual_runner.c', ROOT/'demo/code/vision_planner_shadow.c',
               ROOT/'demo/code/vision_planner.c', ROOT/'tools/speed_planning/speed_planner.c']
    exe = output/'shadow-visual.exe'
    command = compile_command(str(compiler), family, mode, sources, [ROOT/'demo/code', baseline], exe)
    subprocess.run(command, cwd=output, check=True, capture_output=True, timeout=60)
    reports = []
    columns = ['index', 'mode', 'authorized', 'inhibit', 'farActive', 'farStreak', 'farStartedMs',
               'stopActive', 'faults', 'generation', 'armAttempt', 'armAccepted']
    for case in reference['experiments']:
        path = output/(case['case']+'.csv')
        subprocess.run([exe, baseline/'observations.csv', path, str(case['explicitArmIndex']),
                        str(case['farBudgetMs']), str(case['farRecoveryFrames'])], cwd=output, check=True, timeout=15)
        with path.open(newline='', encoding='utf-8') as file:
            actual = list(csv.DictReader(file))
        with (baseline/path.name).open(newline='', encoding='utf-8') as file:
            expected = list(csv.DictReader(file))
        if len(actual) != 750 or len(actual) != len(expected):
            raise AssertionError('Incomplete target shadow replay')
        ready, controlled, inhibited = 0, 0, 0
        for got, want in zip(actual, expected):
            if any(got[k] != want[k] for k in columns):
                raise AssertionError(f"{case['case']} row {got['index']}: supervisor mismatch")
            if int(got['leftTicks10ms']) or int(got['rightTicks10ms']):
                raise AssertionError('Synthetic zero-speed fixture produced nonzero integer targets')
            if got['status'] == '2':  # VPS_READY
                ready += 1
                if got['matches'] != '1':
                    raise AssertionError('Fresh shadow candidate failed publication check')
                if got['mode'] == '4':
                    controlled += 1
                    if got['action'] != '2':
                        raise AssertionError('Controlled stop did not shape zero target')
            if got['inhibit'] == '1':
                inhibited += 1
                if got['matches'] != '0' or got['action'] != '0':
                    raise AssertionError('Inhibited state retained a consumable candidate')
        reports.append({'case': case['case'], 'rows': len(actual), 'supervisorMatchesIndependentOracle': True,
                        'readyCandidates': ready, 'controlledStopCandidates': controlled, 'inhibitedRows': inhibited,
                        'csvSha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    result = {'schemaVersion': 'smartcar.shadow-visual-replay/1', 'sourceFrames': 750,
              'sourceSessionSha256': reference['sourceSessionSha256'], 'hardwareAccessed': False,
              'productionApproved': False, 'runtimeConnected': False,
              'evidence': 'measured-visual-metadata-with-synthetic-metric-and-zero-motion-inputs',
              'syntheticInputs': {'measuredSpeedMps': 0, 'referenceSpeedMps': 0, 'odometerM': 0,
                                  'leftTicksPerM': 11000, 'rightTicksPerM': 12000,
                                  'geometry': 'straight near2.25m/full3.75m, not image-derived'},
              'experiments': reports, 'compilerFamily': family, 'compilerMode': mode, 'compileCommand': command,
              'sourcesSha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in sources + [Path(__file__), ROOT/'demo/code/vision_planner_shadow.h',
                                                  ROOT/'demo/code/vision_planner.h', ROOT/'tools/speed_planning/speed_planner.h']},
              'limits': ['No image algorithm or measured metric preview.', 'Motion, scales and zero-speed confirmations are synthetic.',
                         'No live scheduler, PI, PWM, C26 admission or production threshold approval.']}
    (output/'report.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'frames': 750, 'cases': len(reports), 'rows': sum(c['rows'] for c in reports),
                      'productionApproved': False}))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('session', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler')
    args = parser.parse_args()
    run(args.session, args.output, args.compiler)
