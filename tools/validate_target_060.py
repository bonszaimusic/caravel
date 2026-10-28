#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""STASIS reduced profile plus root tests outside the legacy host runner."""
import json, os, re, shlex, subprocess, sys, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"build/validation"; OUT.mkdir(parents=True,exist_ok=True)
results=[]
def command(name,args,env=None):
    log=OUT/(re.sub(r"[^a-zA-Z0-9_-]","_",name)+".log")
    start=time.monotonic()
    with log.open("w") as f:
        p=subprocess.run(args,cwd=ROOT,env={**os.environ,**(env or {})},
                         stdout=f,stderr=subprocess.STDOUT)
    results.append({"case":name,"exit_code":p.returncode,
                    "seconds":round(time.monotonic()-start,2),"log":log.name})
    print(name, "PASS" if not p.returncode else "FAIL",flush=True)
    if p.returncode: print(log.read_text(errors="replace")[-12000:],flush=True)
    return not p.returncode
def ccase(name,path,flags=(),args=()):
    exe=OUT/name
    cc=[*shlex.split(os.environ.get("CC","cc")),"-O2","-w","-ffp-contract=off",
        "-Ibuild/gen","-Ifirmware/src",*flags,path,"-lm","-o",str(exe)]
    if command(name+"-compile",cc):return command(name,[str(exe),*args])
    return False
minimal=["-DCARAVEL_REVERB_MINIMAL=1","-DFELUCCA_SLICE=0"]
ccase("stasis-expanded-continuity","tests/stasis_continuity_test.c",
      [*minimal,"-DCARAVEL_STASIS_EXPAND=1"])
ccase("stasis-target","tests/stasis_test.c",minimal)
ccase("stasis-complete-profile","tests/stasis_test.c",[*minimal,"-DSTASIS_FULL_PROFILE=1"])
for hw in [0,1]:
    ccase("stasis-layer-"+str(hw),"tests/stasis_layer_test.c",[*minimal,"-DFELUCCA_PERF_HW="+str(hw)])
ccase("compat-target","tests/project_compat_test.c",[*minimal,"-DFELUCCA_PERF_HW=1"])
san=["-O1","-g","-fsanitize=address,undefined",
     "-fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow",
     "-fno-sanitize-recover=undefined"]
ccase("stasis-target-sanitized","tests/stasis_test.c",[*minimal,*san])
ccase("stasis-complete-profile-sanitized","tests/stasis_test.c",[*minimal,"-DSTASIS_FULL_PROFILE=1",*san])
for hw in [0,1]:
    ccase("stasis-layer-sanitized-"+str(hw),"tests/stasis_layer_test.c",
          [*minimal,"-DFELUCCA_PERF_HW="+str(hw),*san])
ccase("compat-target-sanitized","tests/project_compat_test.c",
      [*minimal,"-DFELUCCA_PERF_HW=1",*san])
command("target-budget-unit",[sys.executable,"tests/target_budget_test.py"])
command("target-budget-actual",[sys.executable,"tests/target_budget.py",
                               "build/felucca.dis","tests/target_budget.txt"])
ccase("d1-diag-time","experiments/dualcore-d1/diag_time_test.c")
ccase("home-diag-time","experiments/home-perf/diag_time_test.c")
ccase("captured-controls","experiments/home-perf/captured_audio_test.c",minimal)
hashes={}
for capture in [0,1]:
    name="captured-controls-isolated-"+str(capture)
    if command(name,[str(OUT/"captured-controls")],{"CAPTURE_DIAGNOSTICS":str(capture)}):
        for line in (OUT/(name+".log")).read_text().splitlines():
            m=re.fullmatch(r"reverse=(\d+) variant=(\d+) capture=(\d+) hash=([0-9a-f]+)",line)
            if m: hashes.setdefault((m[1],m[2]),{})[m[3]]=m[4]
ok=len(hashes)==8 and all(set(v)=={"0","1"} and v["0"]==v["1"] for v in hashes.values())
results.append({"case":"capture-diagnostics-audio-identity","exit_code":0 if ok else 1})
print("capture-diagnostics-audio-identity",ok,flush=True)
# Compare reference feature-off vs HOME/PAIR enabled with neutral controls.
reg=[]
for mode,flags in [("off",[]),("home",["-DFELUCCA_HOME_PERF=1","-DFELUCCA_PAIR_MORPH=1"]),
                   ("hardware",["-DFELUCCA_HOME_PERF=1","-DFELUCCA_PAIR_MORPH=1","-DFELUCCA_PERF_HW=1"])]:
    name="audio-reference-"+mode
    if ccase(name,"experiments/home-perf/audio_regression_test.c",flags):
        text=(OUT/(name+".log")).read_text()
        reg.append(text)
        if mode!="off":command(name+"-macros",[str(OUT/name),"macros"])
if len(reg)==3:
    results.append({"case":"neutral-home-reference-audio-identity",
                    "exit_code":0 if len(set(reg))==1 else 1})
command("d3-exact-profile",[sys.executable,"experiments/dualcore-d1/run_tests.py"],
        {"D3_TEST_CFLAGS":" ".join(minimal)})
ccase("drone-cells-strict-ubsan","tests/drone_cells_test.c",
      ["-O1","-fsanitize=undefined","-fno-sanitize=shift","-fno-sanitize-recover=all"],
      ["--dsp-only"])
(OUT/"target-results.json").write_text(json.dumps(results,indent=2)+"\n")
sys.exit(1 if any(r["exit_code"] for r in results) else 0)

