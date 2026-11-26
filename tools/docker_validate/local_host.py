#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Local workflow runner: amd64 preparation, isolated native host containers."""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import uuid

HERE = Path(__file__).resolve().parents[2]
SHARDS = 'storage_usb synthesis_midi ui_storage effects cpu_dual engines_samples sanitizers web_target'.split()
BOOT = r'''
set -euo pipefail
mode=$1
item=$2
if [ "$mode" = host ] || [ "$mode" = report ]; then
    rsync -a /inputs/source/ /work/
else
    rsync -a --exclude build /src/ /work/
fi
cd /work
mkdir -p build
if [ "$mode" = host ]; then
    rsync -a /inputs/build/ build/
    # Never execute amd64 host helper binaries copied from preparation.
    rm -rf build/validation
fi
if [ "$mode" = report ]; then rsync -a /results/ build/validation/; fi
export SANITIZE=1 ASAN_OPTIONS=detect_leaks=0
export WORKFLOW_REPORT=/out/workflow.json
rc=0
case "$mode" in
    prepare) python3 tools/docker_validate/run_steps.py .github/workflows/validate-format2.yml prepare || rc=$?;;
    host) python3 tools/docker_validate/run_steps.py .github/workflows/validate-format2.yml host --matrix "shard=$item" || rc=$?;;
    report) python3 tools/validate_all.py report || rc=$?;;
    action) python3 tools/docker_validate/run_steps.py .github/workflows/action-banks-android.yml wasm || rc=$?;;
esac
if [ "$mode" = prepare ] && [ "$rc" = 0 ]; then
    mkdir -p /inputs/source /inputs/build
    rsync -a --exclude build /work/ /inputs/source/
    rsync -a build/ /inputs/build/
    git rev-parse HEAD > /out/source-revision.txt
    git diff --binary HEAD > /out/source-changes.patch
    git status --porcelain > /out/source-status.txt
fi
mkdir -p /out/artifacts
for path in validation ui_new ui_fm4 ui_slot host; do
    if [ -d "build/$path" ]; then cp -R "build/$path" /out/artifacts/; fi
done
exit "$rc"
'''


def positive(value):
    value = int(value)
    if value < 1:
        raise ValueError('worker counts must be positive')
    return value


