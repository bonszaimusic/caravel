#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Python USB client against the real C backup handler with a binary host bridge."""
import sys,struct,subprocess,tempfile,zlib
from pathlib import Path
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'tools'))
import caravel_backup as b
import sampleio
class Bridge:
 def __init__(self):self.p=subprocess.Popen([sys.argv[1],'--bridge'],stdin=subprocess.PIPE,stdout=subprocess.PIPE)
 def drain(self):pass
 def send(self,p):
  self.cmd=p[4];a=p[5:-1];self.p.stdin.write(bytes([self.cmd])+struct.pack('<H',len(a))+a);self.p.stdin.flush();return True
 def read(self,timeout):
  size=self.p.stdout.read(2)
  if len(size)!=2:raise ValueError('C bridge stopped')
  n=struct.unpack('<H',size)[0];return b.HDR+bytes([self.cmd])+self.p.stdout.read(n)+b'\xF7'
 def close(self):self.p.stdin.close();self.p.wait(timeout=10)
link=Bridge();c=b.Client(link)
try:
 catalog=c.catalog();assert set(catalog)==set(b.IDS)
 items={k:c.get(k,catalog[k]) for k in b.IDS}
 for k,data in items.items():b.validate(k,data)
 capsule=items[2];assert len(capsule) in (7200,10000)
 c.put(4,capsule);cat=c.catalog();restored=c.get(4,cat[4]);assert restored[32:]==capsule[32:]
 c.put(0,capsule);assert c.get(0,c.catalog()[0])[32:]==capsule[32:]
 with tempfile.TemporaryDirectory() as d:
  file=Path(d)/'backup.zip';b.save_archive(file,items);assert b.load_archive(file)==items
  file.write_bytes(b'broken archive')
  try:b.load_archive(file)
  except b.zipfile.BadZipFile:pass
  else:raise AssertionError('Corrupt ZIP accepted')
 # Native archived projects also remain acceptable to the client.
 b.validate(2,capsule[32:32+3840])
 bad=bytearray(capsule);bad[200]^=1
 try:b.validate(0,bad)
 except ValueError:pass
 else:raise AssertionError('Corrupt HPM1 accepted')
 # A format-3 capsule (0.9.0: a set of snapshots per track) and a format-2 one (0.7.1 to 0.8.0): built here, valid and compressed to the length the firmware writes
 def hpm(version,rawlen):
  payload=bytearray([0,0]);left=rawlen-1                        # one literal zero byte, then matches of distance 1 (3 to 130 bytes each)
  while left:
   n=min(130,left)
   if 0<left-n<3:n=left-3
   payload+=bytes([128+n-3,1,0]);left-=n
  payload=bytes(payload)
  assert len(payload)<=3328
  cap=bytearray(7200);struct.pack_into("<6I",cap,0,0x314D5048,version,7200,1,0,0);struct.pack_into("<II",cap,24,len(payload),rawlen);cap[3872:3872+len(payload)]=payload
  struct.pack_into("<I",cap,16,b.fnv(bytes(cap[32:])));struct.pack_into("<I",cap,20,b.fnv(bytes(cap[:20])));return bytes(cap)
 b.validate(2,hpm(3,5558));b.validate(2,hpm(2,5518))
 b.validate(2,hpm(4,5594));b.validate(2,hpm(5,5594))      # 0.13.0 ELASTIC capsules (v3 block + 4 x 9 bytes): backed up and restored as opaque bytes
 for version,rawlen in ((3,5518),(2,5558),(4,5558),(5,5558),(3,5594),(4,5518),(6,5594)):
  try:b.validate(2,hpm(version,rawlen))
  except ValueError:pass
  else:raise AssertionError(f'HPM1 version {version} with a raw length of {rawlen} accepted')
 # Sample restore uses the existing erase/data/header-last protocol.
 hdr,audio=sampleio.user_slot('TEST',[(list(range(-100,100)),60,None,None)])
 sample=hdr.ljust(512,b'\0')+audio;b.validate(32,sample)
 class SampleClient(b.Client):
  def __init__(self):self.calls=[]
  def req(self,cmd,args,timeout=12):
   self.calls.append((cmd,bytes(args)))
   if cmd==12:return bytes(args[:4])+b'\0'
   return bytes([args[0],0])
 s=SampleClient();s.sample(32,sample);assert s.calls[0]==(14,b'\0') and s.calls[-1][0]==13
 data=b''.join(b.unpack(args[4:]) for cmd,args in s.calls if cmd==12);assert data==audio
 assert b.unpack(s.calls[-1][1][1:])==hdr
 print('USB client: real C LIST/GET/PUT, complete HPM1 slot/WORK roundtrips, 13-object ZIP with CRC, native projects, format-2 to format-5 capsules, corrupt HPM/ZIP rejection and sample header-last wire format passed.')
finally:link.close()
