#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Offline self-test of tools/caravel_backup.py against a stand-in for the FORMAT 2 firmware's backup replies
(editor_backup.c: LIST version 2 with ids 0..7, GET with 7-bit packing). The project object is a real capsule
(pass its path: the firmware's own capture, 6144 bytes), the rest are empty or a minimal bank/settings object.
  python3 tools/caravel_backup_f2_selftest.py tests/format2_project_capsule.bin
"""
import struct,sys,tempfile,zlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import caravel_backup as cb

cap=Path(sys.argv[1]).read_bytes()
bank=struct.pack("<IHH",0x32425055,192,16)+bytes(16*192)
OBJ={0:cap,1:bytes(range(1,200))*2,2:cap,3:b"",4:b"",5:b"",6:bank,7:b""}

class Fake:
    def __init__(s):s.out=[]
    def drain(s):s.out=[]
    def send(s,m):
        body=bytes(m[len(cb.HDR):-1]);cmd=body[0];a=body[1:]
        if cmd==65:
            r=bytes([2,0,len(OBJ)])+b"".join(bytes([k])+cb.u32(len(v))+cb.u32(zlib.crc32(v)) for k,v in OBJ.items())
        elif cmd==66:
            k=a[0];off=cb.r32(a[1:6]);n=a[6]|a[7]<<7;v=OBJ[k]
            r=bytes([k,0])+cb.u32(off)+bytes([n&127,n>>7])+cb.pack(v[off:off+n])
        else:r=b""
        s.out.append(cb.HDR+bytes([cmd])+r+b"\xF7");return True
    def read(s,t):return s.out.pop(0) if s.out else None

c=cb.Client(Fake());cat=c.catalog()
assert c.fmt==2 and sorted(cat)==list(cb.IDS2) and cat[0][0]==6144
items={k:c.get(k,cat[k]) for k in cb.IDS2}
for k,v in items.items():
    assert v==OBJ[k],k
    cb.validate(k,v,2)
d=Path(tempfile.mkdtemp());f=d/"b.zip";cb.save_archive(f,items,2)
fmt,res=cb.load_archive(f);assert fmt==2 and res==OBJ
print("caravel_backup FORMAT 2 self-test: ok (catalog, 8 objects, archive round trip)")