def image(docker, filename, platform, override=None):
    if override:
        subprocess.run([docker, 'image', 'inspect', override], check=True, stdout=subprocess.DEVNULL)
        return override
    digest = hashlib.sha1((HERE / 'tools/docker_validate' / filename).read_bytes()).hexdigest()[:12]
    tag = ('caravel-validate' if filename == 'Dockerfile' else 'caravel-validate-native') + ':' + digest
    if subprocess.run([docker, 'image', 'inspect', tag], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode:
        subprocess.run([docker, 'build', '--platform', platform, '-f', str(HERE / 'tools/docker_validate' / filename),
                        '-t', tag, str(HERE / 'tools/docker_validate')], check=True)
    return tag


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('command', choices=['host', 'action', 'image'])
    ap.add_argument('gate', nargs='?', default='all')
    ap.add_argument('source', nargs='?', default=str(HERE))
    a = ap.parse_args()
    if a.command == 'host' and len(sys.argv) == 2:
        ap.error('host needs a shard (or fast/all)')
    if a.command == 'action':
        if a.gate != 'all':
            a.source = a.gate
        a.gate = 'wasm'
    src = Path(a.source).resolve()
    shards = SHARDS if a.gate in ('all', 'fast') else a.gate.split(',')
    if a.command == 'host' and (not shards or len(set(shards)) != len(shards) or any(s not in SHARDS for s in shards)):
        ap.error('unknown or repeated shard')
    docker = shutil.which('docker') or '/Applications/Docker.app/Contents/Resources/bin/docker'
    info = json.loads(subprocess.check_output([docker, 'info', '--format', '{{json .}}']))
    arch = info['Architecture']
    native_platform = 'linux/arm64' if arch in ('aarch64', 'arm64') else 'linux/amd64'
    platform = os.environ.get('VALIDATE_HOST_PLATFORM', native_platform)
    if platform not in ('linux/arm64', 'linux/amd64'):
        ap.error('VALIDATE_HOST_PLATFORM must be linux/arm64 or linux/amd64')
    slots = positive(os.environ.get('VALIDATE_SHARDS', 1))
    if a.command == 'host':
        slots = min(slots, len(shards), max(1, int(info['NCPU'])),
                    max(1, int(info['MemTotal']) // (2 * 1024**3) - 2))
    # Share both CPU and a conservative 2 GiB per compiler across active shards.
    def worker_env(active, reserved=0):
        cpu_budget = max(1, (int(info['NCPU']) - reserved) // active)
        memory_budget = max(1, (int(info['MemTotal']) // (2 * 1024**3) - 2 - reserved) // active)
        workers = min(8, cpu_budget, memory_budget)
        return {'VALIDATE_JOBS': str(min(positive(os.environ.get('VALIDATE_JOBS', workers)), workers)),
                'CHORDEXT_FUZZ_WORKERS': str(min(positive(os.environ.get('CHORDEXT_FUZZ_WORKERS', min(12, cpu_budget))), cpu_budget)),
                'CHORDEXT_MUT_WORKERS': str(min(positive(os.environ.get('CHORDEXT_MUT_WORKERS', workers)), workers))}
    env = worker_env(slots)
    if a.command == 'host' and a.gate == 'all':
        if os.environ.get('CHORDEXT_FUZZ_SMOKE', '0') != '0' or ('CHORDEXT_FUZZ_SEEDS' in os.environ and positive(os.environ['CHORDEXT_FUZZ_SEEDS']) < 141):
            ap.error('host all requires at least 141 seeds and cannot run in smoke mode')
    if a.command == 'host' and a.gate == 'fast':
        env['CHORDEXT_FUZZ_SMOKE'] = '1'
    if 'CHORDEXT_FUZZ_SEEDS' in os.environ:
        env['CHORDEXT_FUZZ_SEEDS'] = str(positive(os.environ['CHORDEXT_FUZZ_SEEDS']))
    # Native TSAN is mandatory. Only the explicitly selected emulated baseline
    # retains the old documented limitation; never silently fall back to it.
    emulated = platform != native_platform
    if emulated and not os.environ.get('FORCE_TSAN'):
        env.update(NO_TSAN='1', DROP_ENV='TSAN')
        print('Explicit emulated baseline: ThreadSanitizer is unavailable.', flush=True)
    target = image(docker, 'Dockerfile', 'linux/amd64', os.environ.get('VALIDATE_TARGET_IMAGE')) if a.command != 'action' else None
    host = image(docker, 'Dockerfile.native' if not emulated else 'Dockerfile', platform, os.environ.get('VALIDATE_HOST_IMAGE'))
    if a.command == 'image':
        return 0
    if not (src / 'tools/build.py').is_file() or not any((src / 'firmware/engines').iterdir()):
        ap.error('source checkout and its populated engines submodule are required')
    run_id = time.strftime('%Y%m%dT%H%M%S') + '-' + uuid.uuid4().hex[:8]
    out = src / 'build/docker-validate' / (a.command + '-' + a.gate + '-' + run_id)
    out.mkdir(parents=True)
    control = out / 'control'
    control.mkdir()
    fuzz_budget = control / 'fuzz-workers'

    def set_fuzz_budget(workers):
        temporary = control / 'fuzz-workers.tmp'
        temporary.write_text(str(workers) + '\n')
        temporary.replace(fuzz_budget)

    set_fuzz_budget(worker_env(1)['CHORDEXT_FUZZ_WORKERS'])
    print('Artifacts: ' + str(out), flush=True)
    volume = 'caravel-validation-' + uuid.uuid4().hex
    subprocess.run([docker, 'volume', 'create', volume], check=True, stdout=subprocess.DEVNULL)
    start = time.monotonic()
    rows = []
    completed = False

    def run(mode, item, tag, run_platform, allocation=None):
        folder = out / item
        folder.mkdir()
        argv = [docker, 'run', '--rm', '--platform', run_platform,
                '-v', str(src) + ':/src:ro', '-v', str(folder) + ':/out',
                '-v', volume + ':/inputs' + ('' if mode == 'prepare' else ':ro')]
        if mode == 'host' and item == 'cpu_dual' and not emulated:
            argv += ['--security-opt', 'seccomp=' + str(HERE / 'tools/docker_validate/seccomp-tsan.json')]
        container_env = {**env, **(allocation or {})}
        if item == 'sanitizers':
            argv += ['-v', str(control) + ':/control:ro']
            container_env['CHORDEXT_FUZZ_WORKERS'] = worker_env(1)['CHORDEXT_FUZZ_WORKERS']
            container_env['CHORDEXT_FUZZ_BUDGET_FILE'] = '/control/fuzz-workers'
        for key, value in container_env.items():
            argv += ['-e', key + '=' + value]
        # report uses the aggregate from this run, never stale previous results.
        if mode == 'report':
            argv += ['-v', str(out / 'artifacts/validation') + ':/results:ro']
        t0 = time.monotonic()
        with (folder / 'run.log').open('w') as log:
            p = subprocess.run([*argv, tag, 'bash', '-c', BOOT, 'local-validation', mode, item], stdout=log, stderr=subprocess.STDOUT)
        row = {'step': item, 'exit_code': p.returncode, 'seconds': round(time.monotonic() - t0, 2), 'platform': run_platform, 'workers': container_env}
        print(json.dumps(row), flush=True)
        if p.returncode:
            print((folder / 'run.log').read_text(errors='replace')[-8000:], flush=True)
        return row

    try:
        if a.command == 'action':
            rows.append(run('action', 'action', host, platform))
        else:
            rows.append(run('prepare', 'prepare', target, 'linux/amd64'))
            if not rows[-1]['exit_code']:
                if 'cpu_dual' in shards and slots > 1:
                    # TSAN's fork-heavy fixtures have long wall time but little
                    # CPU use. Reserve at most two compiler/CPU slots for them;
                    # other shards, then fuzzing, can use the remaining budget.
                    reserved = min(2, max(1, int(info['NCPU']) // 3))
                    cpu_env = worker_env(1, max(0, int(info['NCPU']) - reserved))
                    set_fuzz_budget(worker_env(1, reserved)['CHORDEXT_FUZZ_WORKERS'])
                    with concurrent.futures.ThreadPoolExecutor(max_workers=1) as cpu_pool:
                        cpu = cpu_pool.submit(run, 'host', 'cpu_dual', host, platform, cpu_env)
                        cpu.add_done_callback(lambda _: set_fuzz_budget(worker_env(1)['CHORDEXT_FUZZ_WORKERS']))
                        ordinary = [s for s in shards if s not in ('cpu_dual', 'sanitizers')]
                        # The editor test spends much of its time on timers;
                        # start it early so other compiles can overlap the wait.
                        ordinary.sort(key=lambda s: s != 'web_target')
                        with concurrent.futures.ThreadPoolExecutor(max_workers=slots - 1) as pool:
                            rows.extend(pool.map(lambda s: run('host', s, host, platform, worker_env(slots - 1, reserved)), ordinary))
                        if 'sanitizers' in shards:
                            rows.append(run('host', 'sanitizers', host, platform, worker_env(1, 0 if cpu.done() else reserved)))
                        rows.append(cpu.result())
                else:
                    with concurrent.futures.ThreadPoolExecutor(max_workers=slots) as pool:
                        rows.extend(pool.map(lambda s: run('host', s, host, platform), [s for s in shards if s != 'sanitizers']))
                    if 'sanitizers' in shards:
                        rows.append(run('host', 'sanitizers', host, platform, worker_env(1)))
                aggregate = out / 'artifacts/validation'
                aggregate.mkdir(parents=True)
                for s in ['prepare', *shards]:
                    path = out / s / 'artifacts/validation'
                    if path.is_dir():
                        shutil.copytree(path, aggregate, dirs_exist_ok=True)
                if set(shards) == set(SHARDS):
                    rows.append(run('report', 'report', host, platform))
        completed = True
    finally:
        summary = {'command': a.command, 'gate': a.gate, 'platform': platform, 'workers': env,
                   'shard_workers': slots, 'wall_seconds': round(time.monotonic() - start, 2), 'steps': rows,
                   'exit_code': int(not completed or any(r['exit_code'] for r in rows) or not rows)}
        (out / 'timing.json').write_text(json.dumps(summary, indent=2) + '\n')
        subprocess.run([docker, 'volume', 'rm', volume], stdout=subprocess.DEVNULL, check=True)
    print('Result: ' + str(out / 'timing.json'), flush=True)
    if a.command == 'host' and a.gate in ('all', 'fast'):
        timing_rc = subprocess.run([sys.executable, str(HERE / 'tools/validation_timing.py'), str(out)]).returncode
        if timing_rc:
            summary['exit_code'] = 1
            (out / 'timing.json').write_text(json.dumps(summary, indent=2) + '\n')
    return summary['exit_code']


if __name__ == '__main__':
    sys.exit(main())
