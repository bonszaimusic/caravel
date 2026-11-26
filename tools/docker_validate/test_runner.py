#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Fault injection for the compile plan and ordered execution (no firmware edits)."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('validation', Path(__file__).parents[1] / 'validate_format2.py')
v = importlib.util.module_from_spec(spec)
spec.loader.exec_module(v)


class CompilePlanTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / 'tests').mkdir()
        self.output = self.root / 'build/validation'
        self.patches = [patch.object(v, 'ROOT', self.root), patch.object(v, 'OUT', self.output)]
        for p in self.patches:
            p.start()
        self.quiet = contextlib.redirect_stdout(io.StringIO())
        self.quiet.__enter__()

    def tearDown(self):
        self.quiet.__exit__(None, None, None)
        for p in reversed(self.patches):
            p.stop()
        self.tmp.cleanup()

    def source(self, name, code):
        (self.root / 'tests' / (name + '_test.c')).write_text(code)

    def execute(self, shard, cases, jobs):
        r = v.Runner(shard)
        if jobs > 1:
            r.plan = True
            for name in cases:
                r.c(name)
            r.plan = False
            r.compile_all(jobs)
            r.idx = 0
        rc = 1
        try:
            for name in cases:
                r.c(name)
            rc = 0
        finally:
            r.results(rc)
        return r

    def test_repeated_names_have_separate_binaries_and_logs(self):
        self.source('repeat', 'int main(void){return 0;}')
        r = self.execute('storage_usb', ['repeat'] * 6, 4)
        self.assertEqual(len(r.rows), 12)
        self.assertEqual(len({row['log'] for row in r.rows}), 12)
        self.assertEqual([row['case'] for row in r.rows], ['repeat-compile', 'repeat'] * 6)

    def test_sequential_and_parallel_keep_execution_order(self):
        self.source('first', '#include <stdio.h>\nint main(void){FILE*f=fopen("ordered","w"); fputs("yes",f);fclose(f);return 0;}')
        self.source('second', '#include <stdio.h>\nint main(void){FILE*f=fopen("ordered","r");if(!f)return 3;fclose(f);return 0;}')
        rows = []
        for jobs in (1, 3):
            marker = self.root / 'ordered'
            marker.unlink(missing_ok=True)
            r = self.execute('ui_storage', ['first', 'second'], jobs)
            rows.append([(row['case'], row['exit_code']) for row in r.rows])
        self.assertEqual(*rows)

    def test_broken_test_fails_each_shard(self):
        self.source('broken', 'int main(void){return 73;}')
        for shard in v.SHARDS:
            for jobs in (1, 3):
                with self.subTest(shard=shard, jobs=jobs):
                    with self.assertRaisesRegex(RuntimeError, 'broken: exit 73'):
                        self.execute(shard, ['broken'], jobs)
                    result = json.loads((self.output / (shard + '.json')).read_text())
                    self.assertEqual(result['exit_code'], 1)
                    self.assertEqual(result['checks'][-1]['exit_code'], 73)

    def test_compile_error_is_recorded_and_fails(self):
        self.source('broken_compile', 'this cannot compile')
        for jobs in (1, 3):
            with self.assertRaisesRegex(RuntimeError, 'broken_compile-compile: exit'):
                self.execute('sanitizers', ['broken_compile'], jobs)
            result = json.loads((self.output / 'sanitizers.json').read_text())
            self.assertNotEqual(result['checks'][0]['exit_code'], 0)

    def test_timeout_and_missing_executable_are_recorded(self):
        r = v.Runner('web_target')
        with self.assertRaisesRegex(RuntimeError, 'exit 124'):
            r.command('timeout', ['python3', '-c', 'import time;time.sleep(10)'], timeout=.02)
        with self.assertRaisesRegex(RuntimeError, 'exit 127'):
            r.command('missing', [str(self.root / 'does-not-exist')])
        self.assertEqual([row['exit_code'] for row in r.rows], [124, 127])

    def test_invalid_worker_count_is_rejected(self):
        for value in ('0', '-1', 'no'):
            with patch.dict(os.environ, VALIDATE_JOBS=value):
                with self.assertRaises(ValueError):
                    v.positive_env('VALIDATE_JOBS', 8)


if __name__ == '__main__':
    unittest.main()
