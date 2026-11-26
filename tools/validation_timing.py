#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Print measured local gate timings; fail incomplete gates and exceeded budgets.

  python3 tools/validation_timing.py build/docker-validate/host-fast-RUN
  python3 tools/validation_timing.py RUN --before BASELINE_RUN
"""
import argparse
import json
from pathlib import Path
import sys

SHARDS = 'storage_usb synthesis_midi ui_storage effects cpu_dual engines_samples sanitizers web_target'.split()


def load(root):
    run = json.loads((root / 'timing.json').read_text())
    checks = {}
    for shard in SHARDS:
        path = root / 'artifacts/validation' / (shard + '.json')
        if path.exists():
            checks[shard] = json.loads(path.read_text())
    return run, checks


def validate(run, checks):
    errors = []
    if run['exit_code'] or any(r['exit_code'] for r in run['steps']):
        errors.append('gate failed')
    if run['command'] == 'host' and run['gate'] in ('all', 'fast'):
        if {r['step'] for r in run['steps']} != {'prepare', 'report', *SHARDS}:
            errors.append('missing gate stages')
        if set(checks) != set(SHARDS):
            errors.append('missing shard results')
    for name, result in checks.items():
        if result['exit_code'] or not result['checks'] or any(r['exit_code'] for r in result['checks']):
            errors.append(name + ': failed or empty checks')
    return errors


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('run', type=Path)
    ap.add_argument('--before', type=Path)
    ap.add_argument('--budget', type=float, help='gate wall seconds (default: fast 600, all 1200)')
    a = ap.parse_args()
    run, checks = load(a.run)
    errors = validate(run, checks)
    baseline, old = load(a.before) if a.before else (None, {})
    print('| Step | Before (s) | Measured (s) | Exit |')
    print('|---|---:|---:|---:|')
    previous = {r['step']: r for r in baseline['steps']} if baseline else {}
    for row in run['steps']:
        print(f"| {row['step']} | {previous.get(row['step'], {}).get('seconds', '—')} | {row['seconds']:.2f} | {row['exit_code']} |")
    print(f"| **Gate wall time** | {baseline['wall_seconds'] if baseline else '—'} | **{run['wall_seconds']:.2f}** | {run['exit_code']} |")
    budget = a.budget if a.budget is not None else {'fast': 600, 'all': 1200}.get(run['gate'])
    if budget is not None and run['wall_seconds'] > budget:
        errors.append(f"time budget exceeded: {run['wall_seconds']:.2f}s > {budget:.2f}s")
    if baseline:
        errors += ['baseline: ' + e for e in validate(baseline, old)]
        if set(old) != set(checks):
            errors.append('shard set changed')
        for name in set(old) & set(checks):
            before, after = old[name], checks[name]
            if [(r['case'], r['exit_code']) for r in before['checks']] != [(r['case'], r['exit_code']) for r in after['checks']]:
                errors.append(name + ': ordered check names/counts/results changed')
            if set(after['skips']) - set(before['skips']):
                errors.append(name + ': new omissions')
        left = a.before / 'artifacts/validation/host-inventory.json'
        right = a.run / 'artifacts/validation/host-inventory.json'
        if not left.exists() or not right.exists() or json.loads(left.read_text()) != json.loads(right.read_text()):
            errors.append('inventory changed or missing')
    # The fuzzer and mutants are nested steps: check their actual row coverage,
    # rather than trusting only the enclosing process exit code.
    if 'sanitizers' in checks and checks['sanitizers']['exit_code'] == 0:
        root = a.run / 'artifacts/validation'
        try:
            fuzz = json.loads((root / 'chordext-fuzz/results.json').read_text())
            seeds = fuzz['seeds_per_profile']
            expected = {(c, p, s) for c in (0, 1) for p in (2, 3, 4) for s in range(1, seeds + 1)}
            actual = [(r['cpu2_compiled'], r['profile'], r['seed']) for r in fuzz['rows']]
            if seeds < (141 if run['gate'] == 'all' else 30) or len(actual) != len(expected) or set(actual) != expected or any(r['exit_code'] for r in fuzz['rows']):
                errors.append('fuzzer coverage/results mismatch')
            mutants = json.loads((root / 'chordext-mutants/results.json').read_text())
            rows = mutants['mutants']
            if len(rows) != 11 or {r['mutation'] for r in rows} != {'M' + str(i) for i in range(1, 12)} or not all(r['compile_exit'] == 0 and r['detected'] for r in rows):
                errors.append('mutation coverage/results mismatch')
            controls = mutants['controls']
            if len(controls) != 3 or not all(r['compile_exit'] == 0 and r['control_pass'] for r in controls):
                errors.append('mutation controls failed')
        except (OSError, KeyError, ValueError) as exc:
            errors.append('missing or invalid nested results: ' + str(exc))
    for name, result in checks.items():
        for row in sorted(result['checks'], key=lambda r: -r['seconds'])[:3]:
            print(f"Slow step: {name}/{row['case']}: {row['seconds']:.2f}s")
    for error in errors:
        print('FAIL: ' + error, file=sys.stderr)
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
