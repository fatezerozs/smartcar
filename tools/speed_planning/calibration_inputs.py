"""Offline manual Phase 2 intake. No device access or production profile writes."""
import argparse
import csv
import datetime
import hashlib
import json
import math
from pathlib import Path
import statistics

ROOT = Path(__file__).resolve().parents[2]
GEOMETRY = (
    'wheelbase_left_mm', 'wheelbase_right_mm', 'rear_track_mm',
    'front_track_mm', 'camera_height_mm', 'camera_offset_right_mm',
    'camera_offset_forward_mm',
)
HEADERS = {
    'geometry': 'trial_id,quantity,value_mm,condition_id,instrument_id,measured_at,accepted,notes',
    'rolling': 'trial_id,wheel,direction,turns,distance_mm,condition_id,instrument_id,measured_at,accepted,notes',
    'steering': 'trial_id,pulse_us,sweep_direction,left_angle_right_positive_deg,right_angle_right_positive_deg,condition_id,instrument_id,measured_at,accepted,notes',
    'steeringTriangles': 'trial_id,pulse_us,sweep_direction,left_dx_right_mm,left_dy_forward_mm,right_dx_right_mm,right_dy_forward_mm,condition_id,instrument_id,measured_at,accepted,notes',
}


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('Duplicate JSON key: ' + key)
            result[key] = value
        return result
    return json.loads(path.read_text(encoding='utf-8-sig'), object_pairs_hook=unique,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))


def number(value, name, positive=False):
    if isinstance(value, bool) or value is None:
        raise ValueError('Missing/invalid ' + name)
    try:
        result = float(value)
    except (ValueError, TypeError):
        raise ValueError('Invalid ' + name) from None
    if not math.isfinite(result) or (positive and result <= 0):
        raise ValueError('Non-finite/non-positive ' + name)
    return result


def describe(values):
    return {'count': len(values), 'mean': statistics.mean(values),
            'minimum': min(values), 'maximum': max(values),
            'sampleStandardDeviation': statistics.stdev(values) if len(values) > 1 else None,
            'descriptiveRepeatThresholdMet': len(values) >= 3,
            'metrologicalAcceptancePassed': False}


