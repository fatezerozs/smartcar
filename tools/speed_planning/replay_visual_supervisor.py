"""Offline hybrid replay: measured visual flags/times, synthetic zero-speed encoder.

Verifies all source images before using metadata. Does not create a measured
motion sidecar, estimate a production budget, or access a device.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

from tools.speed_planning.analyze_visual_trace import analyze
from tools.speed_planning.reference import DEFAULT_REFERENCE, load_reference, values
from tools.speed_planning.replay import compile_command, MODES
from tools.speed_planning.vehicle_model import ROOT


def recovery_episodes(rows, confirmations):
    """Observation-only segments; no invented metric distances or velocities."""
    episodes, start, streak = [], None, 0
    for index, row in enumerate(rows):
        if start is None and row['control_valid'] and not row['control_far_valid']:
            start, streak = index, 0
        if start is None:
            continue
        streak = streak+1 if row['control_valid'] and row['control_far_valid'] else 0
        if streak >= confirmations:
            episodes.append({'firstIndex': start, 'confirmedRecoveryIndex': index,
                             'elapsedCaptureMs': (row['capture_ms']-rows[start]['capture_ms']) & 0xffffffff,
                             'leftCensored': start == 0, 'rightCensored': False})
            start, streak = None, 0
    if start is not None:
        episodes.append({'firstIndex': start, 'confirmedRecoveryIndex': None,
                         'elapsedCaptureMs': (rows[-1]['capture_ms']-rows[start]['capture_ms']) & 0xffffffff,
                         'leftCensored': start == 0, 'rightCensored': True})
    return episodes


def oracle(rows, arm_index, budget, confirmations):
    """Independent time/streak oracle for this near-valid, stationary fixture.

    The fixture has no frame/encoder faults; it cannot be used to oracle near
    recovery, nonzero speed, distance exhaustion, or asynchronous control polls.
    """
    armed = False
    mode, generation, far_start, streak, stop, zero = 0, 0, None, 0, False, 0
    result = []
    for index, row in enumerate(rows):
        if index == arm_index:
            armed = bool(row['control_valid'] and row['control_far_valid'])
            if armed:
                mode, generation = 1, generation+1
        elif armed:
            if stop:
                zero += 1
                if zero >= 3:
                    armed, mode, generation = False, 5, generation+1
            if armed:
                if not row['control_far_valid'] and far_start is None:
                    far_start, streak = row['capture_ms'], 0
                if far_start is not None:
                    streak = streak+1 if row['control_far_valid'] else 0
                    age = (row['observe_end_ms']-far_start) & 0xffffffff
                    if stop or age >= budget:
                        stop, mode = True, 4
                    elif streak >= confirmations:
                        far_start, streak, mode = None, 0, 1
                    else:
                        mode = 2
        result.append({'mode': mode, 'authorized': int(armed), 'inhibit': int(not armed),
                       'generation': generation})
    return result


def run(session, output, compiler=None, reference=DEFAULT_REFERENCE, arm_index=103):
    session, output = Path(session).resolve(), Path(output).resolve()
    analysis = analyze(session)  # Fail closed on incomplete, tampered or mismatched data.
    manifest = json.loads((session/'session.json').read_text(encoding='utf-8'))
    rows = manifest['frames']
    if not all(row['control_valid'] == 1 for row in rows):
        raise ValueError('This replay oracle requires an all-near-valid trace')
    if not 0 <= arm_index < len(rows) or not rows[arm_index]['control_far_valid']:
        raise ValueError('Explicit arm index must have valid far observation')
    if output.exists():
        raise ValueError('Output must be a new directory')
    family = os.environ.get('SMARTCAR_TEST_COMPILER', 'gcc')
    if family not in ('gcc', 'msvc'):
        raise ValueError('Supported compiler family: gcc or msvc')
    mode = os.environ.get('SMARTCAR_TEST_MODE', 'optimized')
    if mode not in ('optimized', 'asan'):
        raise ValueError('Supported compiler mode: optimized or asan')
    compiler = compiler or os.environ.get('SMARTCAR_TEST_COMPILER_PATH') or shutil.which('cl' if family == 'msvc' else 'gcc')
    if not compiler:
        raise ValueError('Existing compiler required; no tools are installed')
    profile = load_reference(reference)
    params = values(profile)
    output.mkdir(parents=True)
    # Same policy constants as the existing Host reference pipeline. No model,
    # wheel scale, fictitious metric preview or PI is needed for this experiment.
    policy = (f"{{100U,30U,{params['nearWindowMs']}U,{params['farBudgetMs']}U,"
              f"{params['stopTimeoutMs']}U,{params['nearFrames']}U,{params['farFrames']}U,"
              f"3U,{float(params['farDistanceM'])}f,0.02f}}")
    (output/'visual_policy.h').write_text('/* Host simulation only. */\n#define VISUAL_POLICY '+policy+'\n', encoding='utf-8')
    source = ROOT/'tools/speed_planning/visual_supervisor_runner.c'
    planner = ROOT/'tools/speed_planning/speed_planner.c'
    executable = output/'visual-supervisor.exe'
    command = compile_command(str(compiler), family, mode, [source, planner],
                              [ROOT/'tools/speed_planning', output], executable)
    subprocess.run(command, cwd=output, check=True, capture_output=True, timeout=60)
    with (output/'observations.csv').open('w', encoding='utf-8', newline='') as file:
        writer = csv.writer(file)
        writer.writerow(['index', 'sequence', 'captureMs', 'observeEndMs', 'nearValid', 'farValid'])
        writer.writerows([i, r['sequence'], r['capture_ms'], r['observe_end_ms'],
                          r['control_valid'], r['control_far_valid']] for i, r in enumerate(rows))
    scenarios = [('initial-admission-only', 0, params['farBudgetMs'], params['farFrames']),
                 ('reference-time-budget', arm_index, params['farBudgetMs'], params['farFrames'])]
    scenarios += [(f'diagnostic-recovery-{n}', arm_index, 10000, n) for n in (1, 3, 5)]
    reports = []
    for name, arm, budget, confirmations in scenarios:
        path = output/(name+'.csv')
        subprocess.run([executable, output/'observations.csv', path, str(arm), str(budget), str(confirmations)],
                       cwd=output, check=True, timeout=15)
        with path.open(encoding='utf-8', newline='') as file:
            actual = [{key: int(value) for key, value in row.items()} for row in csv.DictReader(file)]
        expected = oracle(rows, arm, budget, confirmations)
        if len(actual) != len(rows):
            raise AssertionError('Incomplete C replay')
        transitions, previous = [], None
        for index, (got, want) in enumerate(zip(actual, expected)):
            if got['index'] != index or got['faults'] or any(got[key] != want[key] for key in want):
                raise AssertionError(f'{name} oracle mismatch at frame {index}: {got}, expected {want}')
            if got['mode'] != previous:
                transitions.append({'index': index, 'captureElapsedMs': (rows[index]['capture_ms']-rows[0]['capture_ms']) & 0xffffffff,
                                    'mode': MODES[got['mode']]})
                previous = got['mode']
        reports.append({'case': name, 'explicitArmIndex': arm, 'armAccepted': bool(actual[arm]['armAccepted']),
                        'farBudgetMs': budget, 'farRecoveryFrames': confirmations,
                        'allRowsMatchIndependentOracle': True, 'transitions': transitions,
                        'finalMode': MODES[actual[-1]['mode']],
                        'csvSha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    result = {'schemaVersion': 'smartcar.visual-supervisor-replay/1', 'sourceSession': str(session),
              'sourceSessionSha256': hashlib.sha256((session/'session.json').read_bytes()).hexdigest(),
              'evidence': 'hybrid-measured-visual-metadata-and-synthetic-stationary-encoder',
              'productionApproved': False, 'hardwareAccessed': False, 'sourceFrames': len(rows),
              'sourceIntegrity': analysis, 'referenceId': profile['referenceId'],
              'referenceSha256': hashlib.sha256(Path(reference).read_bytes()).hexdigest(),
              'policy': {'frameTimeoutMs': 100, 'encoderTimeoutMs': 30, 'zeroSamples': 3,
                         'zeroSpeedMps': 0.02, 'farDistanceM': params['farDistanceM']},
              'syntheticInputs': {'encoderValid': True, 'encoderSequence': 'index+1',
                                  'encoderMs': 'observe_end_ms', 'odometerM': 0, 'speedMps': 0},
              'compilerFamily': family, 'compilerMode': mode, 'compileCommand': command,
              'sourcesSha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in (source, planner, Path(__file__))},
              'experiments': reports,
              'observationRecoverySensitivity': {str(n): recovery_episodes(rows, n) for n in (1, 3, 5)},
              'limits': ['No image algorithm replay: uses stored controller validity flags.',
                         'Encoder, zero speed and zero path are synthetic, not synchronized vehicle measurements.',
                         'One supervisor call per acquired frame; no inter-frame10ms schedule or hardware output.',
                         '10s diagnostic budget is an intentionally permissive software experiment, never a proposed production value.',
                         'No distance-budget, metric planner, PI, actual stopping or MPC target validation.']}
    (output/'replay-report.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'frames': len(rows), 'cases': len(reports), 'oracleChecks': len(rows)*len(reports),
                      'finalModes': {case['case']: case['finalMode'] for case in reports},
                      'productionApproved': False}, ensure_ascii=False))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('session', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler')
    parser.add_argument('--reference', type=Path, default=DEFAULT_REFERENCE)
    parser.add_argument('--arm-index', type=int, default=103)
    args = parser.parse_args()
    run(args.session, args.output, args.compiler, args.reference, args.arm_index)


if __name__ == '__main__':
    main()
