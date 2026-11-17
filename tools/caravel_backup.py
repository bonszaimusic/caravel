#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Caravel USB backup and HPM1 exchange (the performance project format of 0.7 to 0.13, and FORMAT 2 of 1.0).

  python3 tools/caravel_backup.py ports
  python3 tools/caravel_backup.py save session.zip
  python3 tools/caravel_backup.py restore session.zip
  python3 tools/caravel_backup.py export A project.hpm
  python3 tools/caravel_backup.py import A project.hpm

Use WORK for the unsaved working project, or A..D for a saved slot.
Full ZIP includes WORK, A..D, settings/MIDI Learn and user presets; up to 0.13 also FM6 and user samples. A FORMAT 2 device (1.0)
answers with its own catalog (ids 0..7, no samples): the archive is then "CaravelBackup2" and only restores to a FORMAT 2 device.
Requires mido + python-rtmidi; close other MIDI editors first. Restoring stops playback.
An interrupted full restore can be incomplete; the source backup remains intact.
"""
import argparse
import json
from pathlib import Path
import struct
import time
import zipfile
import zlib
import fm1_install as midi
import caravel_generation as generation

HDR = generation.V1_HEADER
IDS = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 32, 33, 34)          # up to 0.13
IDS2 = (0, 1, 2, 3, 4, 5, 6, 7)                             # FORMAT 2 (1.0): no FM6, no samples
FORMATS = {1: ("CaravelBackup1", IDS), 2: ("CaravelBackup2", IDS2)}
CAPSULE2 = (6, 6144, 5610)                                  # FORMAT 2 project: version, size, raw snapshot size
BANK2 = (0x32425055, 192, 16, 8 + 16 * 192)                 # "UPB2": magic, record size, slots, bytes
NAMES = {0: "WORK", 1: "SETTINGS", **{i+2: c for i,c in enumerate("ABCD")},
         6:"PRESETS-1", 7:"PRESETS-2", 8:"LEGACY-FM6", 9:"FM6", 32:"USR1", 33:"USR2", 34:"USR3"}


def u32(v):
    return bytes((v >> (7*i)) & 127 for i in range(5))


def r32(b):
    if len(b)!=5 or b[4]>15: raise ValueError("Invalid 32-bit reply")
    return sum(v << (7*i) for i,v in enumerate(b))


def pack(b):
    out=bytearray()
    for i in range(0,len(b),7):
        g=b[i:i+7];out.append(sum((v>>7)<<j for j,v in enumerate(g)));out.extend(v&127 for v in g)
    return bytes(out)


def unpack(b):
    out=bytearray()
    for i in range(0,len(b),8):
        g=b[i:i+8]
        if len(g)<2 or g[0]>>(len(g)-1):raise ValueError("Invalid packed reply")
        out.extend(v|((g[0]>>j)&1)<<7 for j,v in enumerate(g[1:]))
    return bytes(out)


class Client:
    def __init__(self,link,header=HDR):self.link=link;self.header=header
    def req(self,cmd,args=b"",timeout=12):
        self.link.drain()
        if not self.link.send(self.header+bytes([cmd])+bytes(args)+b"\xF7"):raise OSError("MIDI send failed")
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            p=self.link.read(end-time.monotonic())
            if p and p.startswith(self.header+bytes([cmd])) and p[-1]==247:return p[5:-1]
        raise TimeoutError(f"No reply to command {cmd}; reconnect and retry from the original backup")
    def catalog(self):
        try:p=self.req(65,timeout=1.5)
        except TimeoutError:
            if self.header!=HDR:raise
            self.header=generation.CLASSIC_HEADER
            p=self.req(65)
        if len(p)<3 or p[0] not in (1,2) or len(p)!=3+11*p[2]:raise ValueError("Unexpected reply to the backup LIST: this tool and the firmware do not speak the same backup version")
        if p[1]:raise ValueError(f"The device cannot list a backup right now (status {p[1]}): stop playback and recording, leave menus and layers, and retry")
        self.fmt=p[0]
        if (self.fmt==2)!=(self.header==HDR):raise ValueError("Backup framing/LIST generation mismatch")
        out={}
        for at in range(3,len(p),11):
            k=p[at];out[k]=(r32(p[at+1:at+6]),r32(p[at+6:at+11]))
        if self.fmt==1 and out.get(0,(0,))[0] not in (7200,10000):raise ValueError("This tool requires Caravel performance project backup support")
        if self.fmt==2 and (tuple(sorted(out))!=IDS2 or out[0][0]!=CAPSULE2[1]):raise ValueError("The device's FORMAT 2 backup catalog is not the one this tool knows (ids 0..7, project 6144 bytes)")
        return out
    def get(self,k,entry):
        n,crc=entry;out=bytearray()
        for off in range(0,n,256):
            count=min(256,n-off);p=self.req(66,bytes([k])+u32(off)+bytes([count&127,count>>7]))
            if len(p)<9 or p[0]!=k or p[1] or r32(p[2:7])!=off or (p[7]|p[8]<<7)!=count:raise ValueError(f"GET {NAMES[k]} failed")
            b=unpack(p[9:])
            if len(b)!=count:raise ValueError("Short GET reply")
            out.extend(b)
        if zlib.crc32(out)!=crc:raise ValueError(f"{NAMES[k]} changed during backup; retry")
        return bytes(out)
    def put(self,k,data):
        p=self.req(67,bytes([0,k])+u32(len(data))+u32(zlib.crc32(data)))
        if p!=bytes([0,k,0]):raise ValueError(f"PUT begin {NAMES[k]} refused: {list(p)}")
        for off in range(0,len(data),256):
            p=self.req(67,bytes([1,k])+u32(off)+pack(data[off:off+256]))
            if p!=bytes([1,k,0]):raise ValueError(f"PUT data {NAMES[k]} refused: {list(p)}")
        p=self.req(67,bytes([2,k]))
        if p!=bytes([2,k,0]):raise ValueError(f"PUT commit {NAMES[k]} failed: {list(p)}")
    def sample(self,k,data):
        slot=k-32
        p=self.req(14,bytes([slot]))
        if p!=bytes([slot,0]):raise ValueError(f"Erase {NAMES[k]} failed")
        if not data:return
        for off in range(512,len(data),256):
            a=bytes([slot,off&127,(off>>7)&127,(off>>14)&127]);p=self.req(12,a+pack(data[off:off+256]))
            if p!=a+b"\0":raise ValueError(f"Write {NAMES[k]} failed")
        p=self.req(13,bytes([slot])+pack(data[:480]))
        if p!=bytes([slot,0]):raise ValueError(f"Commit {NAMES[k]} failed")


def fnv(b):
    v=2166136261
    for x in b:v=((v^x)*16777619)&0xFFFFFFFF
    return v


def validate(k,b,fmt=1):
    if fmt==2 and k in (6,7):
        if not b:return
        if len(b)!=BANK2[3] or struct.unpack_from("<IHH",b)!=BANK2[:3]:raise ValueError("Invalid FORMAT 2 user preset bank")
    elif fmt==2 and (k==0 or 2<=k<=5) and (k==0 or b) and len(b)!=CAPSULE2[1]:raise ValueError("Invalid FORMAT 2 project length")
    if k>=32:
        if not b:return
        if len(b)<512 or len(b)>0x14000:raise ValueError("Invalid sample length")
        magic,version,nz=struct.unpack_from("<IHB",b)
        n,crc=struct.unpack_from("<II",b,16)
        if magic!=0x504D5346 or version!=1 or not 1<=nz<=16 or n!=len(b)-512 or zlib.crc32(b[512:])!=crc:raise ValueError("Invalid sample header/CRC")
    elif k==0 or 2<=k<=5:
        if not b and k!=0:return
        if len(b) in (7200,10000,6144):
            magic,version,size,seq,crc,hcrc=struct.unpack_from("<6I",b)
            if magic!=(0x324D5048 if version==6 else 0x314D5048) or (version,size) not in ((1,10000),(2,7200),(3,7200),(4,7200),(5,7200),CAPSULE2[:2]) or (fmt==2)!=(version==6) or size!=len(b) or fnv(b[32:])!=crc or fnv(b[:20])!=hcrc:raise ValueError("Invalid HPM1 capsule")
            if version in (2,3,4,5,6):                # 2: 0.7.1-0.8.0 (four shared slots), 3: 0.9.0 (a set per track), 4/5: 0.13.0 ELASTIC (v3 + 4 x 9 B spring settings; 5: tempo-synced times)
                clen,rawlen=struct.unpack_from("<II",b,24)
                if not 0<clen<=3328 or rawlen!={2:5518,3:5558,4:5594,5:5594,6:CAPSULE2[2]}[version]:raise ValueError("Invalid snapshot payload length")
                raw=bytearray();src=memoryview(b)[len(b)-3328:len(b)-3328+clen];at=0
                while at<len(src):
                    tag=src[at];at+=1
                    if tag<128:
                        n=tag+1
                        if n>len(src)-at:raise ValueError("Truncated snapshot literal")
                        raw.extend(src[at:at+n]);at+=n
                    else:
                        n=tag-128+3
                        if len(src)-at<2:raise ValueError("Truncated snapshot match")
                        dist=src[at]|src[at+1]<<8;at+=2
                        if not 0<dist<=min(len(raw),4096):raise ValueError("Invalid snapshot match")
                        for _ in range(n):raw.append(raw[-dist])
                    if len(raw)>rawlen:raise ValueError("Oversize snapshot payload")
                if len(raw)!=rawlen:raise ValueError("Truncated snapshot payload")
        elif len(b)>3840 or len(b)<4 or struct.unpack_from("<I",b)[0]>>8!=0x46554E:raise ValueError("Invalid native project")


def save_archive(path,items,fmt=1):
    rows=[]
    with zipfile.ZipFile(path,"w",zipfile.ZIP_DEFLATED) as z:
        for k in FORMATS[fmt][1]:
            b=items[k];name=f"{k:02d}-{NAMES[k]}.bin";z.writestr(name,b);rows.append(dict(id=k,file=name,size=len(b),crc=zlib.crc32(b)))
        z.writestr("manifest.json",json.dumps(dict(format=FORMATS[fmt][0],objects=rows),indent=2))


def load_archive(path):
    with zipfile.ZipFile(path) as z:
        if sum(p.file_size for p in z.infolist())>1024*1024:raise ValueError("Backup exceeds device capacity")
        m=json.loads(z.read("manifest.json"));rows=m.get("objects",[])
        fmt=next((f for f,(name,ids) in FORMATS.items() if m.get("format")==name),0)
        if not fmt or tuple(p.get("id") for p in rows)!=FORMATS[fmt][1]:raise ValueError("Invalid archive manifest")
        result={}
        for p in rows:
            b=z.read(p["file"])
            if len(b)!=p["size"] or zlib.crc32(b)!=p["crc"]:raise ValueError("Archive CRC mismatch")
            validate(p["id"],b,fmt);result[p["id"]]=b
    return fmt,result


def main():
    ap=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--input",help="Exact MIDI input port");ap.add_argument("--output",help="Exact MIDI output port");ap.add_argument("--yes",action="store_true")
    sub=ap.add_subparsers(dest="cmd",required=True);sub.add_parser("ports")
    for cmd in ("save","restore"):sub.add_parser(cmd).add_argument("file",type=Path)
    for cmd in ("export","import"):
        p=sub.add_parser(cmd);p.add_argument("slot",choices=["WORK",*"ABCD"]);p.add_argument("file",type=Path)
    a=ap.parse_args();backend=midi.MidoBackend()
    if a.cmd=="ports":print("INPUT:",*backend.input_names(),"OUTPUT:",*backend.output_names(),sep="\n");return
    data=None;afmt=0
    if a.cmd=="restore":afmt,data=load_archive(a.file)
    if a.cmd=="import":data=a.file.read_bytes()      # (validated once the device has said which format it speaks)
    if a.cmd in ("restore","import") and not a.yes:
        if input("Overwrite the selected project / archive objects and stop playback? [y/N] ").strip().lower()!="y":return
    def choose(explicit,names):
        if explicit in names:return explicit
        if explicit:raise ValueError("Requested MIDI port not found")
        found=[p for p in names if any(x in p.lower() for x in ("caravel","felucca","fm-1","fm1"))]
        if len(found)!=1:raise ValueError("Select exact --input and --output ports; list them with 'ports'")
        return found[0]
    link=backend.open(choose(a.input,backend.input_names()),choose(a.output,backend.output_names()))
    try:
        c=Client(link);catalog=c.catalog();fmt=c.fmt
        if a.cmd=="import":validate(0,data,fmt)
        if a.cmd=="save":
            items={}
            for k in FORMATS[fmt][1]:
                if k not in catalog:raise ValueError(f"Missing backup object {k}")
                items[k]=c.get(k,catalog[k]);validate(k,items[k],fmt);print(f"Read {NAMES[k]}: {len(items[k])} bytes")
            save_archive(a.file,items,fmt)
        elif a.cmd=="restore":
            if afmt!=fmt:raise ValueError(f"This archive is {FORMATS[afmt][0]}, the device speaks {FORMATS[fmt][0]}: it cannot be restored to this device")
            for k in (6,7,8,9,32,33,34,2,3,4,5,0,1) if fmt==1 else (6,7,2,3,4,5,0,1):
                if k>=32:c.sample(k,data[k])
                else:c.put(k,data[k])
                print(f"Restored {NAMES[k]}")
        else:
            k=0 if a.slot=="WORK" else 2+"ABCD".index(a.slot)
            if a.cmd=="export":
                b=c.get(k,catalog[k])
                if not b:raise ValueError("Selected project slot is empty")
                a.file.write_bytes(b)
            else:c.put(k,data)
        print("Done.")
    finally:link.close()


if __name__=="__main__":
    try:main()
    except (ValueError,OSError,TimeoutError,zipfile.BadZipFile,ImportError) as e:raise SystemExit(str(e)) from None
