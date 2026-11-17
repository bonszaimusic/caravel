#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""CAPTURE on the device, the static half of the block-time question (plan C-e): for each CAPTURE function the target compiler left
in the listing of a build with CARAVEL_UX_CAPTURE=1 (build/felucca.dis): its instructions, those inside loops, hardware divides and
calls (the same reading as tests/target_budget.py, same caveat: a static count, not cycles; calls are counted, their bodies not).
  tools/capture_cost.py [DIS]
Then the worst block of four full loops is bounded by hand from the loop sizes, see docs/CAPTURE_GERAET_UMSETZUNG."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tests"))
import target_budget as tb

NAMES = ["cap_trim", "cap_commands", "cap_take", "cap_pause", "cap_resume", "cap_clear", "cap_wipe", "cap_release", "cap_all_pause",
         "cap_publish", "cap_idle_body", "cap_idle", "cap_tick", "cap_fire", "cap_play_on", "cap_play_off", "cap_put", "cap_log",
         "cap_close_live", "cap_note_held", "cap_mapping_held", "cap_clock_now", "cap_age"]
tb.FUNCS = NAMES
dis = sys.argv[1] if len(sys.argv) > 1 else "build/felucca.dis"
funcs = tb.functions(dis)
print(f"{'function':16} {'insns':>6} {'in loops':>9} {'divides':>8} {'calls':>6}")
for n in NAMES:
    if n not in funcs:
        print(f"{n:16} (not in the listing: inlined or absent)")
        continue
    c = tb.cost(funcs[n])
    print(f"{n:16} {c['insns']:6d} {c['loop']:9d} {c['div']:8d} {c['call']:6d}")
