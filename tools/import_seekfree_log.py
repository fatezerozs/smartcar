"""Offline import of SeekFree TEXT serial logs. Never opens or writes a COM port.

Control-state segments are observations, not proof of physical start/standstill.
Preserves the source bytes and separately exports delayed firmware TRACE dumps.
"""
import argparse
import csv
import hashlib
import json
import re
from datetime import datetime
from pathlib import Path

from analyze_run_log import fields, write_analysis

TOKEN = re.compile(r'\[(RUN|SPEED|TURN|DRIVE|LAST_STOP|BOOT|LINK|FIELD)\]([^\r\n]*)')


def split_segments(text, gap_ms=1500):
    segments, active = [], None
    seen_stops = set()
    epoch, boot_group = 0, False
    last_live_ms = None

    def close(reason, line_number, stop=None):
        nonlocal active
        if active is None:
            return
        active['end_evidence'] = reason
        active['end_line'] = line_number
        active['stop'] = stop
        active['stop_observed'] = stop is not None
        active['physical_start_confirmed'] = False
        active['physical_stop_confirmed'] = False
        active['whole_run_guaranteed'] = False
        segments.append(active)
        active = None

    for line_number, line in enumerate(text.splitlines(), 1):
        # Keep the untouched line (including host timestamps) in the segment log.
        match = TOKEN.search(line)
        if not match:
            if active is not None and not re.search(r'\[TRACE(?:_BEGIN|_END)?\]', line):
                active['lines'].append(line)
            continue
        kind, body = match.groups()
        data = fields(body)
        prefix = line[:match.start()].strip()
        if kind == 'BOOT':
            if not boot_group:
                close('firmware_reboot', line_number - 1)
                epoch += 1
                seen_stops.clear()
            boot_group = True
            last_live_ms = None
            continue
        # Wireless BOOT text may be absent. SPEED continues while stopped and
        # exposes a reboot even between two already-closed running segments.
        if kind in ('RUN', 'SPEED') and isinstance(data.get('ms'), int):
            live_ms = data['ms']
            if last_live_ms is not None and ((live_ms - last_live_ms) & 0xffffffff) > 0x7fffffff:
                close('clock_reset_or_out_of_order', line_number - 1)
                epoch += 1
                seen_stops.clear()
            last_live_ms = live_ms
        is_running = kind == 'RUN' or (kind == 'DRIVE' and data.get('state') == 1)
        if is_running:
            boot_group = False
            ms = data.get('ms')
            if active is not None and isinstance(ms, int) and active['last_run_ms'] is not None:
                delta = (ms - active['last_run_ms']) & 0xffffffff
                if delta > 0x7fffffff:
                    close('clock_reset_or_out_of_order', line_number - 1)
                    epoch += 1
                    seen_stops.clear()
                elif delta > gap_ms:
                    close('telemetry_gap_not_proven_stop', line_number - 1)
            if active is None:
                active = {
                    'segment': len(segments) + 1, 'boot_epoch': epoch,
                    'start_line': line_number, 'first_run_ms': ms,
                    'last_run_ms': ms, 'start_evidence': 'first_running_observation',
                    'lines': [], 'run_rows': [], 'speed_rows': [], 'turn_rows': [],
                }
            if isinstance(ms, int):
                active['last_run_ms'] = ms
        if kind == 'LAST_STOP' and data.get('seq', 0):
            key = (data.get('seq'), data.get('ms'))
            if key in seen_stops:
                continue
            seen_stops.add(key)
            # Periodic LAST_STOP can describe an earlier attempt, including when
            # connecting midway through a later run. Do not close that later run.
            if active is not None:
                stop_ms, start_ms = data.get('ms'), active['first_run_ms']
                if isinstance(stop_ms, int) and isinstance(start_ms, int):
                    if ((stop_ms - start_ms) & 0xffffffff) > 0x7fffffff:
                        continue
                active['lines'].append(line)
                close('last_stop_control_event', line_number, data)
            continue
        if active is not None:
            active['lines'].append(line)
            if kind in ('RUN', 'SPEED', 'TURN'):
                row = dict(data, source_line=line_number, host_prefix=prefix)
                active[{'RUN': 'run_rows', 'SPEED': 'speed_rows', 'TURN': 'turn_rows'}[kind]].append(row)
            if kind == 'DRIVE' and data.get('state') in (0, 2):
                close('drive_state_stopped_or_fault', line_number, data)
            elif kind in ('LINK', 'FIELD') and re.match(r'\s*STOP\b', body):
                close('stop_ack_control_event', line_number, data)
    close('capture_ended_without_stop', len(text.splitlines()))
    for item in segments:
        start, end = item['first_run_ms'], item['last_run_ms']
        item['observed_run_span_ms'] = ((end - start) & 0xffffffff) if isinstance(start, int) and isinstance(end, int) else None
        rows = item['run_rows']
        for key, label in [('e', 'lateral'), ('h', 'heading'), ('u', 'steering')]:
            values = [r[key] for r in rows if isinstance(r.get(key), (int, float))]
            if values:
                item[label + '_min_max'] = [min(values), max(values)]
        item['run_sample_count'] = len(rows)
        item['speed_sample_count'] = len(item['speed_rows'])
    return segments


