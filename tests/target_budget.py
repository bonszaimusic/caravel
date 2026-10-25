#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Target-side cost estimate of the hot DSP code, from the pi32v2 objdump listing tools/build.py writes
(build/felucca.dis). For each engine's render function and the audio ISR: the instructions
inside natural loops (control-flow backedges whose heads dominate their latches: the per-sample loops and what they
contain), and the hardware divides and calls in them. cost = the loop instructions, each weighted 4 per
level of nesting (an inner loop runs several times per sample) and 1 + 8 for a divide (many cycles). A static count, not a cycle count: it changes only when the compiled code changes,
so it is exact run to run, and it catches a render loop that grew (more work per sample, a new divide,
something not inlined any more). Compared with tests/target_budget.txt at +10 %. The host
CPU check (regress.c) is the primary one; this one sees the target compiler's code.
  tests/target_budget.py [DIS [BUDGET]]      BUDGET_UPDATE=1 rewrites BUDGET"""
import os
import re
import sys

try:
    from target_loops import Instructions, natural_loops, FlowError
except ModuleNotFoundError:
    # tools/check_perf_audio.py imports this module by file path.
    sys.path.insert(0, str(__import__("pathlib").Path(__file__).resolve().parent))
    from target_loops import Instructions, natural_loops, FlowError

NEW_LFO2 = {'track_lfo2_tick', 'lfo2_sync_inc', 'lfo2_trigger', 'mod_kind'}
NEW_RENDERERS = NEW_LFO2 | {'drone_render', 'drone_legacy_render', 'drone_cell_render', 'hades_render', 'sem_render', 'drumx_render', 'dx_run', 'dx_layer', 'drumx_mix', 'analog_morph_render', 'lofi_morph_render', 'phase_render_ext'}
FUNCS = sorted(NEW_LFO2) + ['drone_render', 'drone_legacy_render', 'drone_cell_render', 'hades_render', 'sem_render'] + ['drumx_render', 'dx_run', 'dx_layer', 'drumx_mix'] + ['phase_render_ext', 'analog_morph_render', 'lofi_morph_render'] + ["analog_render", "digital_render", "digital_render_legacy", "digital_render_custom", "phase_render", "phase_render_f", "lofi_render", "lofi_formula_render", "lofi_byte", "lofi_float", "sample_render", "formant_render",
         "trio_render", "trio_pass", "wheel_render", "wheel_block",
         "grain_render", "grain_block", "phys_render", "drum_render", "noise_render", "fm6_render", "fm6_op_run", "fm6_op_fb", "px_modal_block", "px_modal_run", "px_memb_block",
         "px_string_excite", "px_string_run", "px_symp_run",
         "dv_metal_run", "dv_kick_run", "dv_snare_run", "dv_clap_run", "dv_hat_run", "dv_tom_run",   # drum_voice.c
         "dv_rim_run", "dv_bell_run", "dv_cym_run", "dv_out", "dv_metal_mix",
         "slicer_track",
         "slice_render", "slc_rev",                          # SLICE (eng_slice.c): the render, the reverse windows
         "fm1_alnk0_irq", "fm1_timer5_irq",               # the audio ISR; TIMER5: the key / LED scan (hal/fm1_input.h)
         "project_pattern_tick", "project_sound_apply", "project_pattern_engines_ok",  # rare boundary helpers
         "mix_part", "mix_part_inner", "track_render",                       # a part's block (fx.c), on either core since the second
         "uac_tap48",                                       # the USB audio input at 48 kHz: the 44.1 -> 48 resampler (usb.c)
         "click_render",                                    # the metronome's click (click.c), while it sounds
         "mod_begin", "mod_voice", "mod_end",                 # the modulation matrix (mod.c), called when active
         "perf_begin", "perf_mute", "perf_pre", "perf_block", "perf_master",   # the FX layer (perform.c), when busy
         "perf_flanger", "perf_phaser",                     # .. FLANGER, PHASER (1.2), while held or fading
         "perf_throw",                                      # .. REV / ECHO THROW (1.5.1): per part, while held or gliding
         "rev_room", "rev_spring", "rev_hall", "rev_side_mix",   # the reverb bus (fx.c): REVERB TYPE ROOM / SPRING / HALL (and its stereo)
         "track_insert", "ins_drive", "ins_swept",          # the per-track INSERT (fx.c, 1.5), while a TYPE plays
         "track_filter", "tf_wet_block"]                                    # the per-track FILTER (fx.c, 0.3.0), while FILT is set or fading (twice a block with SPREAD)
# built only with FELUCCA_FM4=1 (DIGITAL, src/eng_digital.c; not in the default build, so not in BUDGET): absent,
# they are skipped; present, checked against these (their budget lines until the engine was retired in 1.0)
OPTIONAL = {"digital_render": 12, "digital_render_legacy": 333, "digital_render_custom": 558,
            "uac_tap48": 504,           # (absent from a FELUCCA_UAC_48K=0 or FELUCCA_UAC=0 build)
            "mix_part": 2800, "mix_part_inner": 2800, "track_render": None}           # (FELUCCA_CPU2=1, the default since 1.6.1: out of line, either core may run it)
# a FELUCCA_CPU2=0 build (no mix_part: inlined into the audio ISR, as 1.6) has the one-core ISR, checked against this
ONE_CORE = {"fm1_alnk0_irq": 37502}
TOL = 0.10                      # exact (no noise): small edits pass, a grown render loop does not
DIV_W = 8                       # a divide weighs 1 + 8 instructions
NEST = 4                        # an instruction in a loop inside a loop weighs 4, two deep 16, ...
MAXD = 4

dis = sys.argv[1] if len(sys.argv) > 1 else "build/felucca.dis"
budget = sys.argv[2] if len(sys.argv) > 2 else "tests/target_budget.txt"
LABEL = re.compile(r"^([A-Za-z_][A-Za-z_0-9.]*):$")
INSN = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(.*)$")
TARGET = re.compile(r"goto -?\d+ <[^>]*: ([0-9a-f]+) >")
DIVIDE = re.compile(r"= r\d+(?:_r\d+)? / r\d+(?:_r\d+)?")


def functions(path):
    """Instruction lists with byte/halfword switch and predicated-packet edges.

    JieLi TBB/TBH entries are halfword displacements from the table start.
    Alignment padding is not an instruction and does not create an edge.
    """
    out, cur = {}, None
    pending_switch = None
    table_base = None
    table_targets = []
    predicates, pending_predicate = [], []
    # A table's targets can occur after the table, so defer final filtering.
    tables = []
    with open(path) as f:
        for line in f:
            text = line.strip()
            m = LABEL.match(text)
            if m:
                name = m.group(1)
                if not name.startswith("."):
                    if pending_switch is not None:
                        tables.append((cur, pending_switch, table_targets, table_base))
                    cur = out.setdefault(name, Instructions()) if name in FUNCS else None
                    pending_switch = None; table_base = None; table_targets = []
                    predicates = []; pending_predicate = []
                continue
            if cur is None:
                continue
            if text.startswith("}") and predicates:
                pending_predicate.append(predicates.pop())
                continue
            m = INSN.match(line)
            if not m:
                continue
            addr, op = int(m.group(1), 16), m.group(3).strip()
            if op.startswith("<"):
                entry = re.fullmatch(r"< (-?\d+) : 0x[0-9A-Fa-f]+ >", op)
                if pending_switch is not None and entry:
                    if table_base is None: table_base = addr
                    table_targets.append(table_base + 2*int(entry.group(1)))
                continue
            if pending_switch is not None:
                tables.append((cur, pending_switch, table_targets, table_base))
                pending_switch = None; table_base = None; table_targets = []
            for condition in pending_predicate:
                cur.predicates[condition] = addr
            pending_predicate = []
            cur.append((addr, op))
            if re.search(r"\bifs?\s*\(", op) and op.endswith("{"):
                predicates.append(addr)
            if op.startswith(("tbb ", "tbh ")):
                pending_switch = addr
    if pending_switch is not None:
        tables.append((cur, pending_switch, table_targets, table_base))
    for function, addr, targets, base in tables:
        # A zero displacement is alignment padding, never executable table data.
        # Any other unresolved target is retained so CFG analysis fails closed.
        function.switches[addr] = set(targets) - {base}
    return out


FORMULA_CORES = {"lofi_byte", "lofi_float"}



def cost(insns, formula_core=False):
    """One natural loop per dominating header, including all its latches.
    Instructions in d loop sets weigh NEST ** (d-1). Calls are counted but
    their bodies are not recursively charged; this remains a static guard.
    """
    if formula_core:
        # Formula helpers run per sample; include their straight-line cases.
        return {"insns": len(insns), "loop": 0, "div": 0, "call": 0, "cost": len(insns)}
    if not insns:
        return {"insns": 0, "loop": 0, "div": 0, "call": 0, "cost": 0}
    try:
        loops = natural_loops(insns)
    except FlowError:
        # A conservative fallback for unknown/irreducible target control flow.
        # Main reports these functions; never silently claim full CFG coverage.
        lo = insns[0][0]; heads = {}
        for addr, text in insns:
            match = TARGET.search(text)
            if match and lo <= int(match.group(1), 16) < addr:
                head = int(match.group(1), 16)
                heads[head] = max(heads.get(head, addr), addr)
        loops = [{addr for addr, _ in insns if h <= addr <= end} for h, end in heads.items()]
    n = divs = calls = 0
    w = 0
    for a, t in insns:
        d = min(sum(1 for loop in loops if a in loop), MAXD)
        if not d:
            continue
        k = NEST ** (d - 1)
        n += 1
        if DIVIDE.search(t):
            divs += 1
            k *= 1 + DIV_W
        calls += t.startswith("call")
        w += k
    return {"insns": len(insns), "loop": n, "div": divs, "call": calls, "cost": w}


def main():
    if not os.path.exists(dis):
        print(f"target: skip ({dis} missing: run ./build.sh)")
        return 0
    fns = functions(dis)
    res = {n: cost(fns[n], n in FORMULA_CORES or n in NEW_LFO2) for n in FUNCS if fns.get(n)}
    # Custom CPU2 keeps track_render out of line to reduce mixer register pressure.
    # Charge its complete measured voice loop to the original mix_part budget;
    # extracting it must not hide any renderer work or increase that budget.
    if "mix_part" in res and "track_render" in res:
        helper = res.pop("track_render")
        res["mix_part"] = {key: value + helper[key] for key, value in res["mix_part"].items()}
    missing = [n for n in FUNCS if n not in res and n not in OPTIONAL]
    base = {}
    if os.path.exists(budget):
        for line in open(budget):
            p = line.split()
            if len(p) >= 2 and not line.startswith("#"):
                base[p[0]] = int(p[1])
    if os.environ.get("BUDGET_UPDATE"):
        with open(budget, "w") as f:
            f.write("# FELUCCA target cost budget (tests/target_budget.py): instructions in the loops of each\n"
                    f"# function in build/felucca.dis, x{NEST} per nesting level, divides x{1 + DIV_W}. The check allows "
                    f"+{TOL * 100:.0f} %.\n# Rewritten by BUDGET_UPDATE=1.\n")
            for n, r in res.items():
                if n not in OPTIONAL:
                    f.write(f"{n} {r['cost']}\n")
        print(f"target: budget {budget} rewritten ({len(res)} functions)")
    fail = 0
    for name, instructions in fns.items():
        try: natural_loops(instructions)
        except FlowError as error:
            print(f"target: FAIL CFG {name}: {error}; conservative span estimate shown")
            fail += 1
    for n in missing:
        print(f"target: FAIL {n} not found in {dis} (renamed? inlined? update FUNCS)")
        fail += 1
    for n, r in res.items():
        b = base.get(n, OPTIONAL.get(n))
        if "mix_part" not in res and n in ONE_CORE:
            b = ONE_CORE[n]
        state = "no budget (BUDGET_UPDATE=1 adds it)" if b is None else "ok"
        if b is None and n in NEW_RENDERERS and not os.environ.get("BUDGET_UPDATE"):
            fail += 1
        if b is not None and not os.environ.get("BUDGET_UPDATE"):
            if r["cost"] > b * (1 + TOL):
                state = f"OVER BUDGET (+{(r['cost'] / b - 1) * 100:.0f} %, limit +{TOL * 100:.0f} %)"
                fail += 1
            elif r["cost"] < b * (1 - TOL):
                state = f"note: {(r['cost'] / b - 1) * 100:.0f} % (BUDGET_UPDATE=1 to keep it)"
        print(f"target: {n:15s} {r['insns']:5d} instructions, {r['loop']:4d} in loops, {r['div']} divides, "
              f"{r['call']:2d} calls there: cost {r['cost']:5d} (budget {b if b is not None else '-'}) {state}")
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())

