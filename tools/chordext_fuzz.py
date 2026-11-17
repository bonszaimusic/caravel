#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Required CHORD EXT A/B/D fuzz: real FORMAT 2 emulator, 141 seeds each, CPU2 compiled out / in."""
import concurrent.futures, json, os, shlex, subprocess, time
from pathlib import Path
from validate_format2 import PROFILE
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/validation/chordext-fuzz'
def main():
 OUT.mkdir(parents=True,exist_ok=True);exes={c:OUT/f'fuzz-cpu2-{c}' for c in [0,1]}
 flags=['-std=gnu11','-O2','-g','-w','-ffp-contract=off','-fwrapv','-fsanitize=address,undefined','-fno-sanitize-recover=undefined','-fno-omit-frame-pointer','-Ibuild/gen','-Ifirmware/src','-Itests',*PROFILE,'-DCARAVEL_UX_CAPTURE=1']
 for cpu,exe in exes.items():
  with (OUT/f'compile-cpu2-{cpu}.log').open('w') as log:
   subprocess.run([*shlex.split(os.environ.get('CC','cc')),*flags,f'-DFELUCCA_CPU2={cpu}',f'-DCARAVEL_DUALCORE_D1={cpu}','experiments/action-banks/chordext_fuzz.c','-lm','-o',str(exe)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
 def one(item):
  cpu,profile,seed=item;exe=exes[cpu];name=f'{cpu}-{profile}-{seed}';start=time.monotonic();rc=124
  with (OUT/(name+'.log')).open('w') as log:
   try:
    p=subprocess.run([str(exe),str(seed),'4000','150','1',str(profile)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=120,env={**os.environ,'RV_STOP':'1','ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
    rc=p.returncode
   except subprocess.TimeoutExpired:log.write('\nTIMEOUT after 120 seconds\n')
  return {'cpu2_compiled':cpu,'profile':profile,'seed':seed,'iterations':4000,'exit_code':rc,'seconds':round(time.monotonic()-start,2),'log':name+'.log'}
 with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
  rows=list(pool.map(one,[(c,p,s) for c in [0,1] for p in [2,3,4] for s in range(1,142)]))
 (OUT/'results.json').write_text(json.dumps({'profiles':{'2':'A','3':'D','4':'B'},'seeds_per_profile':141,'rows':rows},indent=2)+'\n')
 bad=[r for r in rows if r['exit_code']]
 for r in bad:print('FAIL '+json.dumps(r),flush=True)
 print(f'CHORD EXT fuzz: {len(rows)-len(bad)}/846 passed; A, B and D, CPU2 out/in, 141 seeds x 4000 iterations, ASan/UBSan, CAPTURE enabled (no worker thread in this emulator)',flush=True)
 return bool(bad)
if __name__=='__main__':raise SystemExit(main())
