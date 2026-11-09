"""Check native C count/history conversion against an independent Python calculation.

Consumes existing frozen stop traces only. No serial, firmware, control commands,
metric vision or synthetic PI output is added to a measured motion record.
"""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
from tools.speed_planning.export_stop_trace import validate_trace
from tools.speed_planning.replay import compile_command
from tools.speed_planning.vehicle_model import ROOT


def run(traces, output, left_scale, right_scale, compiler=None):
    traces = [Path(p).resolve() for p in traces]; output = Path(output).resolve()
    if not traces or any(not math.isfinite(v) or not 0 < v <= 1e6 for v in (left_scale, right_scale)):
        raise ValueError('Explicit finite positive wheel scales and traces required')
    loaded = []
    for path in traces:
        data = json.loads(path.read_text('utf-8'))
        if data.get('schemaVersion') != 'smartcar.stop-trace/1':
            raise ValueError('Unsupported trace schema')
        checks = validate_trace(data['info'], data['rows'])
        loaded.append((path, data, checks))
    if output.exists():
        raise ValueError('Output must be a new directory')
    family = os.environ.get('SMARTCAR_TEST_COMPILER', 'gcc'); mode = os.environ.get('SMARTCAR_TEST_MODE', 'optimized')
    if family not in ('gcc', 'msvc') or mode not in ('optimized', 'asan'):
        raise ValueError('Unsupported compiler family/mode')
    compiler = compiler or os.environ.get('SMARTCAR_TEST_COMPILER_PATH') or shutil.which('cl' if family == 'msvc' else 'gcc')
    if not compiler:
        raise ValueError('Existing compiler required')
    output.mkdir(parents=True)
    sources = [ROOT/'tools/speed_planning/metric_odom_runner.c', ROOT/'demo/code/vision_metric_io.c',
               ROOT/'tools/speed_planning/vision_adapter.c', ROOT/'tools/speed_planning/speed_planner.c']
    exe = output/'metric-odom.exe'; command = compile_command(str(compiler), family, mode, sources,
                                                               [ROOT/'demo/code'], exe)
    subprocess.run(command, cwd=output, check=True, capture_output=True, timeout=60)
    reports = []
    for case, (path, data, checks) in enumerate(loaded):
        source = output/f'input-{case}.csv'; destination = output/f'output-{case}.csv'
        with source.open('w', encoding='utf-8', newline='') as file:
            writer = csv.writer(file);writer.writerow(['sequence', 'ms', 'totalLeft', 'totalRight'])
            writer.writerows((r['seq'], r['ms'], r['tl'], r['tr']) for r in data['rows'])
        subprocess.run([exe, source, destination, str(left_scale), str(right_scale)], cwd=output, check=True, timeout=15)
        with destination.open(encoding='utf-8', newline='') as file:
            actual = list(csv.DictReader(file))
        if len(actual) != len(data['rows']):
            raise AssertionError('Incomplete native odometry output')
        travelled_l, travelled_r, previous_distance = 0, 0, 0.
        for index, (row, got) in enumerate(zip(data['rows'], actual)):
            dt = 0 if not index else (row['ms']-data['rows'][index-1]['ms']) & 0xffffffff
            dl = 0 if not index else -(row['tl']-data['rows'][index-1]['tl'])
            dr = 0 if not index else row['tr']-data['rows'][index-1]['tr']
            travelled_l += abs(dl); travelled_r += abs(dr)
            distance = .5*(travelled_l/left_scale+travelled_r/right_scale)
            left_v, right_v = (dl/left_scale*1000/dt, dr/right_scale*1000/dt) if dt else (0., 0.)
            expected = {'odometerM': distance, 'leftSpeedMps': left_v, 'rightSpeedMps': right_v,
                        'speedMps': .5*(abs(left_v)+abs(right_v))}
            if (int(got['index']), int(got['sequence']), int(got['ms'])) != (index, row['seq'], row['ms']):
                raise AssertionError('Native identity mismatch')
            if int(got['captureStatus']) != 0:
                raise AssertionError('Known midpoint bracket was rejected')
            fraction = ((int(got['captureMs'])-data['rows'][index-1]['ms']) & 0xffffffff)/dt if dt else 0
            expected['captureOdometerM'] = previous_distance+fraction*(distance-previous_distance)
            for key, value in expected.items():
                if not math.isclose(float(got[key]), value, rel_tol=2e-5, abs_tol=1e-6):
                    raise AssertionError(f'{path.name} row{index} {key}: {got[key]} != {value}')
            previous_distance = distance
        reports.append({'source': str(path), 'sourceSha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                        'rows': len(actual), 'rawTraceCrcVerified': checks['crc_verified'],
                        'allRowsMatchIndependentOracle': True, 'totalWheelPathM': previous_distance,
                        'csvSha256': hashlib.sha256(destination.read_bytes()).hexdigest()})
    result = {'schemaVersion': 'smartcar.metric-odometry-replay/1', 'hardwareAccessed': False,
              'productionApproved': False, 'leftTicksPerM': left_scale, 'rightTicksPerM': right_scale,
              'experiments': reports, 'compilerFamily': family, 'compilerMode': mode, 'compileCommand': command,
              'sourcesSha256': {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in sources+[Path(__file__), ROOT/'demo/code/vision_metric_io.h']},
              'limits': ['Wheel path magnitude; not independent ground displacement or slip measurement.',
                         'Candidate scales are explicit inputs, not approved calibration.',
                         'Capture midpoint queries are synthetic interpolation tests, not measured camera timestamps.',
                         'No new PI, PWM, motion command, physical stopping or target timing evidence.']}
    (output/'report.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'traces': len(reports), 'rows': sum(r['rows'] for r in reports), 'productionApproved': False}))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('traces', type=Path, nargs='+')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--left-ticks-per-m', type=float, required=True)
    parser.add_argument('--right-ticks-per-m', type=float, required=True)
    parser.add_argument('--compiler')
    args = parser.parse_args()
    run(args.traces, args.output, args.left_ticks_per_m, args.right_ticks_per_m, args.compiler)
