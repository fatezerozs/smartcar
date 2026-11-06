"""Read and verify the current parameter handoff without device access.

B3T1 windows verify native counts and operator-reported turns. They are not
motion sessions, metric speed calibration, or near/far budget training data.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verify_b3_static import decode, fields
from speed_planning.vehicle_model import ROOT

DELIVERY = ROOT / 'docs/changes/vehicle-parameters-20261106'
ENCODER_PROFILE = ROOT / 'profiles/encoder-production-20261106.json'


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('Duplicate JSON key: ' + key)
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding='utf-8-sig'), object_pairs_hook=unique,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def child(directory, name):
    directory = Path(directory).resolve()
    path = (directory / name).resolve()
    if not path.is_relative_to(directory) or path == directory:
        raise ValueError('Input path escapes directory')
    return path


def load_encoder(path=ENCODER_PROFILE, root=ROOT):
    data = read_json(path)
    if data['schema'] != 'smartcar.encoder-production/1' or data['production_configuration_written'] is not True:
        raise ValueError('Expected adopted encoder production record')
    source = child(root, data['source_profile']).read_text(encoding='utf-8')
    for wheel, connector, sign in [('left', 3, -1), ('right', 4, 1)]:
        item = data[wheel]
        cpr = item['counts_per_wheel_revolution']
        if isinstance(cpr, bool) or not isinstance(cpr, (int, float)) or not math.isfinite(cpr) or cpr <= 0:
            raise ValueError('Invalid wheel CPR')
        if item['connector'] != connector or type(item['forward_raw_sign']) is not int or item['forward_raw_sign'] != sign:
            raise ValueError('Encoder mapping/sign mismatch')
        turns, counts = item['operator_forward_turns'], item['observed_counts']
        if type(turns) is not int or not 1 <= turns <= 100 or type(counts) is not int or counts * sign <= 0:
            raise ValueError('Invalid operator turn record')
        if not math.isclose(abs(counts) / turns, cpr, abs_tol=1e-8, rel_tol=0):
            raise ValueError('CPR disagrees with observed turns')
        prefix = 'MT_ROUTE_ENCODER_' + wheel.upper()
        match = re.search(r'^#define\s+' + prefix + r'_CPR_X10\s+(\d+)U\s*$', source, re.M)
        sign_match = re.search(r'^#define\s+' + prefix + r'_FORWARD_SIGN\s+\(?(-?\d+)\)?\s*$', source, re.M)
        if not match or int(match[1]) != round(cpr * 10) or not math.isclose(cpr*10, round(cpr*10), abs_tol=1e-7):
            raise ValueError('Production source CPR drift')
        if not sign_match or int(sign_match[1]) != sign:
            raise ValueError('Production source direction drift')
    return data


def verify_manifest(directory):
    manifest = read_json(Path(directory) / 'MANIFEST.json')
    if manifest['schema'] != 'smartcar.local-handoff-manifest/1':
        raise ValueError('Unknown handoff manifest')
    seen = set()
    for item in manifest['files']:
        path = child(directory, item['path'])
        if path in seen:
            raise ValueError('Duplicate manifest path')
        seen.add(path)
        if path.stat().st_size != item['bytes'] or sha256(path) != item['sha256']:
            raise ValueError('Handoff hash/size mismatch: ' + item['path'])
    return seen


def inspect_trace(directory):
    directory = Path(directory)
    header = (directory / 'header.txt').read_text(encoding='utf-8')
    metadata = fields(header)
    report, rows = decode(header, (directory / 'trace.b3t').read_bytes())
    if report['status'] != 'complete' or report['synthetic'] or report['c26_raw_levels_seen'] != [1]:
        raise ValueError('Expected complete hardware C26 OFF trace')
    if not report['all_three_actuator_snapshots_present']:
        raise ValueError('Missing actuator snapshots')
    samples = [row for row in rows if row['kind'] == 'encoder_sample']
    if len(samples) != 500:
        raise ValueError('Expected 500 samples per delivered window')
    references = {row['ordinal']: row for row in rows if row['kind'] == 'raw_counter_read'}
    for sample in samples:
        raw = references[sample['v11']]
        if raw['timestamp_ms_extended'] != sample['timestamp_ms_extended']:
            raise ValueError('Raw/sample timestamp mismatch')
        if sample['v1'] & 0xffffffff != sample['timestamp_ms_extended'] & 0xffffffff:
            raise ValueError('Native sample timestamp mismatch')
        if any(abs(sample[key]) >= 2147483647 for key in ('v4', 'v5')):
            raise ValueError('Saturated cumulative counter')
    for before, after in zip(samples, samples[1:]):
        if (after['v0'] - before['v0']) & 0xffffffff != 1 or (after['v1'] - before['v1']) & 0xffffffff != 10:
            raise ValueError('Native sample gap')
        a, b = references[before['v11']], references[after['v11']]
        for raw_key, total_key, tick_key in [('v0', 'v4', 'v2'), ('v1', 'v5', 'v3')]:
            delta = ((b[raw_key] - a[raw_key] + 32768) % 65536) - 32768
            if abs(delta) == 32768 or after[total_key] - before[total_key] != delta or after[tick_key] != delta:
                raise ValueError('Raw/total/native tick disagreement')
    last = samples[-1]
    raw = references[last['v11']]
    return {'sha256': sha256(directory / 'trace.b3t'), 'source': metadata['source'],
            'samples': len(samples), 'first_ms': samples[0]['timestamp_ms_extended'],
            'last_ms': last['timestamp_ms_extended'], 'sample_sequence': last['v0'],
            'total_left_right': [last['v4'], last['v5']], 'raw_left_right': [raw['v0'], raw['v1']],
            'samplePeriodMs': 10, 'observedAppliedPwmZero': report['all_observed_applied_pwm_zero'],
            'observedNativeCounts10msRange': [[min(r[k] for r in samples), max(r[k] for r in samples)] for k in ('v2', 'v3')]}


def audit(directory=DELIVERY, encoder_path=ENCODER_PROFILE, root=ROOT):
    directory = Path(directory)
    verified = verify_manifest(directory)
    required = ['parameters.json', 'evidence/encoder-measurement-ledger.json',
                'evidence/b2-final-acceptance.json', 'evidence/joint-build-evidence.json']
    if any(child(directory, name) not in verified for name in required):
        raise ValueError('Required handoff file absent from manifest')
    current = load_encoder(encoder_path, root)
    parameters = read_json(directory / 'parameters.json')
    ledger = read_json(directory / 'evidence/encoder-measurement-ledger.json')
    if parameters['encoder_production'] != current or ledger['adopted_production_profile'] != current:
        raise ValueError('Current encoder record differs from delivered record; review new handoff')
    windows = {}
    for ident, recorded in ledger['windows'].items():
        window_dir = child(directory / 'evidence/encoder', ident)
        if any((window_dir / name).resolve() not in verified for name in ('header.txt', 'trace.b3t')):
            raise ValueError('Trace missing from manifest')
        observed = inspect_trace(window_dir)
        for key in ('sha256', 'source', 'samples', 'first_ms', 'last_ms', 'sample_sequence', 'total_left_right', 'raw_left_right'):
            if observed[key] != recorded[key]:
                raise ValueError('Window disagrees with measurement ledger: ' + ident + '/' + key)
        windows[ident] = observed
    for trial in ledger['trials']:
        before, after = [windows[trial[key]] for key in ('before', 'after')]
        if before['source'] != after['source'] or after['first_ms'] <= before['last_ms'] or after['sample_sequence'] <= before['sample_sequence']:
            raise ValueError('Trial continuity mismatch')
        delta = [b-a for a,b in zip(before['total_left_right'], after['total_left_right'])]
        if delta != trial['delta_left_right']:
            raise ValueError('Trial count mismatch')
        for side in range(2):
            if delta[side] % 65536 != (after['raw_left_right'][side] - before['raw_left_right'][side]) % 65536:
                raise ValueError('Trial modular counter mismatch')
        wheel = trial['operator_wheel']
        side = ('left', 'right').index(wheel)
        turns = trial['operator_turns']
        if type(turns) is not int or not 1 <= turns <= 100:
            raise ValueError('Invalid trial turn count')
        if not math.isclose(abs(delta[side])/turns, trial['counts_per_turn'], rel_tol=0, abs_tol=1e-8):
            raise ValueError('Trial CPR mismatch')
        direction = trial['operator_direction']
        if direction not in ('forward', 'reverse'):
            raise ValueError('Unknown operator direction')
        expected_sign = current[wheel]['forward_raw_sign'] * (1 if direction == 'forward' else -1)
        if delta[side]*expected_sign <= 0 or trial['direction_matches_config'] is not True:
            raise ValueError('Trial direction mismatch')
        if trial['status'].startswith('user_adopted_for_production'):
            if delta[side] != current[wheel]['observed_counts'] or turns != current[wheel]['operator_forward_turns']:
                raise ValueError('Adopted trial mismatch')
    b2 = read_json(directory / 'evidence/b2-final-acceptance.json')
    joint = read_json(directory / 'evidence/joint-build-evidence.json')
    return {'schemaVersion': 'smartcar.planning-data-inputs/1', 'hardwareAccessed': False,
            'manifestFilesVerified': len(verified), 'manifestSha256': sha256(directory / 'MANIFEST.json'),
            'encoderProfileSha256': sha256(encoder_path), 'encoder': current, 'windows': windows,
            'verifiedNativeSamples': sum(w['samples'] for w in windows.values()),
            'operatorTurnTrialsRecomputed': len(ledger['trials']),
            'readiness': {'wheelCprAdopted': True, 'nativeCountReplay': True,
                          'measuredMetricSpeed': False,
                          'actualWheelAngleMap': False, 'motionNearFarBudgetDataset': False,
                          'productionPlannerIntegrationAccepted': False},
            'b2Snapshot': {'sessionId': b2['session_id'], 'frames': b2['frames'],
                           'scope': b2['scope'], 'fullImageSessionReverifiedThisRun': False},
            'jointSnapshot': {'buildPassed': joint['build_passed'], 'hardwareJointTestAcceptedByThisAudit': False},
            'limitations': ['CPR adoption retains recorded repeatability limitations.',
                            'Operator turn counts and no intervening reset remain operator evidence.',
                            'B3 windows bracket manual turns; no continuous hand-turn trajectory is invented.',
                            'B2 is standalone stationary image/SD evidence; no cross-session telemetry join.',
                            'Nominal circumference can support an explicit reference, never measured metric speed.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = audit()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False)+'\n', encoding='utf-8')
    print(f"Data inputs PASS: {result['manifestFilesVerified']} hashes, {len(result['windows'])} windows, "
          f"{result['verifiedNativeSamples']} samples, {result['operatorTurnTrialsRecomputed']} turn trials")


if __name__ == '__main__':
    main()
