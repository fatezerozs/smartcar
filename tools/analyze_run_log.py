"""Offline analysis only. Reads passive UART logs; never opens a serial port."""
import argparse
import csv
import json
import re
from pathlib import Path


def fields(text):
    out = {}
    for key, value in re.findall(r'([A-Za-z0-9_/]+)=([^\s;]+)', text):
        try:
            out[key] = float(value) if '.' in value else int(value)
        except ValueError:
            out[key] = value
    return out


def analyze_text(text):
    traces, stops, current = [], {}, None
    pattern = r'\[(TRACE_BEGIN|TRACE_END|TRACE|LAST_STOP)\]([^\r\n]*)'
    for match in re.finditer(pattern, text):
        kind, data = match.group(1), fields(match.group(2))
        if kind == 'LAST_STOP':
            if data.get('seq', 0):
                stops[(data.get('seq'), data.get('ms'))] = data
        elif kind == 'TRACE_BEGIN':
            current = {'run': data.get('run'), 'expected': data.get('count', 0),
                       'scale': data.get('scale', 1000), 'records': [], 'end_seen': False}
            traces.append(current)
        elif current and data.get('run') == current['run']:
            if kind == 'TRACE':
                current['records'].append(data)
            elif kind == 'TRACE_END':
                current['end_seen'] = data.get('count') == current['expected']
    for trace in traces:
        records = trace['records']
        indices = [r.get('i') for r in records]
        trace['complete'] = bool(trace['end_seen'] and indices == list(range(trace['expected'])))
        trace['received'] = len(records)
        if records:
            trace['duration_ms'] = (records[-1].get('ms', 0) - records[0].get('ms', 0)) & 0xffffffff
            trace['max_abs_lateral'] = max(abs(r.get('e', 0)) for r in records) / trace['scale']
            trace['full_steer_records'] = sum(abs(r.get('u', 0)) >= 990 for r in records)
            trace['invalid_records'] = sum(not r.get('v', 0) for r in records)
    return {'traces': traces, 'retained_stops': list(stops.values())}


def write_analysis(log, output):
    result = analyze_text(log.read_text(encoding='utf-8-sig', errors='replace'))
    output.mkdir(parents=True, exist_ok=True)
    for index, trace in enumerate(result['traces'], 1):
        rows = trace['records']
        if not rows:
            continue
        columns = ['i', 'ms', 'elapsed_ms', 'frame', 'e', 'h', 'u', 'duty', 'servo',
                   'raw', 'target', 'pwm', 'v', 'q', 'f', 'w', 'cx', 'both', 'inferred', 'state', 'source', 'obs', 'faults', 'junction', 'anchors', 'ring', 'ring_side', 'ring_rows', 'entry_guide']
        name = output / f'trace-{index:03d}-run-{trace["run"]}.csv'
        with name.open('w', encoding='utf-8-sig', newline='') as handle:
            writer = csv.DictWriter(handle, fieldnames=columns, extrasaction='ignore')
            writer.writeheader()
            for raw in rows:
                row = dict(raw)
                row['elapsed_ms'] = (row.get('ms', 0) - rows[0].get('ms', 0)) & 0xffffffff
                for key in ('e', 'h', 'u', 'duty'):
                    if key in row:
                        row[key] /= trace['scale']
                writer.writerow(row)
        trace['csv'] = str(name.resolve())
        del trace['records']
    (output / 'summary.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    folder = args.output or args.log.parent / (args.log.stem + '-analysis')
    print(json.dumps(write_analysis(args.log, folder), ensure_ascii=False, indent=2))
