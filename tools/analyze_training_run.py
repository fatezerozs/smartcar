"""Summarize a stopped training run without accessing hardware.
Legacy RUN pwm is controller demand; C29 RUN duty is demand and pwm is output.
Encoder samples are sparse telemetry, so no distance or m/s is inferred.
"""
import argparse
import json
import re
from pathlib import Path

RUN = re.compile(r'\[RUN\] n=\d+ ms=(\d+) e=([-\d.]+) h=([-\d.]+) u=([-\d.]+) pwm=([-\d.]+) q=(\d+) age=(\d+)')
SPEED = re.compile(r'\[SPEED\] ms=(\d+) sample=\d+ enabled=(\d+) raw3_L=(-?\d+) raw4_R=(-?\d+) target_R/L=(-?\d+)/(-?\d+) measured_R/L=(-?\d+)/(-?\d+) pwm_R/L=(\d+)/(\d+) faults=(\d+)')
STOP = re.compile(r'\[(?:LINK|FIELD)\] STOP')
COMPACT = re.compile(r'\[RUN\]([^\r\n]*\bscale=1000\b[^\r\n]*)')


def read_samples(text):
    """Normalize both protocols; absent compact raw counters remain missing."""
    runs = [(m.start(), [float(v) for v in m.groups()]) for m in RUN.finditer(text)]
    speeds = [(m.start(), [int(v) for v in m.groups()]) for m in SPEED.finditer(text)]
    for match in COMPACT.finditer(text):
        values = dict(re.findall(r'([A-Za-z_]+)=([^\s;]+)', match.group(1)))
        try:
            ms, n, age, q = (int(values[k]) for k in ('ms', 'n', 'age', 'q'))
            e, h, u, duty = (int(values[k]) / 1000 for k in ('e', 'h', 'u', 'duty'))
            target = [int(v) for v in values['target'].split('/')]
            measured = [int(v) for v in values['measured'].split('/')]
            pwm = [int(v) for v in values['pwm'].split('/')]
            enabled, faults = (int(values[k]) for k in ('enabled', 'faults'))
            if any(len(pair) != 2 for pair in (target, measured, pwm)):
                continue
        except (KeyError, ValueError):
            continue  # A damaged packet is not reconstructed as a sample.
        runs.append((match.start(), [ms, e, h, u, duty, q, age]))
        speeds.append((match.start(), [ms, enabled, None, None, *target, *measured, *pwm, faults]))
    return ([row for _, row in sorted(runs)], [row for _, row in sorted(speeds)])

def summarize(text):
    # Separate attempts so a later STOP cannot be mistaken for an earlier run.
    summaries = []
    cursor = 0
    while (start := re.search(r'\[RUN\][^\r\n]*', text[cursor:])) is not None:
        start_at = cursor + start.start()
        after_start = cursor + start.end()
        stop = STOP.search(text, after_start)
        end = stop.start() if stop else len(text)
        segment = text[start_at:end]
        runs, speeds = read_samples(segment)
        if not runs:
            cursor = stop.end() if stop else len(text)
            continue
        t0 = int(runs[0][0])
        first = next((s for s in speeds if s[1] and (s[4] or s[5]) and (s[6] or s[7])), None)
        summaries.append({
            'first_run_ms': t0,
            'logged_run_span_ms': int(runs[-1][0]) - t0,
            'run_samples': len(runs), 'speed_samples': len(speeds),
            'first_nonzero_feedback_delay_ms': first[0] - t0 if first else None,
            'last_target_R_L': speeds[-1][4:6] if speeds else None,
            'last_measured_R_L': speeds[-1][6:8] if speeds else None,
            'peak_output_R_L_permille': [max(s[i] for s in speeds) for i in (8, 9)] if speeds else None,
            'faults_seen': sorted({s[10] for s in speeds}),
            'quality_min_max': [int(min(r[5] for r in runs)), int(max(r[5] for r in runs))],
            'near_error_min_max': [min(r[1] for r in runs), max(r[1] for r in runs)],
            'heading_min_max': [min(r[2] for r in runs), max(r[2] for r in runs)],
            'stop_ack_seen': stop is not None,
        })
        if stop is None:
            break
        cursor = stop.end()
    return summaries

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = {'source': str(args.log.resolve()), 'runs': summarize(args.log.read_text(encoding='utf-8-sig'))}
    output = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output:
        args.output.write_text(output + '\n', encoding='utf-8')
    print(output)
