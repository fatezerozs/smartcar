"""Compare complete-course times from explicitly reviewed external recordings.

Never controls hardware or infers completion from UART/TRACE spans. Review
assertions are supplied by an operator; this tool checks consistency/integrity,
not the visual truth of a recording or which binary is actually on the board.
"""
import argparse
import hashlib
import json
import re
import statistics
from collections import Counter, defaultdict
from pathlib import Path


def integer(value, minimum=0):
    return type(value) is int and value >= minimum


def sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def course_fingerprint(course):
    if not isinstance(course, dict):
        raise ValueError('course must be an object')
    for key in ('id', 'start_rule', 'finish_rule', 'direction', 'conditions'):
        if not isinstance(course.get(key), str) or not course[key].strip():
            raise ValueError('Unconfirmed course field: ' + key)
    if type(course.get('include_stops')) is not bool:
        raise ValueError('Confirm include_stops before timing acceptance')
    passages = course.get('required_passages')
    if not isinstance(passages, dict) or not passages:
        raise ValueError('Specify required_passages before timing acceptance')
    if any(not isinstance(k, str) or not k.strip() or not integer(v, 1)
           for k, v in passages.items()):
        raise ValueError('Each required passage needs a nonempty name and positive count')
    normalized = json.dumps(course, sort_keys=True, ensure_ascii=False, separators=(',', ':'))
    return hashlib.sha256(normalized.encode('utf-8')).hexdigest()


def evaluate_trial(course, trial, root):
    fingerprint = course_fingerprint(course)
    if not isinstance(trial, dict):
        raise ValueError('Each trial must be an object')
    result = {'id': trial.get('id'), 'firmware': trial.get('firmware'),
              'firmware_sha256': trial.get('firmware_sha256'),
              'status': 'pending', 'reasons': [], 'elapsed_ms': None,
              'evidence_basis': 'operator_review; no automatic video/board verification'}
    pending, invalid = [], []
    if not isinstance(trial.get('id'), str) or not trial['id'].strip():
        pending.append('trial_id_missing')
    if trial.get('course_fingerprint') != fingerprint:
        pending.append('different_or_unconfirmed_course')
    if not isinstance(trial.get('firmware'), str) or not trial['firmware'].strip():
        pending.append('firmware_missing')
    if not isinstance(trial.get('firmware_sha256'), str) or not re.fullmatch(
            r'[0-9a-fA-F]{64}', trial['firmware_sha256']):
        pending.append('firmware_sha256_missing')
    review = trial.get('review')
    if not isinstance(review, dict):
        review = {}
    if not isinstance(review.get('reviewer'), str) or not review['reviewer'].strip():
        pending.append('reviewer_missing')
    for field in ('continuous_recording', 'on_track', 'route_complete'):
        if review.get(field) is False:
            invalid.append(field + '_false')
        elif review.get(field) is not True:
            pending.append(field + '_not_reviewed')
    start, finish = trial.get('start_ms'), trial.get('finish_ms')
    timed = integer(start) and integer(finish) and finish > start
    if timed:
        result['elapsed_ms'] = finish - start
    else:
        pending.append('external_timing_missing_or_invalid')
    uncertainty = trial.get('timing_uncertainty_ms')
    if not integer(uncertainty, 1):
        pending.append('timing_uncertainty_missing')
    else:
        result['timing_uncertainty_ms'] = uncertainty
    evidence = trial.get('evidence')
    if not isinstance(evidence, dict):
        evidence = {}
    path, expected = evidence.get('file'), evidence.get('sha256')
    if not isinstance(path, str) or not path.strip():
        pending.append('recording_missing')
    else:
        path = (Path(root) / path).resolve()
        try:
            actual = sha256_file(path)
            result['evidence_file'] = str(path)
            result['evidence_sha256'] = actual
            if not isinstance(expected, str) or actual != expected.lower():
                pending.append('recording_hash_unconfirmed_or_changed')
        except OSError:
            pending.append('recording_unavailable')
    coverage = review.get('passages')
    if not isinstance(coverage, list):
        coverage = []
    intervals = defaultdict(list)
    for event in coverage:
        if not isinstance(event, dict) or not isinstance(event.get('name'), str) or event['name'] not in course['required_passages']:
            pending.append('unknown_or_malformed_passage')
            continue
        a, b = event.get('from_ms'), event.get('to_ms')
        if not timed or not integer(a) or not integer(b) or not start <= a < b <= finish:
            pending.append('passage_outside_timed_run_or_invalid')
            continue
        intervals[event['name']].append((a, b))
    counts = {}
    for name, required in course['required_passages'].items():
        spans = sorted(intervals[name])
        # Duplicate/overlapping annotations of the same passage cannot count
        # as a second orbit. Different spatial passages may overlap in time.
        if any(right[0] < left[1] for left, right in zip(spans, spans[1:])):
            pending.append('overlapping_passage_annotations:' + name)
        counts[name] = len(spans)
        if len(spans) < required:
            pending.append('required_passage_not_documented:' + name)
    result['passage_counts'] = counts
    stop_rows = review.get('stops')
    if review.get('stops_reviewed') is not True or not isinstance(stop_rows, list):
        pending.append('stops_not_reviewed')
        stop_rows = []
    stops = []
    for stop in stop_rows:
        if not isinstance(stop, dict):
            pending.append('malformed_stop')
            continue
        a, b = stop.get('from_ms'), stop.get('to_ms')
        if not timed or not integer(a) or not integer(b) or not start <= a < b <= finish:
            pending.append('stop_outside_timed_run_or_invalid')
        elif not isinstance(stop.get('reason'), str) or not stop['reason'].strip():
            pending.append('stop_reason_missing')
        else:
            stops.append((a, b))
    stops.sort()
    if any(right[0] < left[1] for left, right in zip(stops, stops[1:])):
        pending.append('overlapping_stop_annotations')
    # Union prevents double subtraction even in a malformed, ineligible input.
    merged = []
    for a, b in stops:
        if merged and a <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], b)
        else:
            merged.append([a, b])
    result['fault_stops'] = stop_rows
    result['fault_stop_count'] = len(merged)
    if timed and review.get('stops_reviewed') is True:
        result['stopped_ms'] = sum(b - a for a, b in merged)
        result['moving_ms'] = result['elapsed_ms'] - result['stopped_ms']
        result['score_ms'] = result['elapsed_ms'] if course['include_stops'] else result['moving_ms']
        if result['moving_ms'] <= 0:
            pending.append('no_positive_motion_time')
    result['reasons'] = sorted(set(invalid + pending))
    if invalid:
        result['status'] = 'invalid'
    elif not pending:
        result['status'] = 'timed_with_faults' if stops else 'eligible_by_operator_review'
    if timed and integer(uncertainty, 1):
        result['elapsed_interval_ms'] = [max(0, finish - start - uncertainty),
                                         finish - start + uncertainty]
        if 'score_ms' in result:
            result['score_interval_ms'] = [max(0, result['score_ms'] - uncertainty),
                                            result['score_ms'] + uncertainty]
    return result


