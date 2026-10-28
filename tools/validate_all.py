#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run every existing host-suite command in bounded, independent CI shards.
The shell commands are extracted unchanged from tests/run_tests.sh; anchors fail
closed if the suite changes. Full host reference != the reduced STASIS binary.
No sound, CPU or target baselines are rewritten.
"""
import argparse, json, os, re, shlex, subprocess, sys, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/validation"
SHARDS = ["storage_usb", "synthesis_midi", "ui_storage", "effects",
          "cpu_dual", "engines_samples", "sanitizers", "web_target"]
def parts():
    s = (ROOT / "tests/run_tests.sh").read_text()
    first = s.index('run "release artifacts:')
    host_if = s.index("if [ -f build/gen/felucca_tables.h ]; then", first)
    host = s.index('    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/hostsim"', host_if)
    ui = s.index('    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/theme_test"', host)
    fx = s.index('    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/mod_test"', ui)
    cpu = s.index('    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/regress"', fx)
    eng = s.index("    # Felucca-Engines (firmware/engines, MIT)", cpu)
    host_end = s.index('else\n    echo "== skip hostsim', eng)
    san = s.index("# ASan + UBSan:", host_end)
    web = s.index('run "regression: target cost', san)
    end = s.index('[ $fail -eq 0 ] && echo "ALL HOST TESTS PASSED"', web)
    pre = s[:first].replace('cd "$(dirname "$0")/.."', "cd " + shlex.quote(str(ROOT)))
    ranges = [(first,host_if),(host,ui),(ui,fx),(fx,cpu),(cpu,eng),
              (eng,host_end),(san,web),(web,end)]
    return pre, dict(zip(SHARDS, [s[a:b] for a,b in ranges]))

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("shard", choices=SHARDS + ["inventory"])
    a = ap.parse_args()
    pre, groups = parts()
    OUT.mkdir(parents=True, exist_ok=True)
    source = (ROOT / "tests/run_tests.sh").read_text()
    inventory = {k: re.findall(r'run "([^"]+)"', v) for k,v in groups.items()}
    tests = sorted(p.as_posix() for p in Path("tests").glob("*_test.c"))
    missing = [p for p in tests if p not in source and p not in
               ["tests/stasis_test.c", "tests/stasis_layer_test.c", "tests/stasis_continuity_test.c", "tests/project_compat_test.c"]]
    (OUT / "host-inventory.json").write_text(json.dumps({
        "source":"tests/run_tests.sh", "shards":inventory,
        "separate_target_tests":["tests/stasis_test.c","tests/stasis_layer_test.c","tests/stasis_continuity_test.c","tests/project_compat_test.c"],
        "uncovered_root_tests":missing}, indent=2)+"\n")
    if missing: raise SystemExit("Uncovered root tests: "+", ".join(missing))
    if a.shard == "inventory":
        print(json.dumps(inventory, indent=2)); return
    # Inputs are actual compiled firmware/loader. No synthetic firmware replaces them.
    for p in ["build/caravel.fwsc","build/felucca.bin","build/loader/ota.bin",
              "build/gen/felucca_tables.h"]:
        if not (ROOT/p).is_file(): raise SystemExit("Missing test input: "+p)
    if a.shard == "web_target":
        for cmd in ["node","emcc"]:
            if subprocess.run(["sh","-c","command -v "+cmd],capture_output=True).returncode:
                raise SystemExit("Required tool missing: "+cmd)
    if a.shard == "engines_samples":
        if not (Path(os.environ["DAISYSP"])/"Source/PhysicalModeling").is_dir():
            raise SystemExit("Required DaisySP reference checkout missing")
        if '{"PIANO", 0, 5},' not in (ROOT/"build/gen/felucca_samples.h").read_text():
            raise SystemExit("Required five-zone CC0 PIANO fixture missing")
    if a.shard == "engines_samples":
        subprocess.run([sys.executable,"tools/gen_waves.py","build/genwav"],cwd=ROOT,check=True)
    # The original suite creates these before sanitizers; each shard gets its own.
    fixture = r'''
head -c 200000 build/felucca.bin > "$OUT/old_app.bin"
python3 tools/fm1pkg_make.py "$OUT/old_app.bin" build/loader/ota.bin "$OUT/old.fwsc" >/dev/null
'''
    if a.shard == "web_target":
        fixture += r'''
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/descdump" tests/descdump.c -lm
"$OUT/descdump" > "$OUT/desc.json"
$CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/editor_test" tests/editor_test.c -lm
MENU_JSON="$OUT/menu.json" "$OUT/editor_test"
'''
    shell = OUT / (a.shard+".sh")
    shell.write_text(pre + fixture + groups[a.shard] + '\n[ "$fail" -eq 0 ]\n')
    start = time.monotonic()
    log = OUT / (a.shard+".log")
    with log.open("w") as f:
        p = subprocess.run(["sh", str(shell)], cwd=ROOT, stdout=f, stderr=subprocess.STDOUT)
    text = log.read_text(errors="replace")
    skips = [l for l in text.splitlines() if l.startswith("== skip") or "no BREAK" in l]
    result = {"shard":a.shard, "exit_code":p.returncode,
              "seconds":round(time.monotonic()-start,2),
              "checks":[l[3:] for l in text.splitlines() if l.startswith("== ")],
              "failures":[l for l in text.splitlines() if
                          re.search(r"\b(FAIL(?:ED)?|OVER BUDGET|runtime error|ERROR)\b",l)],
              "skips":skips, "profile":"legacy full-feature host; target budgets use actual STASIS image"}
    (OUT/(a.shard+".json")).write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps(result,indent=2), flush=True)
    if p.returncode: print(text[-16000:],file=sys.stderr)
    # A silently skipped optional suite is incomplete for this all-tests request.
    return 1 if p.returncode or skips else 0

if __name__ == "__main__": sys.exit(main())

