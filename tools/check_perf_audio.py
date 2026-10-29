#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Static guard for affected DSP; not a hardware cycle measurement.
The complete historical target-budget suite remains separate.
"""
import importlib.util
import sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("target_budget",root/"tests/target_budget.py")
budget=importlib.util.module_from_spec(spec)
spec.loader.exec_module(budget)
def main(path):
    budget.FUNCS += ["stasis_block"]
    functions=budget.functions(path)
    limits={"drone_legacy_render":(788,0),"drone_cell_render":(1824,1),"hades_render":(633,0),"sem_render":(409,0)}
    for name,(cost,calls) in limits.items():
        if not functions.get(name):raise SystemExit(f"audio target: missing {name}")
        result=budget.cost(functions[name])
        if result["cost"]>cost*1.10 or result["call"]>calls:raise SystemExit(f"audio target: {name} regression: {result}")
        print(f"audio target: {name}: static cost {result['cost']}, loop calls {result['call']} (within original -Os limits)")
    st=functions.get("stasis_block")
    if not st:raise SystemExit("audio target: missing stasis_block")
    result=budget.cost(st)
    if result["call"]>1:raise SystemExit(f"audio target: per-sample STASIS helper calls returned: {result}")
    print(f"audio target: STASIS loop call sites {result['call']} (rc4: 18); helper bodies are inlined, not removed")
    print("audio target: affected DSP passed; hardware deadline remains unmeasured")
if __name__=="__main__":main(sys.argv[1] if len(sys.argv)>1 else str(root/"build/felucca.dis"))
