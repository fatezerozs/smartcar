"""Offline, versioned optional sidecar; never opens a device or alters SCRAW.

PC timestamps are expanded monotonic milliseconds (unwrap before export).
All distance/speed values are SI, including the accumulated *path* odometer.
Metadata records a claim of origin, not a signature proving measurement quality.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path

SCHEMA = 'smartcar.control-replay/1'
MODES = {'NORMAL_TRACK', 'DEGRADED_FAR_LOSS', 'RECOVERY_NO_LANE',
         'CONTROLLED_STOP', 'STOP_LATCHED', 'FAULT_LATCHED', 'DISARMED'}


def _unique(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise ValueError('Duplicate JSON key: ' + key)
        value[key] = item
    return value


def _number(value, name, low=0):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or value < low:
        raise ValueError('Invalid ' + name)
    return value


def _integer(value, name):
    if type(value) is not int or value < 0:
        raise ValueError('Invalid ' + name)
    return value


def load_session(path):
    path = Path(path)
    raw = path.read_bytes()
    lines = [json.loads(line, object_pairs_hook=_unique,
                        parse_constant=lambda s: (_ for _ in ()).throw(ValueError(s)))
             for line in raw.decode('utf-8-sig').splitlines() if line.strip()]
    if len(lines) < 2 or not all(isinstance(item, dict) for item in lines):
        raise ValueError('Header and samples required')
    header, samples = lines[0], lines[1:]
    required_header = {'schemaVersion', 'sessionId', 'datasetVersion', 'units', 'provenance', 'imageSha256'}
    if set(header) != required_header or header['schemaVersion'] != SCHEMA or header['units'] != 'SI':
        raise ValueError('Unsupported sidecar header/version/units')
    if header['provenance'] not in ('simulation', 'project-measured'):
        raise ValueError('Missing provenance')
    for key in ('sessionId', 'datasetVersion'):
        if not isinstance(header[key], str) or not header[key].strip():
            raise ValueError('Missing ' + key)
    digest = header['imageSha256']
    if not isinstance(digest, str) or len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
        raise ValueError('Invalid image archive SHA-256')
    required = {'frameSeq', 'captureMs', 'nearValid', 'farValid', 'odometerM', 'speedMeasuredMps',
                'speedReferenceMps', 'leftTargetTicksS', 'rightTargetTicksS', 'leftPwmPermille',
                'rightPwmPermille', 'mode', 'outputGeneration'}
    optional = {'nearDistanceM', 'curvaturePerM', 'queueDrops', 'storageErrors'}
    previous = None
    for sample in samples:
        if not required <= sample.keys() or sample.keys() - required - optional:
            raise ValueError('Unsupported/missing sample fields')
        for key in ('frameSeq', 'captureMs', 'outputGeneration'):
            _integer(sample[key], key)
        for key in ('nearValid', 'farValid'):
            if type(sample[key]) is not bool:
                raise ValueError('Invalid ' + key)
        for key in ('odometerM', 'speedMeasuredMps', 'speedReferenceMps', 'leftTargetTicksS', 'rightTargetTicksS'):
            _number(sample[key], key)
        for key in ('leftPwmPermille', 'rightPwmPermille'):
            if _integer(sample[key], key) > 1000:
                raise ValueError('PWM outside 0..1000')
        for key in optional & sample.keys():
            _number(sample[key], key, -math.inf if key == 'curvaturePerM' else 0)
        if sample['mode'] not in MODES:
            raise ValueError('Unknown control mode')
        if previous and (sample['captureMs'] <= previous['captureMs'] or sample['frameSeq'] <= previous['frameSeq'] or
                         sample['odometerM'] < previous['odometerM']):
            raise ValueError('Sequence/time/odometer not monotonic; unwrap before import')
        previous = sample
    return {'header': header, 'samples': samples, 'sha256': hashlib.sha256(raw).hexdigest()}


def episodes(samples, recovery_frames, max_gap_ms):
    if type(recovery_frames) is not int or recovery_frames < 1 or max_gap_ms <= 0:
        raise ValueError('Recovery frames and frame freshness required')
    found, active, streak, previous = [], None, 0, None
    for sample in samples:
        if previous and sample['captureMs'] - previous['captureMs'] >= max_gap_ms:
            if active:
                found.append(dict(active, censored=True, endSeq=previous['frameSeq'],
                                  durationMs=previous['captureMs']-active['startMs'],
                                  distanceM=previous['odometerM']-active['startOdometerM']))
            active, streak = None, 0
        if active is None and sample['nearValid'] and not sample['farValid']:
            active = {'startSeq': sample['frameSeq'], 'startMs': sample['captureMs'],
                      'startOdometerM': sample['odometerM']}
        if active:
            streak = streak+1 if sample['nearValid'] and sample['farValid'] else 0
            if streak >= recovery_frames:
                found.append(dict(active, censored=False, endSeq=sample['frameSeq'],
                                  durationMs=sample['captureMs']-active['startMs'],
                                  distanceM=sample['odometerM']-active['startOdometerM']))
                active, streak = None, 0
        previous = sample
    if active:
        found.append(dict(active, censored=True, endSeq=previous['frameSeq'],
                          durationMs=previous['captureMs']-active['startMs'],
                          distanceM=previous['odometerM']-active['startOdometerM']))
    return found


def quantile_nearest(values, q):
    if not values:
        raise ValueError('No complete episodes')
    ordered = sorted(values)
    return ordered[max(0, math.ceil(q*len(ordered))-1)]


def budget_report(train, validation, *, recovery_frames, frame_period_ms, latency_ms,
                  distance_margin_m, max_gap_ms, p99_min_samples=100):
    if not train or not validation:
        raise ValueError('Separate complete training and validation sessions required')
    ids = [s['header']['sessionId'] for s in train+validation]
    hashes = [s['sha256'] for s in train+validation]
    image_hashes = [s['header']['imageSha256'] for s in train+validation]
    if len(set(ids)) != len(ids) or len(set(hashes)) != len(hashes) or len(set(image_hashes)) != len(image_hashes):
        raise ValueError('Session/image overlap or duplicates; adjacent-frame splits forbidden')
    versions = {s['header']['datasetVersion'] for s in train+validation}
    origins = {s['header']['provenance'] for s in train+validation}
    if len(versions) != 1 or len(origins) != 1:
        raise ValueError('Mixed dataset version/provenance')
    for name, value in [('frame_period_ms', frame_period_ms), ('latency_ms', latency_ms),
                        ('distance_margin_m', distance_margin_m), ('max_gap_ms', max_gap_ms)]:
        _number(value, name)
    if frame_period_ms <= 0 or max_gap_ms <= frame_period_ms or type(p99_min_samples) is not int or p99_min_samples < 1:
        raise ValueError('Invalid sampling policy')
    def extract(group):
        return [dict(e, sessionId=s['header']['sessionId']) for s in group
                for e in episodes(s['samples'], recovery_frames, max_gap_ms)]
    training, heldout = extract(train), extract(validation)
    complete = [e for e in training if not e['censored']]
    if not complete:
        raise ValueError('No complete training episodes; no budget can be fitted')
    provisional = len(complete) < p99_min_samples
    q = 1 if provisional else .99
    time_base = quantile_nearest([e['durationMs'] for e in complete], q)
    distance_base = quantile_nearest([e['distanceM'] for e in complete], q)
    time_budget = time_base+max(2*frame_period_ms, latency_ms)
    distance_budget = distance_base+distance_margin_m
    valid = [e for e in heldout if not e['censored']]
    # Exact deadline wins: restoration at equality is already too late.
    covered = sum(e['durationMs'] < time_budget and e['distanceM'] < distance_budget for e in valid)
    return {'schemaVersion': 'smartcar.far-budget/1', 'productionApproved': False,
            'evidence': 'simulation' if origins == {'simulation'} else 'offline-measurement-analysis',
            'method': 'temporary-maximum-plus-margin' if provisional else 'empirical-nearest-rank-P99-plus-margin',
            'p99MinSamplesPolicy': p99_min_samples, 'sampleCount': len(complete),
            'trainCensoredCount': len(training)-len(complete), 'timeBaseMs': time_base,
            'distanceBaseM': distance_base, 'timeBudgetMs': time_budget, 'distanceBudgetM': distance_budget,
            'recoveryFrames': recovery_frames, 'framePeriodMs': frame_period_ms, 'latencyMs': latency_ms,
            'distanceMarginM': distance_margin_m, 'maxGapMs': max_gap_ms,
            'validationCompleteCount': len(valid), 'validationCensoredCount': len(heldout)-len(valid),
            'coverage': covered/len(valid) if valid else None,
            'budgetWouldExpireFraction': 1-covered/len(valid) if valid else None,
            'falseStopRate': None,  # Requires independently labeled recoverable / unsafe events.
            'sessions': [{'id': s['header']['sessionId'], 'sha256': s['sha256'],
                          'imageSha256': s['header']['imageSha256']} for s in train+validation],
            'trainEpisodes': training, 'validationEpisodes': heldout}


def safety_and_tracking(session):
    rows = session['samples']
    near_violations = [s['frameSeq'] for s in rows if not s['nearValid'] and any(s[k] for k in
                       ('leftTargetTicksS', 'rightTargetTicksS', 'leftPwmPermille', 'rightPwmPermille'))]
    errors = [s['speedReferenceMps']-s['speedMeasuredMps'] for s in rows]
    accel = [(b['speedReferenceMps']-a['speedReferenceMps'])*1000/(b['captureMs']-a['captureMs'])
             for a,b in zip(rows,rows[1:])]
    jerk = [(b-a)*2000/(rows[n+2]['captureMs']-rows[n]['captureMs'])
            for n,(a,b) in enumerate(zip(accel,accel[1:]))]
    return {'nearInvalidNonzeroFrames': near_violations,
            'rmseMps': math.sqrt(sum(e*e for e in errors)/len(errors)),
            'maxAbsTrackingErrorMps': max(map(abs, errors)),
            'maxAbsReferenceAccelMps2': max(map(abs, accel), default=0),
            'maxAbsReferenceJerkMps3': max(map(abs, jerk), default=0),
            'queueDropsReportedMaximum': max((s.get('queueDrops',0) for s in rows), default=0),
            'storageErrorsReportedMaximum': max((s.get('storageErrors',0) for s in rows), default=0)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--train', type=Path, nargs='+', required=True)
    parser.add_argument('--validation', type=Path, nargs='+', required=True)
    parser.add_argument('--recovery-frames', type=int, required=True)
    parser.add_argument('--frame-period-ms', type=float, required=True)
    parser.add_argument('--latency-ms', type=float, required=True)
    parser.add_argument('--distance-margin-m', type=float, required=True)
    parser.add_argument('--max-gap-ms', type=float, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    train = list(map(load_session,args.train));heldout = list(map(load_session,args.validation))
    report = budget_report(train,heldout,recovery_frames=args.recovery_frames,frame_period_ms=args.frame_period_ms,
                           latency_ms=args.latency_ms,distance_margin_m=args.distance_margin_m,max_gap_ms=args.max_gap_ms)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    print('Offline proposal only; productionApproved=false; '+str(args.output))


if __name__ == '__main__':
    main()
