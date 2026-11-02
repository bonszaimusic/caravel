#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Static look at the firmware disassembly written by tools/build.py (build/**/felucca.dis): which function holds the transport counter
(perf_bars_q16, seq.c), how many instructions it has and how many of them divide. Instruction counts, not timing."""
import glob,re,sys
files=glob.glob('build/**/felucca.dis',recursive=True)
if not files:sys.exit('no felucca.dis under build/')
lines=open(files[0]).read().split('\n')
sym=None
for l in lines:
    m=re.match(r'^([0-9a-f]+) .*\bperf_bars_q16\S*\s*$',l)
    if m:sym=m.group(1);break
print('disassembly',files[0],'symbol address',sym)
if not sym:
    for l in lines:
        if 'perf_bars' in l or 'perf_starts' in l:print('candidate:',l[:160])
    sys.exit(0)
pat=sym.lstrip('0')
cur=None;funcs={}
for l in lines:
    m=re.match(r'^[0-9a-f]+ <(.+)>:',l)
    if m:cur=m.group(1);funcs.setdefault(cur,[0,0,0]);continue
    if cur and re.match(r'^\s+[0-9a-f]+:',l):
        funcs[cur][0]+=1
        if re.search(r'div|udiv|umod',l):funcs[cur][1]+=1
        if pat in l:funcs[cur][2]+=1
for k,v in funcs.items():
    if v[2]:print('function',k,'instructions',v[0],'divide-like lines',v[1],'references to the counter',v[2])
for k in funcs:
    if re.search(r'udiv|umod',k):print('library',k,funcs[k][0],'instructions')
