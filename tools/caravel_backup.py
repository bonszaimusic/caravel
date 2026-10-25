#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Caravel Performance V4 USB backup and HPM1 exchange.

  python3 tools/caravel_backup.py ports
  python3 tools/caravel_backup.py save session.zip
  python3 tools/caravel_backup.py restore session.zip
  python3 tools/caravel_backup.py export A project.hpm
  python3 tools/caravel_backup.py import A project.hpm

Use WORK for the unsaved working project, or A..D for a saved slot.
Full ZIP includes WORK, A..D, settings/MIDI Learn, user presets, FM6 and user samples.
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

HDR = bytes([0xF0, 0x7D, 0x46, 0x4C])
IDS = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 32, 33, 34)
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
    def __init__(self,link):self.link=link
    def req(self,cmd,args=b"",timeout=12):
        self.link.drain()
        if not self.link.send(HDR+bytes([cmd])+bytes(args)+b"\xF7"):raise OSError("MIDI send failed")
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            p=self.link.read(end-time.monotonic())
            if p and p.startswith(HDR+bytes([cmd])) and p[-1]==247:return p[5:-1]
        raise TimeoutError(f"No reply to command {cmd}; reconnect and retry from the original backup")
    def catalog(self):
        p=self.req(65)
        if len(p)<3 or p[:2]!=b"\1\0" or len(p)!=3+11*p[2]:raise ValueError("Device refused backup LIST")
        out={}
        for at in range(3,len(p),11):
            k=p[at];out[k]=(r32(p[at+1:at+6]),r32(p[at+6:at+11]))
        if out.get(0,(0,))[0]!=10000:raise ValueError("This tool requires the Performance V4 test firmware")
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


def validate(k,b):
    if k>=32:
        if not b:return
        if len(b)<512 or len(b)>0x14000:raise ValueError("Invalid sample length")
        magic,version,nz=struct.unpack_from("<IHB",b)
        n,crc=struct.unpack_from("<II",b,16)
        if magic!=0x504D5346 or version!=1 or not 1<=nz<=16 or n!=len(b)-512 or zlib.crc32(b[512:])!=crc:raise ValueError("Invalid sample header/CRC")
    elif k==0 or 2<=k<=5:
        if not b and k!=0:return
        if len(b)==10000:
            magic,version,size,seq,crc,hcrc=struct.unpack_from("<6I",b)
            if magic!=0x314D5048 or version!=1 or size!=10000 or fnv(b[32:])!=crc or fnv(b[:20])!=hcrc:raise ValueError("Invalid HPM1 capsule")
        elif len(b)>3840 or len(b)<4 or struct.unpack_from("<I",b)[0]>>8!=0x46554E:raise ValueError("Invalid native project")


def save_archive(path,items):
    rows=[]
    with zipfile.ZipFile(path,"w",zipfile.ZIP_DEFLATED) as z:
        for k in IDS:
            b=items[k];name=f"{k:02d}-{NAMES[k]}.bin";z.writestr(name,b);rows.append(dict(id=k,file=name,size=len(b),crc=zlib.crc32(b)))
        z.writestr("manifest.json",json.dumps(dict(format="CaravelBackup1",objects=rows),indent=2))


def load_archive(path):
    with zipfile.ZipFile(path) as z:
        if sum(p.file_size for p in z.infolist())>1024*1024:raise ValueError("Backup exceeds device capacity")
        m=json.loads(z.read("manifest.json"));rows=m.get("objects",[])
        if m.get("format")!="CaravelBackup1" or tuple(p.get("id") for p in rows)!=IDS:raise ValueError("Invalid archive manifest")
        result={}
        for p in rows:
            b=z.read(p["file"])
            if len(b)!=p["size"] or zlib.crc32(b)!=p["crc"]:raise ValueError("Archive CRC mismatch")
            validate(p["id"],b);result[p["id"]]=b
    return result


def main():
    ap=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--input",help="Exact MIDI input port");ap.add_argument("--output",help="Exact MIDI output port");ap.add_argument("--yes",action="store_true")
    sub=ap.add_subparsers(dest="cmd",required=True);sub.add_parser("ports")
    for cmd in ("save","restore"):sub.add_parser(cmd).add_argument("file",type=Path)
    for cmd in ("export","import"):
        p=sub.add_parser(cmd);p.add_argument("slot",choices=["WORK",*"ABCD"]);p.add_argument("file",type=Path)
    a=ap.parse_args();backend=midi.MidoBackend()
    if a.cmd=="ports":print("INPUT:",*backend.input_names(),"OUTPUT:",*backend.output_names(),sep="\n");return
    data=None
    if a.cmd=="restore":data=load_archive(a.file)
    if a.cmd=="import":data=a.file.read_bytes();validate(0,data)
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
        c=Client(link);catalog=c.catalog()
        if a.cmd=="save":
            items={}
            for k in IDS:
                if k not in catalog:raise ValueError(f"Missing backup object {k}")
                items[k]=c.get(k,catalog[k]);validate(k,items[k]);print(f"Read {NAMES[k]}: {len(items[k])} bytes")
            save_archive(a.file,items)
        elif a.cmd=="restore":
            for k in (6,7,8,9,32,33,34,2,3,4,5,0,1):
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
