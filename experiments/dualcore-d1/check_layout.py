#!/usr/bin/env python3
"""Validate D1/D2 stack placement in the real linked ELF, including retained RAM reserves."""
import os,re,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[2]
tool=Path(os.environ['JIELI_TOOLCHAIN'])/'common/bin/objdump'
s=subprocess.check_output([str(tool),'-h',str(r/'build/felucca.elf')],text=True)
a={m[1]:(int(m[2],16),int(m[3],16),m[4]) for m in re.finditer(r'^\s*\d+\s+(\.\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+(.*)$',s,re.M)}
z,base,kind=a['.c1_stack'];assert z==4352 and kind=='BSS'
assert base>=a['.c1_text'][1]+a['.c1_text'][0] and base+z<=0x1c06000
assert a['.data'][0]+a['.bss'][0]<=73728-4096
assert a['.pool'][0]<=368640-8192
assert a['.noinit'][0]<=0x3d50
print(s)
print('Dual-core layout: disjoint code/stack, NOLOAD worker stacks, unchanged data/pool reserves OK')
