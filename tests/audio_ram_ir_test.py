#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Far-call ABI, bounded stack use and target startup-copy safety contracts."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from audio_ram_ir import HOT, transform
from check_audio_ram import boot_copy


def fixture():
    ir = '''target triple = "x86_64-unknown-linux-gnu"
@trace = internal global i32 5
@fmt = private constant [4 x i8] c"%u\\0A\\00"
declare i32 @printf(i8*, ...)
define internal fastcc i32 @cold(i32 %x) noinline {
entry:
  %a = load volatile i32, i32* @trace
  %b = xor i32 %a, %x
  %c = add i32 %b, 12345
  store volatile i32 %c, i32* @trace
  ret i32 %c
}
'''
    for k, name in enumerate(sorted(HOT)):
        call = "  %v = call fastcc i32 @cold(i32 %x)"
        if name == "stasis_block":
            call = "  %y = call i32 @rev_hall(i32 %x)\n  %v = call fastcc i32 @cold(i32 %y)"
        ir += f'''define i32 @{name}(i32 %x) noinline {{
entry:
{call}
  %r = add i32 %v, {k + 1}
  ret i32 %r
}}
'''
    return ir + '''define i32 @main() {
entry:
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %sum = phi i32 [ 7, %entry ], [ %b, %loop ]
  %a = call i32 @drone_cell_render(i32 %sum)
  %b = call i32 @stasis_block(i32 %a)
  %next = add i32 %i, 1
  %keep = icmp ult i32 %next, 1000000
  br i1 %keep, label %loop, label %exit
exit:
  %f = getelementptr [4 x i8], [4 x i8]* @fmt, i32 0, i32 0
  %p = call i32 (i8*, ...) @printf(i8* %f, i32 %b)
  ret i32 0
}
'''


class FarCalls(unittest.TestCase):
    def test_layout_calls_and_bounded_entry_slot(self):
        result, count = transform(fixture())
        self.assertEqual(count, 9)
        self.assertEqual(result.count('section ".audio_text"'), len(HOT))
        self.assertEqual(result.count("%caravel_far_slot = alloca"), 8)
        self.assertIn("%y = call i32 @rev_hall(i32 %x)", result)  # RAM -> RAM stays direct
        self.assertIn("call i32 (i8*, ...) @printf(i8* %f, i32 %b)", result)
        # Both calls in a million-iteration loop reuse the caller's one slot.
        main = result.split("define i32 @main()", 1)[1]
        self.assertLess(main.index("alloca"), main.index("loop:"))
        self.assertNotIn("alloca", main.split("loop:", 1)[1])
        self.assertIn("call fastcc i32 %caravel_far_fn0(i32 %x)", result)
        self.assertIn("call i32 %caravel_far_fn1(i32 %a)", result)

    def test_missing_roots_and_repeat_application_fail_closed(self):
        with self.assertRaisesRegex(ValueError, "missing noinline"):
            transform(fixture().replace("@sem_render(", "@missing_sem(", 1))
        rewritten, _ = transform(fixture())
        with self.assertRaisesRegex(ValueError, "already contains"):
            transform(rewritten)

    def test_unknown_target_and_unsupported_signature_fail_closed(self):
        with self.assertRaisesRegex(ValueError, "unknown far-call"):
            transform(fixture().replace("call fastcc i32 @cold(", "call fastcc i32 @unknown(", 1))
        source = fixture().replace("define internal fastcc i32 @cold(i32 %x)",
                                   "define internal fastcc i32 @cold(<4 x i32> %x)")
        with self.assertRaisesRegex(ValueError, "unsupported far-call signature"):
            transform(source)

    def test_tail_marker_removed_only_on_bridged_call(self):
        source = fixture().replace("%v = call fastcc", "%v = tail call fastcc")
        result, _ = transform(source)
        self.assertNotIn("tail call fastcc", result)

    def test_host_execution_preserves_results_and_side_effects(self):
        clang = os.environ.get("AUDIO_RAM_HOST_CLANG") or shutil.which("clang")
        if not clang:
            if os.environ.get("AUDIO_RAM_REQUIRE_RUNTIME") == "1":
                self.fail("clang is required for the native far-call ABI fixture")
            self.skipTest("host clang unavailable; CI requires the execution fixture")
        source = fixture()
        moved, _ = transform(source)
        (ROOT / "build/tmp").mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / "build/tmp") as directory:
            results = []
            for name, ir in [("baseline", source), ("ram", moved)]:
                path = Path(directory) / (name + ".ll")
                path.write_text(ir)
                executable = Path(directory) / name
                subprocess.run([clang, "-O0", "-Wno-override-module", "-no-pie", str(path), "-o", str(executable)], check=True)
                results.append(subprocess.check_output([str(executable)], timeout=20))
            self.assertEqual(results[0], results[1])
            self.assertRegex(results[0], rb"^\d+\n$")


class BootCopy(unittest.TestCase):
    def ops(self, source=2000, end=1040, early=False, fence=True):
        ops = [(0, "sti")] if early else []
        ops += [(2, f"r1 = {end}"), (4, "r2 = 1000"), (6, f"r3 = {source}"),
                (8, "r6 = [r3++=4]"), (10, "[r2++=4] = r6"),
                (12, "if (r2 < r1) goto -6 <boot: 8 >")]
        return ops + ([(14, "csync")] if fence else []) + [(16, "sti")]

    def test_actual_word_loop_contract(self):
        boot_copy(self.ops(), 1000, 1040, 2000)

    def test_wrong_source_or_extent_rejected(self):
        for ops in [self.ops(source=3000), self.ops(end=1036)]:
            with self.assertRaisesRegex(AssertionError, "missing the bounded"):
                boot_copy(ops, 1000, 1040, 2000)

    def test_interrupt_and_visibility_order(self):
        for ops in [self.ops(early=True), self.ops(fence=False)]:
            with self.assertRaises(AssertionError):
                boot_copy(ops, 1000, 1040, 2000)


if __name__ == "__main__":
    unittest.main()
