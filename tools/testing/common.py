"""Manifest contract, schema validation, safe paths and graph selection."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
STATES = ('PASS', 'FAIL', 'ERROR', 'TIMEOUT', 'BLOCKED', 'SKIPPED', 'MANUAL_PENDING')


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('Duplicate JSON key: ' + key)
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding='utf-8-sig'), object_pairs_hook=unique,
                      parse_constant=lambda s: (_ for _ in ()).throw(ValueError('Non-finite JSON: ' + s)))


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    os.replace(temporary, path)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inside(root, value, *, must_exist=True):
    root = Path(root).resolve()
    value_path = Path(value)
    if value_path.is_absolute() or ':' in str(value) or '..' in value_path.parts:
        raise ValueError('Expected a repository-relative path: ' + str(value))
    target = (root / value).resolve()
    if not target.is_relative_to(root):
        raise ValueError('Path escapes allowed root: ' + str(value))
    if must_exist and not target.exists():
        raise ValueError('Missing input: ' + str(value))
    return target


def validate_schema(value, name, root=ROOT):
    pwsh = shutil.which('pwsh')
    if not pwsh:
        raise RuntimeError('PowerShell 7 (pwsh/Test-Json) is required for JSON Schema validation')
    scratch = root / '.test-results/.validation'
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as temp:
        data = Path(temp) / 'input.json'
        write_json(data, value)
        proc = subprocess.run([pwsh, '-NoLogo', '-NoProfile', '-NonInteractive', '-File',
                               str(ROOT / 'tools/testing/Validate-Json.ps1'), '-Path', str(data),
                               '-Schema', str(root / 'tests/schemas' / name)],
                              capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=30,
                              creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        if proc.returncode:
            raise ValueError('JSON Schema validation failed: ' + proc.stderr.strip() + proc.stdout.strip())


def load_catalog(path=None, root=ROOT):
    catalog = read_json(path or root / 'tests/suites/catalog.json')
    validate_schema(catalog, 'suite.schema.json', root)
    suites = {}
    for suite in catalog['suites']:
        sid = suite['id']
        if sid in suites:
            raise ValueError('Duplicate suite ID: ' + sid)
        suites[sid] = suite
        entry = suite['entrypoint']
        inside(root, suite['workingDirectory'], must_exist=False)
        if 'path' in entry:
            path = inside(root, entry['path'])
            if not path.is_relative_to(root / 'tests') and not path.is_relative_to(root / 'tools'):
                raise ValueError('Entrypoints must be under tests/ or tools/: ' + sid)
            suffix = {'python':'.py', 'powershell':'.ps1', 'cmd':'.cmd', 'native':'.exe', 'benchmark':'.json'}.get(entry['type'])
            if suffix and path.suffix.lower() != suffix:
                raise ValueError('Entrypoint type/extension mismatch: ' + sid)
        seen_variants = set()
        for variant in entry.get('variants', []):
            if variant['id'] in seen_variants:
                raise ValueError('Duplicate variant: ' + sid + '/' + variant['id'])
            seen_variants.add(variant['id'])
            for path in variant['sources'] + variant['includeDirectories']:
                inside(root, path)
            if any(not p.endswith('.c') for p in variant['sources']):
                raise ValueError('C recipes require C source paths: ' + sid)
            if 'reference' in variant:
                inside(root, variant['reference']['source'])
        for path in entry.get('fixtures', []):
            inside(root, path)
        for pattern in suite['artifacts']:
            inside(root, pattern, must_exist=False)
        if suite['layer'] == 'hardware' or suite['evidenceClass'] == 'hardware':
            if suite['profiles'] or entry['type'] != 'manual':
                raise ValueError('Hardware suites may only record manual evidence outside automatic profiles')
        if entry['type'] == 'manual' and suite['profiles']:
            raise ValueError('Manual suites must not be in automatic profiles')
    # Validate all graph edges, including suites not selected by this profile.
    visiting, visited = set(), set()

    def visit(sid):
        if sid not in suites:
            raise ValueError('Unknown dependency: ' + sid)
        if sid in visiting:
            raise ValueError('Dependency cycle: ' + sid)
        if sid in visited:
            return
        visiting.add(sid)
        for dep in suites[sid]['dependsOn']:
            visit(dep)
        visiting.remove(sid)
        visited.add(sid)
    for sid in suites:
        visit(sid)
    return suites


def select_suites(catalog, profile, requested=()):
    selected, order = set(), []

    def visit(sid):
        sid = sid.lower()
        if sid not in catalog:
            raise ValueError('Unknown suite: ' + sid)
        if sid in selected:
            return
        selected.add(sid)
        for dep in catalog[sid]['dependsOn']:
            visit(dep)
        order.append(sid)
    targets = requested or [sid for sid, s in catalog.items() if profile in s['profiles']]
    for sid in targets:
        visit(sid)
    if not order:
        raise ValueError('Empty test selection')
    return order


def aggregate_status(results):
    required = [r for r in results if r['required']]
    for state in ('ERROR', 'TIMEOUT', 'FAIL', 'BLOCKED', 'MANUAL_PENDING', 'SKIPPED'):
        if any(r['status'] == state for r in required):
            return state
    return 'PASS' if required and all(r['status'] == 'PASS' for r in required) else 'SKIPPED'
