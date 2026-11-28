#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""FORMAT 2 validation registry. Every active root test is accounted for.
Historical tests tied to removed engines/import formats are listed explicitly,
with their replacement coverage, in tests/format2_legacy_tests.json. Baseline
files are never rewritten. A failed command or missing tool fails its shard.
"""
import argparse, concurrent.futures, gzip, hashlib, json, os, re, shlex, subprocess, sys, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/validation'
SHARDS=['storage_usb','synthesis_midi','ui_storage','effects','cpu_dual','engines_samples','sanitizers','web_target']
GROUPS={
 'storage_usb':'storage input midi_uart timer5_midi usb_update',
 'synthesis_midi':'scale track_mute chord chordext chordext_diff chordext_capture chordsource ux_capture ux_play_midi latch midi_control midi_resync midi_drift midi_clock_cc voice_reserve ratchet step15 seq155 detail step_play click swing',
 'ui_storage':'evolve_save stasis_project_menu_ui ux_elastic_arc harmony_build_display harmony_roles_ui track_role_ui chordset chordext_ui release_boot format2_project format2_settings format2_performance format2_editor format2_backup format2_presets persistence live_pattern live_full duplicate motion ui155 v156 v16 enum_order theme text_ref',
 'effects':'afterburner_host afterburner_ui speaker audio mod sound14 sound15 insert trackfilter slicer perform delay_ext fx_motion fx_edit fx_feedback lfo2 lfo2_editor stasis stasis_layer stasis_continuity stasis_region_math stasis_region_wire stasis_v5_stage1 stasis_region_release stasis_ui_new stasis_scope_rate stasis_scope_policy stasis_scope_geometry stasis_join stasis_join_lut stasis_join_ring stasis_mix_dry stasis_send stasis_hall_feed stasis_level stasis_grain_wander stasis_join_lut_fx',
 'cpu_dual':'dualcore',
 'engines_samples':'format2_engine_memory phys sem hades hades_dsp drone drone_cells drone_motion',
 'sanitizers':'', 'web_target':''}
GROUPS={k:v.split() for k,v in GROUPS.items()}
PROFILE=['-DFELUCCA_HOME_PERF=1','-DFELUCCA_PERF_HW=1','-DFELUCCA_PAIR_MORPH=1','-DCARAVEL_ACTION_BANKS=1','-DFELUCCA_RANDOM_MORPH=1','-DCARAVEL_UX_LAYERS=1','-DCARAVEL_UX_EVOLVE=1','-DCARAVEL_ELASTIC_P1=1','-DCARAVEL_ELASTIC_PLAYABLE=1','-DCARAVEL_REVERB_MINIMAL=1','-DCARAVEL_STASIS_EXPAND=1']
ELASTIC=['core','overlay','p1_shim','row','visual','trajectory','p13','sync']
SANITIZED=['afterburner_host','evolve_save','format2_project','format2_settings','format2_performance','format2_editor','format2_backup','format2_presets','persistence','duplicate','fx_edit','live_full','midi_resync','midi_drift','voice_reserve','lfo2','lfo2_editor','stasis_region_math','stasis_region_wire','ratchet','enum_order','fx_feedback','chordext','chordset','chordext_ui','harmony_roles_ui','chordext_capture','release_boot','chordsource','ux_capture','ux_play_midi']
BASELINES=['tests/golden.txt','tests/cpu_baseline.txt','tests/cpu_format2_idle.txt','tests/target_budget.txt','tests/union_golden.txt','tests/chordext_baseline.txt','tests/chordext_format2_off.txt','tests/chordext_format2_scale.txt','tests/chordext_format2_harm.txt']
EXTERNAL={'ux_render':'tests/ux_render.sh (visual session)',
 'stasis_join_audition':'tools/stasis2_join_audition.sh (a WAV generator with arguments, with tools/stasis2_join_audio_compare.py)',
 'capture_post_quant':'tests/capture_post_quant.sh: OPEN, does not link yet (hp_project_* from experiments/home-perf/home_store*.c are not in the host fixture)'}

def inventory():
 legacy=json.loads((ROOT/'tests/format2_legacy_tests.json').read_text())
 registered={f'tests/{n}_test.c' for v in GROUPS.values() for n in v}
 registered|={f'tests/{n}_test.c' for n in ['uac','usb_desc','ota','ldr',*SANITIZED]}
 registered|={f'tests/home_elastic_{n}_test.c' for n in ELASTIC}
 registered|={f'tests/{n}_test.c' for n in EXTERNAL}
 actual={str(p.relative_to(ROOT)) for p in (ROOT/'tests').glob('*_test.c')}
 missing=actual-registered-set(legacy)
 stale=set(legacy)-actual
 overlap=registered&set(legacy)
 if missing or stale or overlap:raise RuntimeError(f'Inventory mismatch: uncovered={sorted(missing)}, stale={sorted(stale)}, overlap={sorted(overlap)}')
 data={'profile':'FORMAT 2, six engines, protocol 18, complete HPM2 v6', 'shards':GROUPS,'elastic':ELASTIC,'sanitizers':SANITIZED,'historical':legacy,'external':EXTERNAL,'uncovered_root_tests':[]}
 OUT.mkdir(parents=True,exist_ok=True);(OUT/'host-inventory.json').write_text(json.dumps(data,indent=2)+'\n')
 return data

def positive_env(name, default):
 value=int(os.environ.get(name, default))
 if value < 1:raise ValueError(name+' must be a positive integer')
 return value

class Runner:
 def __init__(self,shard):
  self.shard=shard;self.rows=[];self.skips=[];self.cc=shlex.split(os.environ.get('CC','cc'));self.bin=OUT/shard;self.bin.mkdir(parents=True,exist_ok=True)
  self.plan=False;self.jobs=[];self.compiled=None;self.idx=0;self.started=time.monotonic()
 def execute(self,name,args,env=None,output=None,timeout=600,log_id=None):
  log=self.bin/(str(log_id if log_id is not None else len(self.rows))+'-'+re.sub('[^A-Za-z0-9_-]','_',name)+'.log');start=time.monotonic()
  with log.open('w') as f:
   try:
    if output:
     with Path(output).open('w') as data:p=subprocess.run(list(map(str,args)),cwd=ROOT,env={**os.environ,**(env or {})},stdout=data,stderr=f,timeout=timeout)
    else:p=subprocess.run(list(map(str,args)),cwd=ROOT,env={**os.environ,**(env or {})},stdout=f,stderr=subprocess.STDOUT,timeout=timeout)
    rc=p.returncode
   except subprocess.TimeoutExpired:
    f.write('\nTIMEOUT after '+str(timeout)+' seconds\n');rc=124
   except OSError as e:
    f.write(str(e)+'\n');rc=127
  return {'case':name,'exit_code':rc,'seconds':round(time.monotonic()-start,2),'log':str(log.relative_to(ROOT))}
 def record(self,row):
  print('== '+row['case'],flush=True);self.rows.append(row)
  log=ROOT/row['log'];contents=log.read_text(errors='replace')
  if row['exit_code']:
   print(contents[-12000:],flush=True);raise RuntimeError(f"{row['case']}: exit {row['exit_code']}")
  if 'no instruction counter on this host' in contents:self.skips.append(row['case']+': CPU instruction budget unmeasured on this host')
  tail=contents.splitlines()[-2:]
  if tail:print('\n'.join(tail),flush=True)
 def command(self,name,args,env=None,output=None,timeout=600):
  if not self.plan:self.record(self.execute(name,args,env,output,timeout))
 def compile_all(self,jobs):
  def one(item):
   idx,(name,cmd)=item
   return cmd,self.execute(name,cmd,log_id='compile-'+str(idx))
  with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
   self.compiled=list(pool.map(one,enumerate(self.jobs)))
 def c(self,n,flags=(),args=(),env=None,source=None):
  idx=self.idx;self.idx+=1
  exe=self.bin/(n+'-'+str(idx));source=source or f'tests/{n}_test.c'
  cmd=[*self.cc,'-std=gnu11','-O2','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src','-Itests',*flags,source,'-lm','-o',exe]
  if self.plan:self.jobs.append((n+'-compile',cmd));return exe
  if self.compiled is None:self.command(n+'-compile',cmd)
  else:
   planned,row=self.compiled[idx]
   if cmd!=planned:raise RuntimeError('Compile plan changed at '+n)
   self.record(row)
  self.command(n,[exe,*args],env)
  return exe
 def results(self,rc):
  slow=sorted(self.rows,key=lambda x:-x['seconds'])[:8]
  elapsed=round(time.monotonic()-self.started,2)
  print('slowest steps: '+', '.join(f"{x['case']} {x['seconds']:.0f}s" for x in slow)+f'; shard wall time {elapsed:.1f}s',flush=True)
  result={'shard':self.shard,'exit_code':rc,'profile':'FORMAT 2','checks':self.rows,'skips':self.skips,'wall_seconds':elapsed}
  (OUT/(self.shard+'.json')).write_text(json.dumps(result,indent=2)+'\n')


def run(r):
 s=r.shard
 if s=='effects' and not r.plan:
  raw=gzip.decompress((ROOT/'tests/fixtures/speaker-kicks.f32.gz').read_bytes())
  assert hashlib.sha256(raw).hexdigest()=="9f063cf8d56c0dd378be351b813f478c18bfe877e3c535143b69c65c1fdff31d"
  folder=ROOT/'build/fixtures';folder.mkdir(exist_ok=True);(folder/'speaker-kicks.f32').write_bytes(raw)
 for n in GROUPS[s]:
  flags=[]
  if n in ['chordsource','ux_capture','ux_play_midi','ux_elastic_arc','afterburner_ui']:flags+=PROFILE
  if n in ['dualcore']:flags+=['-pthread']
  if n in ['stasis','stasis_continuity','stasis_v5_stage1']:flags+=['-DCARAVEL_REVERB_MINIMAL=1','-DCARAVEL_STASIS_EXPAND=1']
  if n=='stasis_v5_stage1':flags+=['-DCARAVEL_STASIS2=1']
  if n in ['stasis_mix_dry','stasis_send','stasis_hall_feed','stasis_level','stasis_grain_wander','stasis_join_lut_fx']:
   # STASIS 2.0 host DSP tests: the real fx.c with minimal reverb and the expanded capture
   flags+=['-DCARAVEL_REVERB_MINIMAL=1','-DCARAVEL_STASIS_EXPAND=1','-DFELUCCA_SLICE=0','-DCARAVEL_STASIS2=1']
  if n=='stasis_join_lut_fx':flags+=['-DCARAVEL_STASIS_SOURCE_JOIN_EXPERIMENT=1','-DCARAVEL_STASIS_SOURCE_JOIN_LUT_EXPERIMENT=1']
  if n=='stasis_project_menu_ui':flags += [*PROFILE,'-DCARAVEL_STASIS2=1','-DCARAVEL_AFTERBURNER=1','-DCARAVEL_UX_CAPTURE=1','-DCARAVEL_STASIS_SCOPE_V6=1','-DCARAVEL_UX_BUILD_FX_PLAY_STYLE=1']
  if n=='stasis_layer':flags+=PROFILE
  if n=='phys':args=[str(r.bin/'phys-demo')];Path(args[0]).mkdir(exist_ok=True)
  else:args=[]
  exe=r.c(n,flags,args)
  if n=='chordext_diff':
   r.command('chordext-diff-harm',[exe,'check'],{'CE_F2_HARM':'1'})
   r.command('chordext-diff-scale',[exe,'check'],{'CE_F2_RETRIG':'2'})
   r.command('chordext-diff-negative-control',[sys.executable,'tools/chordext_negative.py',exe])
 if s=='ui_storage':
  # This is the real FORMAT-2 codec with opt-in A6 REGION marker; normal
  # format2_project continues to verify original 0x00/0xA5 wire semantics.
  r.c('format2_project_stasis2',
      ['-DCARAVEL_FORMAT2=1','-DCARAVEL_REVERB_MINIMAL=1',
       '-DCARAVEL_STASIS_EXPAND=1','-DCARAVEL_STASIS2=1'],
      source='tests/format2_project_test.c')
  r.c('format2_backup_stasis2',['-DCARAVEL_STASIS2=1'],source='tests/format2_backup_test.c')
 if s=='storage_usb':
  r.c('snapshot_persistence',source='experiments/home-perf/snapshot_persistence_test.c')
  for t in ELASTIC:r.c('home_elastic_'+t,['-O1','-Wall','-Wextra','-Werror','-Wno-unused-function'])
  half=re.search(r'^#define HALF_FRAMES (\d+)',(ROOT/'firmware/src/core.h').read_text(),re.M).group(1)
  for cdc in [0,1]:r.c('uac',[f'-DHALF_FRAMES={half}',f'-DT_CDC={cdc}'])
  modes='1.1.0.1.1 1.1.1.1.1 1.1.2.1.1 1.1.3.1.1 1.1.0.0.1 1.1.2.0.1 0.1.0.1.1 1.1.0.1.0 1.1.0.0.0 1.1.2.1.0 0.1.0.1.0 1.0.0.1.1 1.0.1.1.1 1.0.0.0.1 0.0.0.1.1'.split()
  for mode in modes:
   r.c('usb_desc',[f'-DT_{k}={v}' for k,v in zip(['CDC','UAC','LAYOUT','ON','48K'],mode.split('.'))])
  for mode in ['1.1.0.1.1','1.1.0.0.1','1.1.0.1.0','1.1.0.0.0','0.1.0.1.1']:
   r.c('usb_desc',['-DT_ASF=0',*[f'-DT_{k}={v}' for k,v in zip(['CDC','UAC','LAYOUT','ON','48K'],mode.split('.'))]])
  r.c('timer5_midi',['-DFELUCCA_UART=0'])
  r.c('ota',['-DOWN_PKG=1'],['build/caravel.fwsc'])
  old=r.bin/'old_app.bin';
  if not r.plan:old.write_bytes((ROOT/'build/felucca.bin').read_bytes()[:200000])
  pkg=r.bin/'old.fwsc'
  r.command('loader-old-package',[sys.executable,'tools/fm1pkg_make.py',old,'build/loader/ota.bin',pkg])
  r.c('ldr',args=[pkg,'build/caravel.fwsc'])
  r.command('package-artifact-fixtures',[sys.executable,'-B','tests/release_artifacts_test.py'])
 if s=='cpu_dual':
  r.c('regress',source='tests/regress.c',args=['tests/golden.txt','tests/cpu_baseline.txt'])
  r.c('regress-c1',['-DC1_HOST','-pthread'],source='tests/regress.c',args=['tests/golden.txt','tests/cpu_baseline.txt'])
  r.c('dualcore',['-pthread'],env={'LFO2_TEST':'1','SCENE':'Custom HADES'})
  for n in ['drone','drone_cells']:r.c(n,['-DC1_HOST','-pthread'])
  if os.environ.get('NO_TSAN')=='1':
   r.skips.append('ThreadSanitizer: unavailable under Apple Silicon x86 emulation; native CI remains required')
  elif os.environ.get('TSAN')=='1':
   for n in ['dualcore','drone','drone_cells']:
    r.c(n,['-O1','-g','-fsanitize=thread','-pthread',*(['-DC1_HOST'] if n!='dualcore' else [])],env={'TSAN_OPTIONS':'halt_on_error=1'})
  else:r.skips.append('ThreadSanitizer not requested; set TSAN=1 on a supported native Linux host')
 if s=='engines_samples':
  d=Path(os.environ.get('DAISYSP','vendor/DaisySP'))/'Source'
  if not (d/'PhysicalModeling').is_dir():raise RuntimeError('Pinned DaisySP reference is required')
  obj=r.bin/'phys_fixed.o';exe=r.bin/'phys_ref'
  r.command('phys-fixed-compile',[*r.cc,'-O2','-w','-Ibuild/gen','-Itests','-c','tests/phys_fixed.c','-o',obj])
  sources=['PhysicalModeling/modalvoice.cpp','PhysicalModeling/resonator.cpp','PhysicalModeling/stringvoice.cpp','PhysicalModeling/KarplusString.cpp','Filters/svf.cpp','Utility/dcblock.cpp','Dynamics/crossfade.cpp']
  r.command('phys-float-reference-compile',['c++','-O2','-std=c++14','-w','-I'+str(d),'-I'+str(d/'Utility'),'tests/phys_ref.cpp',*[d/f for f in sources],obj,'-o',exe])
  r.command('phys-independent-float-reference',[exe])
 if s=='sanitizers':
  strict=['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=undefined','-fno-omit-frame-pointer']
  r.c('harmony_build_display',strict,env={'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
  r.c('afterburner_ui',[*strict,*PROFILE],env={'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
  for n in SANITIZED:r.c(n,[*strict,*(PROFILE if n in ['chordsource','ux_capture','ux_play_midi'] else [])],env={'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
  for n,iters,seed in [('fuzz_ed','20000','7'),('fuzz_proj','5000','13')]:r.c(n,strict,[iters,seed],source=f'tests/{n}.c',env={'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
  r.c('timer5_midi',[*strict,'-DTIMER5_SYNTH_TEST=1'])
  r.command('chordext-fuzz-A-B-D',[sys.executable,'tools/chordext_fuzz.py'],timeout=5400)
  r.command('chordext-mutations-M1-M11',[sys.executable,'tools/chordext_mutations.py'],timeout=1200)
 if s=='web_target':
  r.command('target-budget',[sys.executable,'tests/target_budget.py','build/felucca.dis','tests/target_budget.txt'],{'CARAVEL_REVERB_MINIMAL':'1','FELUCCA_SLICE':'0'})
  r.command('installer-cli',[sys.executable,'tests/install_test.py'])
  r.command('generation-protocol-backup',['node','web/generation_test.mjs'])
  r.command('generation-browser-storage',['node','web/generation_storage_test.mjs'])
  r.command('backup-web',['node','web/test_backup.mjs'])
  r.command('breaking-installer-guard',['node','web/test_breaking_guard.mjs'])
  r.command('breaking-installer-behavior',['node','web/test_breaking_behavior.mjs'])
  r.command('editor-chordext-preserve',['node','web/test_web.mjs'])
  r.command('wasm-build',['sh','web/emu/build.sh'],{'EMU_CFLAGS':'-DWEB_LCD_TRACE=1'})
  r.command('format2-wasm-old-data-guard',['node','web/emu/format2_test.mjs','build/emu/felucca.wasm'])
  r.command('afterburner-wasm',['node','web/emu/afterburner_test.mjs','build/emu/felucca.wasm'])
  r.command('ux-layers-119',['node','web/emu/ux_layers_test.mjs'])
  r.command('frame-flicker',['node','web/emu/frame_flicker_test.mjs'])
  r.command('fx-filter-track-retention',['node','web/emu/fx_filter_track_test.mjs','build/emu/felucca.wasm'])
  r.command('capture-dense-four-loops',['node','web/emu/capture_load_test.mjs'])
  r.command('tutorial-source-links',[sys.executable,'web/tutorial_pages.py','--check-source'])
  r.command('tutorial-generated-index',[sys.executable,'web/tutorial_pages.py','--check-readme'])


def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('shard',choices=SHARDS+['inventory','report']);a=ap.parse_args()
 data=inventory()
 if a.shard=='inventory':print(json.dumps(data,indent=2));return 0
 if a.shard=='report':
  rows=[json.loads((OUT/(s+'.json')).read_text()) for s in SHARDS]
  assert all(row['exit_code']==0 and row['checks'] for row in rows)
  print(f"FORMAT 2: {len(rows)} shards passed; limitations: "+json.dumps([row['skips'] for row in rows if row['skips']]))
  return 0
 for f in ['build/gen/felucca_tables.h','build/felucca.bin','build/caravel.fwsc','build/loader/ota.bin']:
  if not (ROOT/f).is_file():raise RuntimeError('Missing target input: '+f)
 before={f:hashlib.sha256((ROOT/f).read_bytes()).hexdigest() for f in BASELINES}
 r=Runner(a.shard);rc=1
 try:
  jobs=positive_env('VALIDATE_JOBS',max(1,min(8,os.cpu_count() or 1)))
  if jobs>1:
   r.plan=True;run(r);r.plan=False;r.skips.clear();r.compile_all(jobs);r.idx=0
  run(r)
  if r.compiled is not None:assert r.idx==len(r.compiled),'Compile plan was not consumed'
  assert before=={f:hashlib.sha256((ROOT/f).read_bytes()).hexdigest() for f in BASELINES},'Baseline files changed during validation'
  rc=0
 finally:r.results(rc)
 return rc
if __name__=='__main__':sys.exit(main())
