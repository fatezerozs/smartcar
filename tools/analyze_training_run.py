"""Summarize a stopped training run without accessing hardware.
PWM fields in RUN are controller demand; SPEED contains actual output permille.
Encoder samples are sparse telemetry, so no distance or m/s is inferred.
"""
import argparse
import json
import re
from pathlib import Path

RUN = re.compile(r'\[RUN\] n=\d+ ms=(\d+) e=([-\d.]+) h=([-\d.]+) u=([-\d.]+) pwm=([-\d.]+) q=(\d+) age=(\d+)')
SPEED = re.compile(r'\[SPEED\] ms=(\d+) sample=\d+ enabled=(\d+) raw3_L=(-?\d+) raw4_R=(-?\d+) target_R/L=(-?\d+)/(-?\d+) measured_R/L=(-?\d+)/(-?\d+) pwm_R/L=(\d+)/(\d+) faults=(\d+)')
STOP = re.compile(r'\[(?:LINK|FIELD)\] STOP')

def summarize(text):
    # Separate attempts so a later STOP cannot be mistaken for an earlier run.
    summaries = []
    cursor = 0
    while (start := RUN.search(text, cursor)) is not None:
        stop = STOP.search(text, start.end())
        end = stop.start() if stop else len(text)
        segment = text[start.start():end]
        runs = [[float(v) for v in m.groups()] for m in RUN.finditer(segment)]
        speeds = [[int(v) for v in m.groups()] for m in SPEED.finditer(segment)]
        t0 = int(runs[0][0])
        first = next((s for s in speeds if s[1] and (s[4] or s[5]) and (s[2] or s[3])), None)
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
