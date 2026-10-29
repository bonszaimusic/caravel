#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Verify the linked DSP RAM image, boot copy and executable-window bounds."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess

from audio_ram_ir import HOT

ROOT = Path(__file__).resolve().parents[1]
XIP = 0x02000120
RAM_LO, RAM_HI = 0x01C00000, 0x01C06000


def boot_copy(ops, start, end, load):
    """Check the actual target's bounded word-copy loop, not source spelling."""
    for i, (address, op) in enumerate(ops):
        read = re.fullmatch(r"r(\d+) = \[r(\d+)\+\+=4\]", op.strip())
        if not read or i + 2 >= len(ops):
            continue
        write = re.fullmatch(r"\[r(\d+)\+\+=4\] = r" + read[1], ops[i + 1][1].strip())
        branch = re.fullmatch(r"if \(r(\d+) < r(\d+)\) goto -?\d+ <[^>]*: ([0-9a-f]+) >", ops[i + 2][1])
        if not write or not branch or branch[1] != write[1] or int(branch[3], 16) != address:
            continue
        constants = {}
        for _, previous in ops[max(0, i - 9):i]:
            value = re.fullmatch(r"r(\d+) = (\d+)(?: <.*>)?", previous.strip())
            if value:
                constants[value[1]] = int(value[2])
        if (constants.get(read[2]), constants.get(write[1]), constants.get(branch[2])) != (load, start, end):
            continue
        assert not any(op == "sti" for _, op in ops[:i]), "interrupts enabled before DSP copy"
        following = [op for _, op in ops[i + 3:]]
        assert "csync" in following, "no visibility fence after DSP copy"
        assert "sti" not in following[:following.index("csync")], "interrupts enabled before copy fence"
        return
    raise AssertionError("target startup is missing the bounded DSP load-to-run copy")


def main():
    out = ROOT / "build"
    objdump = Path(os.environ["JIELI_TOOLCHAIN"]) / "common/bin/objdump"
    elf = str(out / "felucca.elf")
    sections_text = subprocess.check_output([str(objdump), "-h", elf], text=True)
    symbols_text = subprocess.check_output([str(objdump), "-t", elf], text=True)
    sections = {m[1]: (int(m[2], 16), int(m[3], 16)) for m in re.finditer(
        r"^\s*\d+\s+(\.\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)", sections_text, re.M)}

    def sym(name):
        m = re.search(r"^([0-9a-f]+).*\s" + re.escape(name) + r"$", symbols_text, re.M)
        assert m, "missing symbol " + name
        return int(m[1], 16)

    start, end, load = (sym("_audio_" + name) for name in ("start", "end", "load"))
    assert not ((start | end | load) & 3), "DSP copy boundaries are not word aligned"
    assert RAM_LO <= sym("_rt_start") <= sym("_rt_end") <= start <= end <= sym("_c1_start") <= sym("_c1_end") <= RAM_HI
    assert sym("_rt_load") + sections[".ram_text"][0] <= load
    assert load + end - start <= sym("_c1_load") <= sym("_data_load")
    enabled = os.environ.get("CARAVEL_AUDIO_RAM", "1") != "0"
    if enabled:
        assert sections.get(".audio_text") == (end - start, start) and end > start
        assert all(start <= sym(name) < end for name in HOT), "DSP function outside the copied image"
        image = (out / "felucca.bin").read_bytes()
        payload = (out / "audiotext.bin").read_bytes()
        assert len(payload) == end - start
        assert image[load - XIP:load - XIP + len(payload)] == payload, "package omits/misplaces DSP code"

        spec = importlib.util.spec_from_file_location("target_budget", ROOT / "tests/target_budget.py")
        budget = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(budget)
        budget.FUNCS += ["fm1_cstart", "fm1_guard_enable"]
        functions = budget.functions(out / "felucca.dis")
        boot_copy(functions["fm1_cstart"], start, end, load)
        guard_end = sym("_c1_end") if os.environ.get("FELUCCA_CPU2") != "0" else end
        # The one-core compiler can inline the guard into cstart.
        guard_ops = functions.get("fm1_guard_enable", functions["fm1_cstart"])
        constants = {int(m[1]) for _, op in guard_ops
                     if (m := re.search(r"= (\d+) <", op))}
        assert sym("_rt_start") in constants and guard_end in constants, "PC guard does not include DSP RAM"
    else:
        assert start == end and not sections.get(".audio_text", (0, 0))[0]
    if ".c1_stack" in sections:
        size, base = sections[".c1_stack"]
        assert base >= sym("_c1_end") and base + size <= RAM_HI
        if os.environ.get("FELUCCA_CPU2") != "0":
            assert size == 4352
        else:
            assert size < 8  # alignment only; no worker stack
    print(f"audio RAM: {end-start} B, exact package bytes, bounded startup copy, PC window and worker stacks OK")


if __name__ == "__main__":
    main()
