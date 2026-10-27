#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Fail the report on failed or missing machine-readable suite results."""
import json, sys, tempfile
from pathlib import Path

SHARDS=("storage_usb","synthesis_midi","ui_storage","effects","cpu_dual",
        "engines_samples","sanitizers","web_target")
REQUIRED=("stasis-target","stasis-complete-profile","compat-target",
          "stasis-layer-0","stasis-layer-1","stasis-layer-sanitized-0",
          "stasis-layer-sanitized-1","capture-diagnostics-audio-identity",
          "neutral-home-reference-audio-identity","target-budget-actual",
          "d3-exact-profile","drone-cells-strict-ubsan")

def one(root,name):
    files=list(root.rglob(name))
    if len(files)!=1: raise ValueError(f"Expected exactly one {name}, got {len(files)}")
    return json.loads(files[0].read_text())

def validate(root):
    for shard in SHARDS:
        row=one(root,shard+".json")
        if row.get("shard")!=shard or row.get("exit_code")!=0 or row.get("skips"):
            raise ValueError(f"Failed/incomplete host suite: {shard}")
    rows=one(root,"target-results.json")
    if not isinstance(rows,list) or not rows: raise ValueError("Empty target result list")
    bad=[row.get("case","unknown") for row in rows if row.get("exit_code")!=0]
    if bad: raise ValueError("Failed target cases: "+", ".join(bad))
    names={row.get("case") for row in rows}
    missing=set(REQUIRED)-names
    if missing: raise ValueError("Missing target cases: "+", ".join(sorted(missing)))
    print(f"Report gate PASS: {len(SHARDS)} host shards and {len(rows)} target cases")

def self_test():
    with tempfile.TemporaryDirectory() as folder:
        root=Path(folder)
        for shard in SHARDS:
            (root/(shard+".json")).write_text(json.dumps({"shard":shard,"exit_code":0,"skips":[]}))
        good=[{"case":name,"exit_code":0} for name in REQUIRED]
        target=root/"target-results.json"
        target.write_text(json.dumps(good)); validate(root)
        for bad in [good[:-1],[],[{**r,"exit_code":1 if r["case"]=="capture-diagnostics-audio-identity" else 0} for r in good]]:
            target.write_text(json.dumps(bad))
            try: validate(root)
            except ValueError: pass
            else: raise AssertionError("Report accepted a failed/missing target case")
        target.write_text(json.dumps(good))
        for row in [{"shard":"cpu_dual","exit_code":1,"skips":[]},
                    {"shard":"cpu_dual","exit_code":0,"skips":["missing tool"]}]:
            (root/"cpu_dual.json").write_text(json.dumps(row))
            try: validate(root)
            except ValueError: pass
            else: raise AssertionError("Report accepted a failed/skipped host shard")
        print("Report gate self-test PASS: failures, omissions and skips are rejected")

if __name__=="__main__":
    try:
        if len(sys.argv)==2 and sys.argv[1]=="--self-test": self_test()
        elif len(sys.argv)==2: validate(Path(sys.argv[1]))
        else: raise ValueError("Usage: validate_report_060.py REPORT_FOLDER | --self-test")
    except (ValueError,OSError,json.JSONDecodeError) as e:
        print("Report gate FAIL:",e,file=sys.stderr); sys.exit(1)
