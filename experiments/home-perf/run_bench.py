#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Alternating V9/candidate offline DSP timing; no FM-1 cycle conversion."""
from pathlib import Path
import argparse,json,os,platform,statistics,subprocess,time
p=argparse.ArgumentParser();p.add_argument('--reference',type=Path,required=True)
p.add_argument('--out',type=Path,required=True);p.add_argument('--runs',type=int,default=7)
p.add_argument('--seconds',type=int,default=180);p.add_argument('--cpu',type=int)
p.add_argument('--skip-build',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[2];ref=a.reference.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
if a.cpu is not None:os.sched_setaffinity(0,{a.cpu})
emcc=os.environ.get('EMCC','emcc');cc=os.environ.get('CC','gcc')
flags=['-O2','-ffp-contract=off','-w','-Ibuild/gen','-Ifirmware/src','experiments/home-perf/perf_bench.c','-lm']
# Install the identical test harness into the extracted reference source only;
# original V9 archives predate this benchmark. Firmware/DSP files stay untouched.
for name in ['perf_bench.c','captured_controls.h']:
 source=root/'experiments/home-perf'/name;dest=ref/'experiments/home-perf'/name
 if dest.exists() and dest.read_bytes()!=source.read_bytes():raise RuntimeError('Different reference harness: '+str(dest))
 if not dest.exists():dest.write_bytes(source.read_bytes())
if not a.skip_build:
 for label,r in [('v9',ref),('v10',root)]:
  for kind,compiler,extra,ext in [('native',cc,['-pipe'],''),('wasm',emcc,['-sENVIRONMENT=node','-sEXIT_RUNTIME=1','-sSTACK_SIZE=1048576'],'.js')]:
   subprocess.run([compiler,*flags,*extra,'-o',str(out/f'final-{kind}-{label}{ext}')],cwd=r,check=True)
records=[]
def write(path,obj):
 data=(json.dumps(obj,indent=2)+'\n').encode()
 with path.open('wb',buffering=0) as f:
  v=memoryview(data)
  while v:v=v[f.write(v):]
  os.fsync(f.fileno())
for kind in ['wasm','native']:
 for variant in [0,1,5]:
  for repeat in range(a.runs):
   pair=[]
   for label in (['v9','v10'] if repeat%2==0 else ['v10','v9']):
    exe=out/f'final-{kind}-{label}';cmd=([os.environ.get('NODE','node'),str(exe)+'.js'] if kind=='wasm' else [str(exe)])
    run=subprocess.run([*cmd,str(variant),str(a.seconds)],capture_output=True,text=True,check=True)
    record=json.loads(run.stdout);record.update(kind=kind,build=label,repeat=repeat);records.append(record);pair.append(record)
    print(kind,variant,repeat,label,round(record['cpu_ns']/1e9,6),flush=True)
   if pair[0]['hash']!=pair[1]['hash']:raise RuntimeError('Batch audio checksum mismatch')
   write(out/'final-measurements.json',records)
summary=[]
for kind in ['wasm','native']:
 for variant in [0,1,5]:
  old=[r['cpu_ns'] for r in records if r['kind']==kind and r['variant']==variant and r['build']=='v9']
  new=[r['cpu_ns'] for r in records if r['kind']==kind and r['variant']==variant and r['build']=='v10']
  ratios=[new[i]/old[i] for i in range(len(old))]
  summary.append(dict(kind=kind,variant=variant,median_v9_ns=statistics.median(old),median_v10_ns=statistics.median(new),reduction_percent=100*(1-statistics.median(new)/statistics.median(old)),paired_median_reduction_percent=100*(1-statistics.median(ratios)),paired_range_percent=[100*(1-max(ratios)),100*(1-min(ratios))]))
write(out/'final-summary.json',{'platform':platform.platform(),'cpu_affinity':sorted(os.sched_getaffinity(0)) if hasattr(os,'sched_getaffinity') else None,'runs':a.runs,'seconds_per_run':a.seconds,'flags':flags,'notes':'WASM wall monotonic time; native process CPU time; actual mix_block without GUI/real nested hardware interrupts. Representative 120 BPM sequence, not exact missing project replay.','results':summary})
print(json.dumps(summary,indent=2),flush=True)
