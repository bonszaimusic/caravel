#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Required CHORD EXT A/B/D fuzz: real FORMAT 2 emulator, 141 seeds each, CPU2 compiled out / in.
CHORDEXT_FUZZ_SMOKE=1 (or --smoke): 30 seeds per profile, for the quick gate; the full 141 stay the release candidate's gate (CHORDEXT_FUZZ_SEEDS=n sets any number).
CHORDEXT_FUZZ_WORKERS: parallel runs (default: the CPUs, at most 12; it was 6)."""
import concurrent.futures, json, os, shlex, subprocess, sys, time
from pathlib import Path
from validate_format2 import PROFILE, positive_env
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/validation/chordext-fuzz'
def bounded_map(function, items, workers, budget=None):
 # Keep result order, but replenish only the currently available worker slots.
 # The local scheduler may return CPU slots after the long TSAN shard exits.
 rows=[None]*len(items);next_item=0;pending={}
 with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
  while pending or next_item<len(items):
   limit=workers
   if budget:
    value=int(Path(budget).read_text().strip())
    if value<1:raise ValueError('Fuzz worker budget must be positive')
    limit=min(workers,value)
   while next_item<len(items) and len(pending)<limit:
    pending[pool.submit(function,items[next_item])]=next_item;next_item+=1
   done,_=concurrent.futures.wait(pending,return_when=concurrent.futures.FIRST_COMPLETED)
   for future in done:rows[pending.pop(future)]=future.result()
 return rows

def main():
 OUT.mkdir(parents=True,exist_ok=True);exes={c:OUT/f'fuzz-cpu2-{c}' for c in [0,1]}
 seeds=positive_env('CHORDEXT_FUZZ_SEEDS',30 if (os.environ.get('CHORDEXT_FUZZ_SMOKE')=='1' or '--smoke' in sys.argv) else 141)
 workers=positive_env('CHORDEXT_FUZZ_WORKERS',max(1,min(12,os.cpu_count() or 1)))
 flags=['-std=gnu11','-O2','-g','-w','-ffp-contract=off','-fwrapv','-fsanitize=address,undefined','-fno-sanitize-recover=undefined','-fno-omit-frame-pointer','-Ibuild/gen','-Ifirmware/src','-Itests',*PROFILE,'-DCARAVEL_UX_CAPTURE=1']
 def build(item):                                       # the two variants compile at the same time (about 50 s each, one core each)
  cpu,exe=item
  with (OUT/f'compile-cpu2-{cpu}.log').open('w') as log:
   subprocess.run([*shlex.split(os.environ.get('CC','cc')),*flags,f'-DFELUCCA_CPU2={cpu}',f'-DCARAVEL_DUALCORE_D1={cpu}','experiments/action-banks/chordext_fuzz.c','-lm','-o',str(exe)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
 with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(build,list(exes.items())))
 def one(item):
  cpu,profile,seed=item;exe=exes[cpu];name=f'{cpu}-{profile}-{seed}';start=time.monotonic();rc=124
  with (OUT/(name+'.log')).open('w') as log:
   try:
    p=subprocess.run([str(exe),str(seed),'4000','150','1',str(profile)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=120,env={**os.environ,'RV_STOP':'1','ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
    rc=p.returncode
   except subprocess.TimeoutExpired:log.write('\nTIMEOUT after 120 seconds\n')
  return {'cpu2_compiled':cpu,'profile':profile,'seed':seed,'iterations':4000,'exit_code':rc,'seconds':round(time.monotonic()-start,2),'log':name+'.log'}
 start_all=time.monotonic()
 rows=bounded_map(one,[(c,p,s) for c in [0,1] for p in [2,3,4] for s in range(1,seeds+1)],workers,os.environ.get('CHORDEXT_FUZZ_BUDGET_FILE'))
 (OUT/'results.json').write_text(json.dumps({'profiles':{'2':'A','3':'D','4':'B'},'seeds_per_profile':seeds,'rows':rows},indent=2)+'\n')
 bad=[r for r in rows if r['exit_code']]
 for r in bad:print('FAIL '+json.dumps(r),flush=True)
 print(f'CHORD EXT fuzz: {len(rows)-len(bad)}/{len(rows)} passed; A, B and D, CPU2 out/in, {seeds} seeds x 4000 iterations, ASan/UBSan, CAPTURE enabled (no worker thread in this emulator); {workers} at a time, {time.monotonic()-start_all:.0f} s'+('' if seeds>=141 else ' (SMOKE: the release candidate runs all 141)'),flush=True)
 return bool(bad)
if __name__=='__main__':raise SystemExit(main())
