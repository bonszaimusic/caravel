#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Exercise scheduler overlap, resource limits and failure propagation without Docker."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('local_host', Path(__file__).with_name('local_host.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class SchedulingTests(unittest.TestCase):
    def exercise(self, fail=None):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'tools').mkdir()
            (root / 'tools/build.py').touch()
            (root / 'firmware/engines').mkdir(parents=True)
            (root / 'firmware/engines/present').touch()
            sanitizer_started = threading.Event()
            cpu_started = threading.Event()
            lock = threading.Lock()
            active, seen = {}, []
            peak = 0

            def fake_run(argv, **kwargs):
                nonlocal peak
                if argv[:2] == ['docker', 'run']:
                    mode, item = argv[-2:]
                    workers = dict(arg.split('=', 1) for i, arg in enumerate(argv) if i and argv[i - 1] == '-e')
                    with lock:
                        active[item] = int(workers['VALIDATE_JOBS'])
                        peak = max(peak, sum(active.values()))
                        seen.append(item)
                    rc = 0
                    if item == 'cpu_dual':
                        cpu_started.set()
                        # A scheduler that serializes fuzz behind the long CPU
                        # job fails this rendezvous instead of quietly passing.
                        if not sanitizer_started.wait(2):
                            rc = 99
                    elif item == 'sanitizers':
                        sanitizer_started.set()
                    elif mode == 'host':
                        self.assertTrue(cpu_started.wait(1))
                        time.sleep(.02)
                    with lock:
                        del active[item]
                    return subprocess.CompletedProcess(argv, 73 if item == fail else rc)
                return subprocess.CompletedProcess(argv, 0)

            env = {'VALIDATE_SHARDS': '3', 'VALIDATE_TARGET_IMAGE': 'target-test', 'VALIDATE_HOST_IMAGE': 'host-test'}
            with patch.dict(os.environ, env, clear=True), patch.object(m.shutil, 'which', return_value='docker'), \
                    patch.object(m.subprocess, 'check_output', return_value=json.dumps({'Architecture': 'aarch64', 'NCPU': 12, 'MemTotal': 33 * 1024**3}).encode()), \
                    patch.object(m.subprocess, 'run', side_effect=fake_run), \
                    patch.object(sys, 'argv', ['local_host', 'host', 'fast', str(root)]), contextlib.redirect_stdout(io.StringIO()):
                rc = m.main()
            result = json.loads(next((root / 'build/docker-validate').glob('*/timing.json')).read_text())
            self.assertLessEqual(peak, 12)
            self.assertEqual(sorted(seen), sorted(['prepare', 'report', *m.SHARDS]))
            self.assertEqual(result['exit_code'], rc)
            return rc

    def test_long_cpu_job_overlaps_fuzz_without_oversubscription(self):
        self.assertEqual(self.exercise(), 0)

    def test_one_failed_shard_fails_the_whole_gate(self):
        self.assertEqual(self.exercise(fail='effects'), 1)


if __name__ == '__main__':
    unittest.main()