def analyze(session_path, encoder_path=None):
    session_path = Path(session_path).resolve()
    session = read_json(session_path)
    if session.get('schemaVersion') != 'smartcar.phase2-manual/1' or session.get('productionApproved') is not False:
        raise ValueError('Only unapproved manual Phase 2 sessions are supported')
    if session.get('status') not in ('NOT_MEASURED', 'COLLECTING', 'READY_FOR_REVIEW'):
        raise ValueError('Unknown session status')
    if encoder_path is None:
        encoder_path = (session_path.parent / session['encoderProfile']).resolve()
        if encoder_path != (ROOT / 'profiles/encoder-production-20261106.json').resolve():
            raise ValueError('Encoder source must be the adopted repository profile')
    encoder_path = Path(encoder_path)
    encoder = read_json(encoder_path)
    if encoder.get('schema') != 'smartcar.encoder-production/1' or not encoder.get('production_configuration_written'):
        raise ValueError('Adopted encoder profile required')
    cpr = {side: number(encoder[side]['counts_per_wheel_revolution'], side + ' CPR', True)
           for side in ('left', 'right')}
    hashes = {'session.json': hashlib.sha256(session_path.read_bytes()).hexdigest(),
              'encoderProfile': hashlib.sha256(encoder_path.read_bytes()).hexdigest()}
    accepted, pending, excluded, placeholders = [], [], [], []
    ids = set()
    for kind in HEADERS:
        path = (session_path.parent / session[kind + 'Csv']).resolve()
        if path.parent != session_path.parent or path.suffix.lower() != '.csv':
            raise ValueError('CSV must be directly inside the session directory')
        hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
        with path.open(encoding='utf-8-sig', newline='') as stream:
            reader = csv.DictReader(stream)
            if reader.fieldnames != HEADERS[kind].split(','):
                raise ValueError('Wrong CSV columns: ' + kind)
            for original in reader:
                if None in original or any(value is None for value in original.values()):
                    raise ValueError('Wrong CSV row width: ' + kind)
                row = {key: value.strip() for key, value in original.items()}
                if not any(row.values()):
                    continue
                trial = row['trial_id']
                if not trial or trial in ids:
                    raise ValueError('Missing/duplicate trial ID: ' + trial)
                ids.add(trial)
                if row['accepted'] not in ('', 'true', 'false'):
                    raise ValueError('accepted must be blank, true or false')
                numeric_fields = {'geometry': ['value_mm'], 'rolling': ['turns', 'distance_mm'],
                                  'steering': ['pulse_us', 'left_angle_right_positive_deg',
                                               'right_angle_right_positive_deg'],
                                  'steeringTriangles': ['pulse_us', 'left_dx_right_mm', 'left_dy_forward_mm',
                                                       'right_dx_right_mm', 'right_dy_forward_mm']}[kind]
                if not any(row[field] for field in numeric_fields):
                    if row['accepted'] == 'true':
                        raise ValueError('Cannot accept a blank measurement')
                    if row['accepted'] == 'false':
                        if not row['notes']:
                            raise ValueError('Excluded trial requires a reason')
                        excluded.append({'kind': kind, **row})
                    else:
                        placeholders.append(trial)
                    continue
                values = {field: number(row[field], trial + ':' + field,
                          field in ('turns', 'distance_mm', 'pulse_us', 'left_dy_forward_mm', 'right_dy_forward_mm') or
                          (field == 'value_mm' and row['quantity'] not in GEOMETRY[-2:]))
                          for field in numeric_fields}
                if kind == 'geometry' and row['quantity'] not in GEOMETRY:
                    raise ValueError('Unknown geometry quantity')
                if kind == 'rolling':
                    if row['wheel'] not in cpr or row['direction'] not in ('forward', 'reverse'):
                        raise ValueError('Unknown rolling wheel/direction')
                    if not values['turns'].is_integer() or not 1 <= values['turns'] <= 100:
                        raise ValueError('Record 1..100 operator-counted complete turns')
                if kind in ('steering', 'steeringTriangles') and row['sweep_direction'] not in ('increasing_pulse', 'decreasing_pulse'):
                    raise ValueError('Record the actual steering sweep direction')
                entry = {'kind': kind, **row, 'parsed': values}
                if row['accepted'] == 'false':
                    if not row['notes']:
                        raise ValueError('Excluded trial requires a reason')
                    excluded.append(entry)
                elif row['accepted'] == '':
                    pending.append(entry)
                else:
                    if session['status'] == 'NOT_MEASURED':
                        raise ValueError('A NOT_MEASURED session cannot contain accepted data')
                    for key in ('operator', 'vehicleId', 'conditionId'):
                        if not isinstance(session.get(key), str) or not session[key].strip():
                            raise ValueError('Accepted data requires session ' + key)
                    conditions = session.get('conditions', {})
                    for key in ('otherTestReleasedVehicle', 'actuatorPowerIsolated'):
                        if conditions.get(key) is not True:
                            raise ValueError('Accepted data requires confirmed ' + key)
                    for key in ('surface', 'batteryAndPayload', 'tireState'):
                        if not isinstance(conditions.get(key), str) or not conditions[key].strip():
                            raise ValueError('Accepted data requires conditions.' + key)
                    instrument = session.get('instrument', {})
                    if not isinstance(instrument.get('name'), str) or not instrument['name'].strip():
                        raise ValueError('Instrument name required')
                    resolution = 'angleResolutionDeg' if kind == 'steering' else 'distanceResolutionMm'
                    number(instrument.get(resolution), resolution, True)
                    for key in ('condition_id', 'instrument_id', 'measured_at'):
                        if not row[key]:
                            raise ValueError('Accepted trial requires ' + key)
                    if row['condition_id'] != session['conditionId']:
                        raise ValueError('Conditions cannot be mixed within one session')
                    if row['instrument_id'] != instrument.get('id'):
                        raise ValueError('Instrument identity must match session metadata')
                    measured_at = datetime.datetime.fromisoformat(row['measured_at'])
                    if measured_at.tzinfo is None:
                        raise ValueError('Measurement timestamp requires a timezone')
                    accepted.append(entry)
    geometry, rolling = {}, {}
    for quantity in GEOMETRY:
        values = [r['parsed']['value_mm'] for r in accepted
                  if r['kind'] == 'geometry' and r['quantity'] == quantity]
        if values:
            geometry[quantity] = {'unit': 'mm', **describe(values)}
    for wheel in cpr:
        for direction in ('forward', 'reverse'):
            rows = [r for r in accepted if r['kind'] == 'rolling' and
                    r['wheel'] == wheel and r['direction'] == direction]
            if rows:
                values = [r['parsed']['distance_mm'] / r['parsed']['turns'] for r in rows]
                scales = [cpr[wheel] / (value / 1000) for value in values]
                rolling[wheel + ':' + direction] = {
                    'circumferenceMm': describe(values), 'countsPerMetre': describe(scales),
                    'adoptedCpr': cpr[wheel], 'trialIds': [r['trial_id'] for r in rows]}
    steering = []
    for row in accepted:
        if row['kind'] == 'steeringTriangles':
            values = row['parsed']
            steering.append({
                'trialId': row['trial_id'], 'pulseUs': values['pulse_us'],
                'sweepDirection': row['sweep_direction'], 'method': 'atan2_dx_dy',
                'leftAngleRightPositiveDeg': math.degrees(math.atan2(values['left_dx_right_mm'], values['left_dy_forward_mm'])),
                'rightAngleRightPositiveDeg': math.degrees(math.atan2(values['right_dx_right_mm'], values['right_dy_forward_mm'])),
                'effectiveBicycleAngleRad': None, 'uncertaintyDeg': None,
            })
        elif row['kind'] == 'steering':
            values = row['parsed']
            steering.append({
                'trialId': row['trial_id'], 'pulseUs': values['pulse_us'],
                'sweepDirection': row['sweep_direction'], 'method': 'operator-angle-reading',
                'leftAngleRightPositiveDeg': values['left_angle_right_positive_deg'],
                'rightAngleRightPositiveDeg': values['right_angle_right_positive_deg'],
                'effectiveBicycleAngleRad': None, 'uncertaintyDeg': None,
            })
    missing = [q + ': at least three accepted readings' for q in GEOMETRY
               if geometry.get(q, {}).get('count', 0) < 3]
    missing += [w + ':' + d + ': at least three accepted rolling trials'
                for w in cpr for d in ('forward', 'reverse')
                if rolling.get(w + ':' + d, {}).get('circumferenceMm', {}).get('count', 0) < 3]
    if pending:
        missing.append('Unreviewed numeric rows remain')
    return {
        'schemaVersion': 'smartcar.phase2-manual-summary/1', 'sessionId': session['sessionId'],
        'status': 'INCOMPLETE' if missing else 'STATIC_COMPLETE_PENDING_REVIEW',
        'hardwareAccessedByTool': False, 'productionApproved': False,
        'operator': session.get('operator'), 'vehicleId': session.get('vehicleId'),
        'conditionId': session.get('conditionId'), 'instrument': session.get('instrument'),
        'conditions': session.get('conditions'), 'measurementUncertaintyMm': None,
        'sourceSha256': hashes, 'geometry': geometry, 'rolling': rolling,
        'steeringAngleCandidates': steering,
        'acceptedTrials': accepted, 'pendingTrials': pending, 'excludedTrials': excluded,
        'unmeasuredTemplateTrials': placeholders, 'staticMissing': missing,
        'phase2Complete': False,
        'remaining': ['Reviewed actual wheel-angle map and hysteresis',
                      'Ground-image mapping and independent error checks',
                      'Synchronized dynamic/braking/latency/near-far sessions',
                      'Systematic instrument error, slip/load effects and repeatability review'],
        'limitations': ['Right turn is positive, x-right/y-forward; no steering-angle averaging',
                       'One condition per session; directions and wheels remain separate',
                       'Statistics describe observed repetitions, not a certified error bound',
                       'Counts/metre inherits the adopted CPR repeatability limitations',
                       'No production configuration or historical profiles are written'],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--session', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--require-static-complete', action='store_true')
    args = parser.parse_args()
    result = analyze(args.session)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write('\n')
    print(result['status'] + ': ' + str(len(result['acceptedTrials'])) + ' accepted trials; no hardware accessed')
    return 1 if args.require_static_complete and result['status'] == 'INCOMPLETE' else 0


if __name__ == '__main__':
    raise SystemExit(main())