def write_csv(path, rows):
    if not rows:
        return
    columns = list(dict.fromkeys(key for row in rows for key in row))
    with path.open('w', encoding='utf-8-sig', newline='') as handle:
        writer = csv.DictWriter(handle, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)


def import_log(source, output_root, encoding='auto', gap_ms=1500):
    source = Path(source).resolve()
    raw = source.read_bytes()
    if encoding == 'auto':
        candidates = ['utf-16'] if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else ['utf-8-sig', 'gb18030']
        for candidate in candidates:
            try:
                text = raw.decode(candidate)
                encoding = candidate
                break
            except UnicodeDecodeError:
                pass
        else:
            raise ValueError('Cannot decode text. Use --encoding, and export TEXT rather than HEX/image data.')
    else:
        text = raw.decode(encoding)
    if '\x00' in text or not re.search(r'\[(?:RUN|SPEED|DRIVE|LAST_STOP|TRACE_BEGIN|BOOT|FIELD)\]', text):
        raise ValueError('No supported firmware TEXT log found. Check HEX display and camera mode.')
    root = Path(output_root).resolve()
    root.mkdir(parents=True, exist_ok=True)
    # Timestamp plus microseconds; exclusive creation also prevents overwrites.
    output = root / ('seekfree-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    output.mkdir()
    (output / 'source-original.bin').write_bytes(raw)
    normalized = output / 'session.log'
    # Preserve line endings: Windows text-mode writing would turn existing
    # CRLF into CRCRLF and make source_line references disagree with session.log.
    normalized.write_bytes(text.encode('utf-8'))
    segments = split_segments(text, gap_ms)
    for item in segments:
        folder = output / f'segment-{item["segment"]:03d}'
        folder.mkdir()
        (folder / 'serial.log').write_text('\n'.join(item.pop('lines')) + '\n', encoding='utf-8')
        for name in ('run', 'speed', 'turn'):
            write_csv(folder / (name + '.csv'), item.pop(name + '_rows'))
        (folder / 'summary.json').write_text(json.dumps(item, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    trace_result = write_analysis(normalized, output / 'traces')
    result = {
        'source': str(source), 'source_bytes': len(raw),
        'sha256': hashlib.sha256(raw).hexdigest(), 'encoding': encoding,
        'imported_at': datetime.now().astimezone().isoformat(),
        'gap_ms': gap_ms, 'segments': segments,
        'trace_exports': len(trace_result['traces']),
        'complete_trace_exports': sum(t['complete'] for t in trace_result['traces']),
        'notes': [
            'Segments use software state, not physical motion or track position.',
            'A telemetry gap is an uncertain boundary, not proof of a stop.',
            'Capture can start midway; whole-run completeness is not guaranteed.',
            'TRACE dumps arrive after stopping and are exported separately; no guessed segment association.',
            'Encoder speed remains counts/10ms; RUN pwm is demand, SPEED pwm_R/L is output permille.',
        ],
    }
    (output / 'summary.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return output, result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output-root', type=Path, default=Path(__file__).resolve().parents[1] / 'recordings')
    parser.add_argument('--encoding', default='auto', help='auto: UTF-8/BOM UTF-16/GB18030')
    parser.add_argument('--gap-ms', type=int, default=1500, help='uncertain split threshold between RUN timestamps')
    args = parser.parse_args()
    if args.gap_ms <= 0:
        parser.error('--gap-ms must be positive')
    try:
        output, result = import_log(args.log, args.output_root, args.encoding, args.gap_ms)
    except (OSError, UnicodeError, ValueError) as exc:
        parser.exit(1, str(exc) + '\n')
    print(f'Saved: {output}\nSegments: {len(result["segments"])}; TRACE exports: {result["trace_exports"]}')


if __name__ == '__main__':
    main()
