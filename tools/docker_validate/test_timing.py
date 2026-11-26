#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Prove timing/coverage checks fail on incomplete or falsely green results."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('timing', Path(__file__).parents[1] / 'validation_timing.py')
t = importlib.util.module_from_spec(spec)
spec.loader.exec_module(t)


class TimingTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name) / 'run'
        self.results = self.root / 'artifacts/validation'
        self.results.mkdir(parents=True)
        self.run = {'command': 'host', 'gate': 'fast', 'wall_seconds': 100, 'exit_code': 0,
                    'steps': [{'step': s, 'seconds': 1, 'exit_code': 0} for s in ['prepare', *t.SHARDS, 'report']]}
        self.write(self.root / 'timing.json', self.run)
        for shard in t.SHARDS:
            self.write(self.results / (shard + '.json'), {'exit_code': 0, 'checks': [{'case': 'one', 'exit_code': 0, 'seconds': 1}], 'skips': []})
        self.write(self.results / 'host-inventory.json', {'same': True})
        self.fuzz = {'seeds_per_profile': 30, 'rows': [{'cpu2_compiled': c, 'profile': p, 'seed': s, 'exit_code': 0} for c in (0, 1) for p in (2, 3, 4) for s in range(1, 31)]}
        self.write(self.results / 'chordext-fuzz/results.json', self.fuzz)
        self.mutations = {'controls': [{'compile_exit': 0, 'control_pass': True} for _ in range(3)],
                          'mutants': [{'mutation': 'M' + str(i), 'compile_exit': 0, 'detected': True} for i in range(1, 12)]}
        self.write(self.results / 'chordext-mutants/results.json', self.mutations)

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data))

    def check(self, *args):
        with patch.object(sys, 'argv', ['timing', str(self.root), *args]), contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return t.main()

    def test_complete_gate_and_equal_baseline_pass(self):
        baseline = self.root.parent / 'before'
        shutil.copytree(self.root, baseline)
        self.assertEqual(self.check('--before', str(baseline)), 0)

    def test_time_budget_is_fatal(self):
        self.run['wall_seconds'] = 601
        self.write(self.root / 'timing.json', self.run)
        self.assertEqual(self.check(), 1)

    def test_missing_result_is_fatal_even_with_zero_gate_exit(self):
        (self.results / 'effects.json').unlink()
        self.assertEqual(self.check(), 1)

    def test_duplicate_fuzz_rows_are_not_coverage(self):
        self.fuzz['rows'][-1] = self.fuzz['rows'][0]
        self.write(self.results / 'chordext-fuzz/results.json', self.fuzz)
        self.assertEqual(self.check(), 1)

    def test_full_gate_cannot_use_smoke_results(self):
        self.run['gate'] = 'all'
        self.write(self.root / 'timing.json', self.run)
        self.assertEqual(self.check(), 1)

    def test_compile_failure_is_not_mutation_detection(self):
        self.mutations['mutants'][0]['compile_exit'] = 1
        self.write(self.results / 'chordext-mutants/results.json', self.mutations)
        self.assertEqual(self.check(), 1)

    def test_changed_order_or_new_omission_fails_comparison(self):
        baseline = self.root.parent / 'before'
        shutil.copytree(self.root, baseline)
        result = json.loads((self.results / 'effects.json').read_text())
        result['checks'][0]['case'] = 'different'
        result['skips'] = ['new omission']
        self.write(self.results / 'effects.json', result)
        self.assertEqual(self.check('--before', str(baseline)), 1)


if __name__ == '__main__':
    unittest.main()
