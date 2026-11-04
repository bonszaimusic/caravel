#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Run the `run:` steps of one job of a GitHub workflow file here, in order, as the Actions runner would (bash -eo pipefail), so the local
check cannot drift from the CI. Steps with `uses:` (checkout, setup-*, upload/download-artifact) are skipped; the tools they install are in
the image. `${{ runner.temp }}`, `${{ github.workspace }}` and `${{ matrix.KEY }}` are filled in; any other expression is an error.

  run_steps.py WORKFLOW.yml JOB [--matrix KEY=VALUE] [--only SUBSTRING] [--list]
"""
import argparse, os, re, subprocess, sys, time
import yaml

ap = argparse.ArgumentParser()
ap.add_argument("workflow"); ap.add_argument("job")
ap.add_argument("--matrix", action="append", default=[])
ap.add_argument("--only", help="run only steps whose name contains this text")
ap.add_argument("--list", action="store_true")
a = ap.parse_args()
wf = yaml.safe_load(open(a.workflow))
job = wf["jobs"][a.job]
matrix = dict(m.split("=", 1) for m in a.matrix)
temp = os.environ.setdefault("RUNNER_TEMP", "/tmp/runner")
os.makedirs(temp, exist_ok=True)
# the image keeps its tools in /opt; the workflows look in $RUNNER_TEMP
for name, target in (("emsdk", "/opt/emsdk"), ("jieli", "/opt/jieli"), ("ac79-sdk", "/opt/ac79-sdk"), ("DaisySP", "/opt/DaisySP")):
    link = os.path.join(temp, name)
    if not os.path.exists(link):
        os.symlink(target, link)

def fill(text, where):
    def sub(m):
        key = m.group(1).strip()
        if key == "runner.temp": return temp
        if key == "github.workspace": return os.getcwd()
        if key.startswith("matrix.") and key[7:] in matrix: return matrix[key[7:]]
        raise SystemExit(f"{where}: cannot evaluate ${{{{ {key} }}}} locally")
    return re.sub(r"\$\{\{(.*?)\}\}", sub, text)

def enabled(step):
    cond = str(step.get("if", "")).strip()
    if not cond or cond in ("always()", "success()"): return True
    m = re.fullmatch(r"matrix\.(\w+)\s*==\s*'([^']*)'", cond)
    if m: return matrix.get(m.group(1)) == m.group(2)
    raise SystemExit(f"step '{step.get('name')}': cannot evaluate if: {cond}")

# tools the image already holds: their install steps would only download them again
SKIP = ("Install JieLi toolchain", "Fetch and hash-verify pinned JieLi", "Install pinned", "Install Python dependencies", "Pin DaisySP")
steps = [s for s in job["steps"] if "run" in s and enabled(s) and not any(k in s.get("name", "") for k in SKIP)]
if a.only: steps = [s for s in steps if a.only in s.get("name", "")]
if a.list:
    for s in steps: print(s.get("name", "(unnamed)"))
    sys.exit(0)
base_env = dict(os.environ)
base_env.update({str(k): fill(str(v), "env") for k, v in {**(wf.get("env") or {}), **(job.get("env") or {})}.items()})
failed = []
t_all = time.time()
for i, s in enumerate(steps, 1):
    name = s.get("name", f"step {i}")
    env = dict(base_env)
    env.update({str(k): fill(str(v), name) for k, v in (s.get("env") or {}).items()})
    for k in filter(None, os.environ.get("DROP_ENV", "").split(",")):   # e.g. DROP_ENV=TSAN: a step's own TSAN: '1' must not run where ThreadSanitizer cannot
        env.pop(k, None)
    print(f"\n##### [{i}/{len(steps)}] {name}", flush=True)
    t0 = time.time()
    r = subprocess.run(["bash", "--noprofile", "--norc", "-eo", "pipefail", "-c", fill(s["run"], name)], env=env)
    print(f"##### {'ok' if r.returncode == 0 else 'FAILED (exit %d)' % r.returncode}  {time.time() - t0:.0f} s", flush=True)
    if r.returncode:
        failed.append(name)
        break
print(f"\n===== {a.job}: {len(steps)} steps, {'FAILED at: ' + failed[0] if failed else 'all passed'}, {time.time() - t_all:.0f} s")
sys.exit(1 if failed else 0)
