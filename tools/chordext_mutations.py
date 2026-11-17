#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Mutation controls on disposable trees; a compile failure is never a kill.
M3 commits the live window after the follower input; M5 removes source/follower cycle rejection.
Both use the real live-source tests; no compile error counts as detection.
M8 removes stale-toggle clearing; T30 supplies a destination with a stale mark.
"""
import concurrent.futures,json,os,re,shlex,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/validation/chordext-mutants'
def patch(root,m):
 def sub(path,old,new):
  p=root/'firmware/src'/path;s=p.read_text();assert old in s,(m,path,old);p.write_text(s.replace(old,new,1))
 if m=='M1':sub('midi_control.c','            n = m->n;\n            for (i = 0; i < n; i++)\n                nn[i] = m->note[i];\n            m->id = 0;','            n = chord_build(&trk[id - 1u], note, nn);\n            m->id = 0;')
 elif m=='M2':sub('chord.c','static void chord_stop_all(void)\n{\n    uint32_t i, j;','static void chord_stop_all(void)\n{\n    uint32_t i, j;\n    return;')
 elif m=='M3':
  sub('seq.c','    ext_source_tick();\n    keyboard_block();','    keyboard_block();')
  sub('seq.c','    if (clock_mode) {\n        seq_n = 0;','    ext_source_tick();\n    if (clock_mode) {\n        seq_n = 0;')
 elif m=='M4':sub('chord.c','    n += 12 * clamp((int32_t)c->oct, -2, 2);\n    while (n < 0)\n        n += 12;\n    while (n > 127)\n        n -= 12;\n    return n;','    return n + 12 * (int32_t)c->oct;')
 elif m=='M5':sub('chord_source.c','if (trk[source].p[P_CSRC]) return EXT_WHY_SRCEXT;','if (0) return EXT_WHY_SRCEXT;')
 elif m=='M6':sub('ui.c','i == P_SQNT ||\n           CHORDSET_ID(i);','i == P_SQNT;')
 elif m=='M7':sub('seq.c','for (k = 0; k < no; k++) {                                /* rewrite the owners first: they are the release identity */','for (k = 0; k < 0; k++) {')
 elif m=='M8':
  p=root/'firmware/src/seq.c';s=p.read_text();a=s.index('static __attribute__((noinline)) void retrigger_follow');b=s.index('\n#else',a)
  part,n=re.subn(r'lat_put\(lat_eat\[i\], [^;]+;', '(void)0;',s[a:b]);assert n>=4;p.write_text(s[:a]+part+s[b:])
 elif m=='M9':sub('seq.c','static int ret_same(const uint8_t *a, uint32_t na, const uint8_t *b, uint32_t nb)\n{\n    uint32_t i;','static int ret_same(const uint8_t *a, uint32_t na, const uint8_t *b, uint32_t nb)\n{\n    uint32_t i;\n    return 0;')
 elif m=='M10':sub('chord.c','    if (t->p[P_AMODE])\n        return;                                        /* round 7:','    if (0)\n        return;                                        /* round 7:')
 elif m=='M11':sub('project_format2.c','memcpy(h->link_retrig,b+13,4);','memset(h->link_retrig,0,4);')
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 def one(m):
  source='chordsource' if m in ['M3','M5','control-source'] else 'chordset' if m in ['M6','M11'] else 'chordext'
  with tempfile.TemporaryDirectory(prefix=m+'-',dir=OUT) as tmp:
   root=Path(tmp)
   for rel in ['firmware','tests','experiments','web/emu']:shutil.copytree(ROOT/rel,root/rel,ignore=shutil.ignore_patterns('.git','__pycache__'))
   (root/'build').mkdir();(root/'build/gen').symlink_to(ROOT/'build/gen',target_is_directory=True)
   if not m.startswith('control-'):patch(root,m)
   if m=='control-settings':source='chordset'
   exe=root/'test';logpath=OUT/(m+'.log')
   with logpath.open('w') as log:
    flags=['-std=gnu11','-O1','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src','-Itests']
    p=subprocess.run([*shlex.split(os.environ.get('CC','cc')),*flags,f'tests/{source}_test.c','-lm','-o',str(exe)],cwd=root,stdout=log,stderr=subprocess.STDOUT,timeout=180)
    if p.returncode:return {'mutation':m,'compile_exit':p.returncode,'detected':False}
    p=subprocess.run([str(exe)],cwd=root,stdout=log,stderr=subprocess.STDOUT,timeout=120)
   control=m.startswith('control-');detected=p.returncode!=0 and not control
   return {'mutation':m,'compile_exit':0,'test_exit':p.returncode,'detected':detected,'control_pass':p.returncode==0 if control else None}

 controls=[one('control-engine'),one('control-settings'),one('control-source')]
 assert all(r['control_pass'] for r in controls),controls
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:rows=list(pool.map(one,['M'+str(i) for i in range(1,12)]))
 (OUT/'results.json').write_text(json.dumps({'controls':controls,'mutants':rows},indent=2)+'\n')
 for r in rows:print(json.dumps(r),flush=True)
 assert all(r['detected'] for r in rows),'Surviving or uncompilable mutation'
 print('CHORD EXT mutations: M1-M11 detected after successful compilation; positive controls passed',flush=True)
 return 0
if __name__=='__main__':raise SystemExit(main())
