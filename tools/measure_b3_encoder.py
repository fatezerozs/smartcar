"""Compare two stationary B3 snapshots around an operator-counted wheel turn.

No serial access. Raw traces remain authoritative; operator circles/direction
are explicit inputs, never inferred from nominal encoder labels.
"""
import argparse
import hashlib
import json
from pathlib import Path

from verify_b3_static import decode


def inspect_window(directory):
    p = Path(directory).resolve()
    raw = (p / 'trace.b3t').read_bytes()
    report, rows = decode((p / 'header.txt').read_text(), raw)
    if report['status'] != 'complete' or report['synthetic']:
        raise ValueError('Incomplete or synthetic trace')
    if report['c26_raw_levels_seen'] != [1] or not report['all_three_actuator_snapshots_present']:
        raise ValueError('Requires C26 OFF and all three actuator snapshots')
    samples = [r for r in rows if r['kind'] == 'encoder_sample']
    if len(samples) < 400:
        raise ValueError('Insufficient stationary samples')
    ranges = [[min(r['v'+str(j)] for r in samples), max(r['v'+str(j)] for r in samples)] for j in (4, 5)]
    total = [samples[-1]['v4'], samples[-1]['v5']]
    if any(abs(t) >= 2147483647 for limits in ranges for t in limits):
        raise ValueError('Saturated cumulative counter')
    references = {r['ordinal']: r for r in rows if r['kind'] == 'raw_counter_read'}
    last = samples[-1]
    counters = references[last['v11']]
    return {'directory': str(p), 'sha256': hashlib.sha256(raw).hexdigest(),
            'source': report['source_input_digest'], 'samples': len(samples),
            'first_ms': samples[0]['timestamp_ms_extended'],
            'last_ms': last['timestamp_ms_extended'],
            'sample_sequence': last['v0'], 'total_left_right': total,
            'raw_left_right': [counters['v0'], counters['v1']],
            'total_ranges_left_right': ranges,
            'stationary_both_wheels': all(a == b for a, b in ranges) and not any(r['v2'] or r['v3'] for r in samples)}


def snapshot(directory):
    result = inspect_window(directory)
    if not result['stationary_both_wheels']:
        raise ValueError('Wheel movement during snapshot; obtain another stable snapshot')
    return result


def compare(before, after, wheel, direction, turns):
    if not 1 <= turns <= 100:
        raise ValueError('Operator must specify 1..100 complete revolutions')
    if before['source'] != after['source']:
        raise ValueError('Firmware changed')
    if after['first_ms'] <= before['last_ms'] or after['sample_sequence'] <= before['sample_sequence']:
        raise ValueError('Boot/sample continuity not established')
    delta = [b-a for a, b in zip(before['total_left_right'], after['total_left_right'])]
    for i in range(2):
        if delta[i] % 65536 != (after['raw_left_right'][i]-before['raw_left_right'][i]) % 65536:
            raise ValueError('Absolute counter and cumulative delta disagree')
    i = 0 if wheel == 'left' else 1
    if not delta[i] or delta[1-i]:
        raise ValueError('Expected one moving wheel only; inspect zero count or opposite wheel movement')
    sign = 1 if delta[i] > 0 else -1
    return {'evidence_layer': 'computed from CRC-verified hardware trace and operator full-turn report',
            'before': before, 'after': after, 'operator_wheel': wheel,
            'operator_direction': direction, 'operator_complete_turns': turns,
            'delta_left_right': delta, 'effective_wheel_cpr': abs(delta[i])/turns,
            'forward_raw_sign': sign if direction == 'forward' else -sign,
            'direction_multiplier_for_forward_positive': sign if direction == 'forward' else -sign,
            'accepted_final_calibration': False,
            'limits': ['Requires reverse-direction and repeat agreement before final calibration',
                       'Full turns and no intervening reset are operator observations',
                       'No loaded rolling circumference or physical speed calibration']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before')
    parser.add_argument('after')
    parser.add_argument('--wheel', required=True, choices=['left', 'right'])
    parser.add_argument('--direction', required=True, choices=['forward', 'reverse'])
    parser.add_argument('--turns', required=True, type=int)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    result = compare(snapshot(args.before), snapshot(args.after), args.wheel, args.direction, args.turns)
    with Path(args.output).open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
    print(json.dumps(result, indent=2))
