#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Dynamic worker budgets preserve coverage, order and exceptions."""
import concurrent.futures
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest

sys.path.insert(0, str(Path(__file__).parents[1]))
from chordext_fuzz import bounded_map


class FuzzWorkerTests(unittest.TestCase):
    def test_budget_increase_preserves_order_and_maximum(self):
        with tempfile.TemporaryDirectory() as tmp:
            budget = Path(tmp) / 'workers'
            budget.write_text('1')
            started = threading.Event()
            release = threading.Event()
            lock = threading.Lock()
            active, peak, seen = 0, 0, []

            def one(item):
                nonlocal active, peak
                with lock:
                    active += 1
                    peak = max(peak, active)
                    seen.append(item)
                if item == 0:
                    started.set()
                    self.assertTrue(release.wait(1))
                time.sleep(.02)
                with lock:
                    active -= 1
                return item * 2

            with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
                result = pool.submit(bounded_map, one, list(range(12)), 3, budget)
                self.assertTrue(started.wait(1))
                time.sleep(.03)
                with lock:
                    self.assertEqual(seen, [0])
                    self.assertEqual(peak, 1)
                budget.write_text('3')
                release.set()
                self.assertEqual(result.result(), [n * 2 for n in range(12)])
            self.assertEqual(peak, 3)
            self.assertEqual(sorted(seen), list(range(12)))

    def test_worker_exception_fails_the_pool(self):
        def one(item):
            if item == 2:
                raise RuntimeError('deliberately broken fuzz case')
            return item
        with self.assertRaisesRegex(RuntimeError, 'deliberately broken'):
            bounded_map(one, list(range(6)), 3)

    def test_invalid_budget_cannot_produce_a_green_empty_run(self):
        with tempfile.TemporaryDirectory() as tmp:
            budget = Path(tmp) / 'workers'
            for value in ('0', '-1', 'invalid'):
                budget.write_text(value)
                with self.assertRaises(ValueError):
                    bounded_map(lambda n: n, [1, 2], 3, budget)


if __name__ == '__main__':
    unittest.main()
