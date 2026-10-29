#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Place already optimized DSP in RAM and bridge the pi32v2 call range.

Section placement happens after inlining: clang 4 otherwise refuses to inline
XIP helpers into a RAM function. A volatile, stack-local function pointer is
the same far-call mechanism used by fm1_flash.h. One slot per caller bounds
stack use even for calls in sample loops. No shared mutable state is added.
"""
import re
import sys
from pathlib import Path

HOT = frozenset({"drone_legacy_render", "drone_cell_render", "drone_slow_tick",
                 "sem_render", "hades_render", "stasis_block", "rev_hall"})
NAME = r"[-\w.$]+"
HEADER = re.compile(r"^define\s+(.+?)@(" + NAME + r")\((.*)\)([^\n]*)\{", re.M)
DECL = re.compile(r"^declare\s+(.+?)@(" + NAME + r")\((.*)\)([^\n]*)$", re.M)
TYPE = re.compile(r"(?:void|i\d+|float|double|%[-\w.$]+)\**$")
CALL = re.compile(r"\bcall\b[^@\n]*@(" + NAME + r")\(")


def signature(match):
    ret = match[1].split()[-1]
    args = [] if not match[3].strip() else [p.strip().split()[0] for p in match[3].split(",")]
    if not TYPE.fullmatch(ret) or any(a != "..." and not TYPE.fullmatch(a) for a in args):
        raise ValueError("unsupported far-call signature: " + match[0])
    return ret + " (" + ", ".join(args) + ")*"


def transform(ir):
    headers = list(HEADER.finditer(ir))
    missing = HOT - {m[2] for m in headers}
    if missing:
        raise ValueError("missing noinline DSP functions: " + ", ".join(sorted(missing)))
    if "caravel_far_slot" in ir:
        raise ValueError("input already contains far-call bridges")
    definitions = {m[2]: m for m in [*headers, *DECL.finditer(ir)]}
    edits, count = [], 0
    for m in headers:
        end = ir.index("\n}", m.end())
        body = ir[m.end():end]
        lines, rewritten, calls = body.splitlines(), [], 0
        for line in lines:
            call = CALL.search(line)
            if call and not call[1].startswith("llvm.") and ((m[2] in HOT) != (call[1] in HOT)):
                callee = call[1]
                if callee not in definitions:
                    raise ValueError("unknown far-call target: " + callee)
                typ = signature(definitions[callee])
                n = calls
                rewritten += [
                    f"  store volatile i8* bitcast ({typ} @{callee} to i8*), i8** %caravel_far_slot, align 4",
                    f"  %caravel_far_raw{n} = load volatile i8*, i8** %caravel_far_slot, align 4",
                    f"  %caravel_far_fn{n} = bitcast i8* %caravel_far_raw{n} to {typ}",
                ]
                line = line.replace("@" + callee + "(", f"%caravel_far_fn{n}(", 1)
                line = re.sub(r"\btail call\b", "call", line)
                calls += 1
            rewritten.append(line)
        if calls:
            # Entry may have an explicit named label. Allocate once there,
            # never at a call site that could be visited repeatedly.
            at = next((i for i, line in enumerate(rewritten)
                       if line.strip() and not line.lstrip().startswith(";")), len(rewritten))
            if at < len(rewritten) and re.fullmatch(r"\s*[-\w.$]+:\s*(?:;.*)?", rewritten[at]):
                at += 1
            rewritten.insert(at, "  %caravel_far_slot = alloca i8*, align 4")
            count += calls
        header = m[0]
        if m[2] in HOT:
            if " section " in header:
                raise ValueError("DSP already assigned to another section: " + m[2])
            header = header[:-1] + 'section ".audio_text" {'
        if calls or m[2] in HOT:
            edits.append((m.start(), end, header + "\n" + "\n".join(rewritten)))
    for start, end, text in reversed(edits):
        ir = ir[:start] + text + ir[end:]
    return ir, count


if __name__ == "__main__":
    result, count = transform(Path(sys.argv[1]).read_text())
    Path(sys.argv[2]).write_text(result)
    print(f"audio RAM: {len(HOT)} optimized DSP functions; {count} bounded far-call sites")