def evaluate(course, trials, root):
    fingerprint = course_fingerprint(course)
    if not isinstance(trials, list):
        raise ValueError('trials must be a list')
    ids = [t.get('id') for t in trials if isinstance(t, dict)]
    if any(n > 1 for n in Counter(v for v in ids if isinstance(v, str)).values()):
        raise ValueError('Duplicate trial ids; do not count one attempt twice')
    rows = [evaluate_trial(course, trial, root) for trial in trials]
    eligible = sorted((r for r in rows if r['status'] == 'eligible_by_operator_review'),
                      key=lambda r: (r['score_ms'], r['id']))
    for row in eligible:
        row['definitely_faster_than'] = [other['id'] for other in eligible
            if row['score_interval_ms'][1] < other['score_interval_ms'][0]]
    grouped = defaultdict(list)
    for row in rows:
        grouped[(str(row['firmware']), str(row['firmware_sha256']).lower())].append(row)
    builds = []
    for (name, checksum), attempts in grouped.items():
        statuses = Counter(r['status'] for r in attempts)
        times = [r['score_ms'] for r in attempts if r['status'] == 'eligible_by_operator_review']
        builds.append({'firmware': name, 'sha256': checksum, 'attempts': len(attempts),
                       'status_counts': dict(statuses),
                       'best_score_ms': min(times) if times else None,
                       'median_score_ms': statistics.median(times) if times else None,
                       'fault_stop_count': sum(r['fault_stop_count'] for r in attempts)})
    return {'course_fingerprint': fingerprint, 'results': rows, 'builds': builds,
            'ranked_eligible_trials': [r['id'] for r in eligible],
            'fastest_reported_trial': eligible[0]['id'] if eligible else None,
            'automatic_course_completion_verified': False,
            'score': 'elapsed_ms' if course['include_stops'] else 'moving_ms',
            'note': 'Stops are recorded faults, never hidden by subtraction. Faulted timings '
                    'remain diagnostic and are excluded from clean-result ranking. External '
                    'review assertions must be checked against actual evidence.'}


def template():
    return {'course': {'id': None, 'start_rule': None, 'finish_rule': None, 'direction': None,
                       'conditions': None, 'include_stops': None, 'required_passages': {}},
            'trials': []}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('--init', action='store_true', help='Create an unconfirmed empty template')
    parser.add_argument('--fingerprint', action='store_true', help='Print confirmed course fingerprint')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.init:
        with args.manifest.open('x', encoding='utf-8') as stream:
            json.dump(template(), stream, ensure_ascii=False, indent=2)
            stream.write('\n')
        return
    data = json.loads(args.manifest.read_text(encoding='utf-8-sig'))
    if args.fingerprint:
        print(course_fingerprint(data['course']))
        return
    result = evaluate(data['course'], data['trials'], args.manifest.resolve().parent)
    rendered = json.dumps(result, ensure_ascii=False, indent=2) + '\n'
    if args.output:
        # Source manifests and original evidence must never be overwritten.
        protected = {args.manifest.resolve()}
        protected.update(Path(r['evidence_file']) for r in result['results'] if 'evidence_file' in r)
        if args.output.resolve() in protected:
            raise ValueError('Output would overwrite source manifest or evidence')
        with args.output.open('x', encoding='utf-8') as stream:
            stream.write(rendered)
    print(rendered, end='')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, TypeError, OSError) as error:
        raise SystemExit('Cannot evaluate trials: ' + str(error))
