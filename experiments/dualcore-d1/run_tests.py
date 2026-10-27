#!/usr/bin/env python3
import os,subprocess,json,shlex
from pathlib import Path
r=Path(__file__).resolve().parents[2];os.chdir(r)
out=r/'build/dc1';out.mkdir(parents=True,exist_ok=True)
cases=['experiments/home-perf/pair_audio_regression_test.c','experiments/home-perf/diag_audio_test.c','experiments/dualcore-d1/three_track_test.c']
results=[]
for case in cases:
 name=Path(case).stem;outputs=[]
 for mode in ['serial','off','dual']:
  exe=out/(name+'-'+mode)
  args=['cc','-O2','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src']
  args+=shlex.split(os.environ.get('D3_TEST_CFLAGS',''))
  if mode!='serial':args+=['-pthread','-DFELUCCA_CPU2=1','-DCARAVEL_DUALCORE_D1=1','-include','tests/c1_host.h','-DC1_HOST_OFF='+str(int(mode=='off'))]
  subprocess.run([*args,case,'-lm','-o',str(exe)],check=True)
  p=subprocess.run([str(exe),'capture'],env=dict(os.environ,C1_MASK='rot'),capture_output=True,text=True,check=True)
  (out/(name+'-'+mode+'.log')).write_text(p.stdout+p.stderr);outputs.append(p.stdout)
 assert outputs[0]==outputs[1]==outputs[2],name+' audio mismatch'
 results.append({'case':case,'all_three_identical':True,'output_lines':len(outputs[0].splitlines())})
 print(name,'identical: D3 serial / D3 OFF / D3 threaded',flush=True)
(out/'audio-results.json').write_text(json.dumps(results,indent=2)+'\n')

