#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""tools/fm1_install.py against a simulated FM-1 (no hardware, no mido).
The fake device is the one in web/test_web.mjs: identity on the handshake,
then "device asks, host answers" reads of the logical image. Run from the repo root:
  python3 tests/install_test.py
Also checks logical_image/product_of against web/fm1pkg.js (needs node and
build/caravel.fwsc, skipped otherwise)."""
import io
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import json
import hashlib
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install as I  # noqa: E402

if os.environ.get("CLI_TARGET_NEGATIVE"):
    # Reintroduce the former normal-target bypass for an explicit negative control.
    def unchecked_target(raw, product, image, target, record_path=None):
        return I.VerifiedTarget(target,product,hashlib.sha256(image).hexdigest())
    I.verify_target=unchecked_target

I.DELAY.update(open=0, start=0.01, reply=0, loader=0.01, reboot=0.01, retry=0.02, poll=0.05, hs=0.1,
               idle_check=0.4, idle_write=0.4, wait_loader=2, wait_reboot=2)
failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<64} {'ok' if cond else 'FAIL'}")
    failed += not cond


# ---------------------------------------------------------- simulated FM-1 ---

class FakeLink:
    def __init__(self, dev, gen):
        self.dev, self.gen, self.q, self.closed = dev, gen, queue.Queue(), False

    @property
    def lost(self):
        return self.gen != self.dev.gen or not self.dev.connected

    def send(self, pkt):
        if self.lost:
            return False
        self.dev.sent.append(bytes(pkt))
        self.dev.rx(bytes(pkt))
        return True

    def read(self, timeout):
        try:
            return self.q.get(timeout=max(timeout, 0))
        except queue.Empty:
            return None

    def drain(self):
        while not self.q.empty():
            self.q.get_nowait()

    def close(self):
        self.closed = True


class FakeFM1:
    """a MIDI backend with one FM-1 on it"""

    def __init__(self, image, identity="FM-1_015", name="FM-1", unplug_after=None, after_write="FM-1_900",
                 stall_after=None, bad_addr=None, protocol_generation=1, loader_protocol=18):
        self.protocol_generation=protocol_generation
        self.loader_protocol=loader_protocol
        self.image, self.unplug_after, self.after_write = image, unplug_after, after_write
        self.stall_after, self.bad_addr = stall_after, bad_addr
        self.served = self.bad = self.upgrades = 0
        self.sent, self.links, self.gen, self.lock = [], [], 0, threading.Lock()
        self.boot(identity, name)

    def boot(self, identity, name):
        with self.lock:
            self.gen += 1
            self.identity, self.name, self.connected = identity, name, True
            self.waiting, self.queue = None, []

    def input_names(self):
        return [self.name] if self.connected else []

    output_names = input_names

    def open(self, in_name, out_name):
        if in_name != self.name or not self.connected:
            raise IOError("no such port")
        link = FakeLink(self, self.gen)
        self.links.append(link)
        return link

    def tx(self, pkt):
        for link in self.links:
            if link.gen == self.gen and not link.closed:
                link.q.put(bytes(pkt))

    def rx(self, d):
        if d == I.HS_QUERY:
            t = self.identity.encode()
            self.tx(b"\xF0" + I.pack7(bytes([0, 0x59, 0x11, 0, 0, 0]) + t + bytes(28 - len(t))) + b"\xF7")
        elif d.startswith(I.generation.V1_HEADER if self.protocol_generation==2 else I.generation.CLASSIC_HEADER) and d[4] in (1,65):
            fixture=json.loads((ROOT/'tests/fixtures/generation-info.json').read_text())
            import caravel_backup as cb
            ids=tuple(range(8)) if self.protocol_generation==2 else cb.IDS
            objects={k:(F2_CAP if self.protocol_generation==2 else CLASSIC_CAP) if k==0 else bytes([1,2,3]) if k==1 else b'' for k in ids}
            args=bytes(fixture['modern' if self.protocol_generation==2 else 'classic']) if d[4]==1 else bytes([self.protocol_generation,0,len(ids)])+b''.join(bytes([k])+cb.u32(len(objects[k]))+cb.u32(cb.zlib.crc32(objects[k])) for k in ids)
            self.tx(d[:5]+args+b'\xF7')
        elif d.startswith(I.generation.V1_HEADER if self.protocol_generation==2 else I.generation.CLASSIC_HEADER) and d[4]==66:
            import caravel_backup as cb
            a=d[5:-1];k=a[0];off=cb.r32(a[1:6]);n=a[6]|a[7]<<7
            data=(F2_CAP if self.protocol_generation==2 else CLASSIC_CAP) if k==0 else bytes([1,2,3]) if k==1 else b''
            self.tx(d[:5]+bytes([k,0])+cb.u32(off)+bytes([n&127,n>>7])+cb.pack(data[off:off+n])+b'\xF7')
        elif d == (I.UPGRADE if (self.loader_protocol!=18 if self.identity.startswith("ota-") else self.protocol_generation==1) else I.generation.update_entry(2)):
            self.upgrades += 1
            first = self.bad_addr if self.bad_addr is not None else 0
            self.queue = ([(first + k * 512, 512) for k in range(6)] + [(I.FINISH_WRITE, 8)]
                          if self.identity.startswith("ota-")
                          else [(0, 64), (0x40, 160), (0x1000, 512), (I.FINISH_CHECK, 8)])
            self.next()
        elif d in (I.UPGRADE, I.generation.update_entry(2)):
            pass  # The actual USB parser ignores the other generation's entry.
        elif self.waiting:
            u = I.unpack7(d[1:-1])
            addr, n = self.waiting
            fin = addr >= I.FINISH_CHECK
            want = b"success\0" if fin else self.image[addr:addr + n]
            if u[14:14 + len(want)] != want or len(u) != 15 + len(want):
                self.bad += 1
            self.waiting = None
            self.served += 1
            if self.unplug_after and self.served >= self.unplug_after:
                self.connected = False
                return
            if self.stall_after and self.served >= self.stall_after:
                return
            if addr == I.FINISH_CHECK:
                threading.Timer(0.05, self.boot, ("ota-FM-1_900", "Felucca Update")).start()
            elif addr == I.FINISH_WRITE:
                threading.Timer(0.05, self.boot, (self.after_write, "Felucca")).start()
            else:
                self.next()

    def next(self):
        if not self.queue:
            return
        self.waiting = addr, n = self.queue.pop(0)
        u = bytearray([0, 0x59, 0x30, 0, 0, 0, 0]) + addr.to_bytes(4, "little") + bytes([n & 0xFF, n >> 8, 0])
        u.append(~sum(u[6:14]) & 0xFF)
        self.tx(b"\xF0" + I.pack7(u) + b"\xF7")


# ----------------------------------------------------------------- helpers ---

def package(product="FM-1_900", marker=True, size=0x2000):
    raw = bytearray((i * 7) & 0xFF for i in range(size + I.BLOCKS))
    for i in range(I.BLOCKS):
        raw[i * I.BLK + I.KEEP] = (ord(product[i]) + i + 1) & 0xFF if i < len(product) else 0x7D
    raw[0x1800:0x1800 + 16] = I.LOADER_MARK if marker else bytes(16)
    return bytes(raw)


TMP = Path(tempfile.mkdtemp(prefix="felucca-install-"))
# Valid protocol fixtures: the Classic capsule is synthetic, not a new Classic build.
import caravel_backup as CB
import struct
F2_CAP=(ROOT/'tests/format2_project_capsule.bin').read_bytes()
payload=bytearray([0,0]);left=5593
while left:
    n=min(130,left)
    if 0<left-n<3:n=left-3
    payload+=bytes([128+n-3,1,0]);left-=n
cap=bytearray(7200);struct.pack_into('<6I',cap,0,0x314d5048,5,7200,1,0,0)
struct.pack_into('<II',cap,24,len(payload),5594);cap[3872:3872+len(payload)]=payload
struct.pack_into('<I',cap,16,CB.fnv(cap[32:]));struct.pack_into('<I',cap,20,CB.fnv(cap[:20]))
CLASSIC_CAP=bytes(cap)
RECOVERY=TMP/'recovery.zip';CB.save_archive(RECOVERY,{k:F2_CAP if k==0 else bytes([1]) if k==1 else b'' for k in CB.IDS2},2)



def pkgfile(name, raw):
    p = TMP / name
    p.write_bytes(raw)
    return str(p)


def cli(args, dev, answer=True):
    out, err = io.StringIO(), io.StringIO()
    old, sys.stderr = sys.stderr, err
    try:
        selected=list(args)
        if "--info" not in selected:
            package_path = Path(selected[0])
            raw = package_path.read_bytes() if package_path.is_file() else b""
            target = "stock" if hashlib.sha256(raw).hexdigest() == I.STOCK_V15_SHA256 else "2"
            # These transport tests deliberately use synthetic packages and a
            # matching synthetic release record. Target mismatch tests below
            # exercise the production refusal before opening MIDI.
            package_path.with_name("release-build.json").write_text(json.dumps({
                "schema":1,"version":"1.0.0","product":I.product_of(raw) if len(raw)>960 else "FM-1_900",
                "binaries":{"caravel-1.0.0.fwsc":hashlib.sha256(raw).hexdigest()}}))
            selected += ["--target-generation",target,"--backup-dir",str(TMP)]
            if dev.identity.startswith("ota-"): selected += ["--recovery-backup",str(RECOVERY)]
        rc = I.main(selected, backend=dev, out=out, ask=lambda _p: answer)
    finally:
        sys.stderr = old
    return rc, out.getvalue(), err.getvalue()


# ------------------------------------------------------------------- tests ---

def target_binding():
    raw = package()
    p = Path(pkgfile("target.fwsc",raw))
    image = I.logical_image(raw)
    record = p.with_name("release-build.json")
    valid = {"schema":1,"version":"1.0.0","product":"FM-1_900",
             "binaries":{"caravel-1.0.0.fwsc":hashlib.sha256(raw).hexdigest()}}
    record.write_text(json.dumps(valid))
    verified = I.verify_target(raw,"FM-1_900",image,"2",record)
    ok(verified.generation=="2" and verified.image_sha256==hashlib.sha256(image).hexdigest(),
       "target: valid 1.x record binds the exact logical image")
    invalid = [None, [], {**valid,"schema":2}, {**valid,"version":"0.13.0"},
               {**valid,"product":"FM-1_999"}, {**valid,"binaries":{"caravel-1.0.0.fwsc":"0"*64}}]
    blocked = 0
    for candidate in invalid:
        if candidate is None: record.unlink(missing_ok=True)
        else: record.write_text(json.dumps(candidate))
        dev=FakeFM1(image,protocol_generation=2)
        rc=I.main([str(p),"--yes","--target-generation","2","--backup-dir",str(TMP)],backend=dev,out=io.StringIO())
        blocked += rc==2 and not dev.sent
    ok(blocked==len(invalid),"target: missing/schema/version/product/hash records all reject before MIDI")
    record.write_text(json.dumps(valid))
    # Pin substitution isolates the known-package rule without distributing
    # stock or Classic binaries in these small synthetic transport fixtures.
    pins=(I.CLASSIC_013_SHA256,I.STOCK_V15_SHA256)
    try:
        pin_blocks=0
        for name in ("CLASSIC_013_SHA256","STOCK_V15_SHA256"):
            setattr(I,name,hashlib.sha256(raw).hexdigest())
            dev=FakeFM1(image,protocol_generation=2)
            rc=I.main([str(p),"--yes","--target-generation","2","--backup-dir",str(TMP)],backend=dev,out=io.StringIO())
            pin_blocks += rc==2 and not dev.sent
            I.CLASSIC_013_SHA256,I.STOCK_V15_SHA256=pins
        ok(pin_blocks==2,"target: Classic/stock pins cannot be relabelled by a matching 1.x record")
    finally:
        I.CLASSIC_013_SHA256,I.STOCK_V15_SHA256=pins
    dev=FakeFM1(image); device=I.Updater(dev).find()
    dev.sent.clear()
    try:
        I.Updater(dev).install(device,image,"FM-1_900")
        rejected=False
    except I.InstallError: rejected=True
    ok(rejected and not dev.sent,"low-level updater refuses an unverified target before update requests")
    dev=FakeFM1(image); device=I.Updater(dev).find();dev.sent.clear()
    try:
        I.Updater(dev).install(device,image+b"x","FM-1_900",verified_target=verified)
        rejected=False
    except I.InstallError: rejected=True
    ok(rejected and not dev.sent,"low-level updater refuses a changed image after target verification")

def wire():
    data = bytes(range(256)) * 3
    ok(I.unpack7(I.pack7(data))[:len(data)] == data, "pack7 / unpack7 round trip")
    pkt = I.response(0x12345, b"\x01\x02\x03", fl=5)
    u = I.unpack7(pkt[1:-1])
    ok(u[:3] == b"\x00\x59\x30" and int.from_bytes(u[7:11], "little") == 0x12345 and u[14:17] == b"\x01\x02\x03"
       and ~sum(u[6:17]) & 0xFF == u[17], "response: header, address, data, checksum")
    raw = package("FM-1_906")
    ok(I.product_of(raw) == "FM-1_906" and len(I.logical_image(raw)) == len(raw) - 20, "product_of / logical_image (synthetic)")


def installs():
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 2 and dev.served == 11 and "done: the FM-1 runs FM-1_900" in out
       and "100%" in out, f"install: running -> loader -> Felucca ({dev.served} reads)")

    dev = FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 1 and "update mode" in out, "install: device already in update mode -> finishes the write")

    dev = FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", loader_protocol=1)
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 1,
       "install: legacy loader still resumes after ignoring protocol18 entry")

    dev = FakeFM1(image, identity="ota-FM-1_900", name="Caravel Update")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 1 and "update mode" in out, "install: a device named Caravel Update (Caravel's loader) is found too")

    dev = FakeFM1(image, identity="FM-1_900", name="Caravel")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and "done: the FM-1 runs FM-1_900" in out, "install: a running device named Caravel is found too (Felucca's name works as well: above)")

    dev = FakeFM1(image, protocol_generation=2)
    rc, out, err = cli([p,"--yes"],dev)
    ok(rc==0 and dev.upgrades==2 and "Complete backup saved and verified" in out,
       "protocol 18: real F2 capsule archived before the new update entry")
    dev = FakeFM1(image)
    sink=io.StringIO()
    rc=I.main([p,"--yes"],backend=dev,out=sink,ask=lambda _:True)
    ok(rc!=0 and not dev.sent,"CLI: missing explicit target rejected before any MIDI")
    dev = FakeFM1(image,identity="ota-FM-1_900",name="Caravel Update")
    rc=I.main([p,"--yes","--target-generation","2"],backend=dev,out=sink,ask=lambda _:True)
    ok(rc!=0 and dev.upgrades==0,"CLI: loader recovery requires a saved matching backup")

    dev = FakeFM1(image)
    rc, out, err = cli([p], dev, answer=False)
    ok(rc == 1 and dev.upgrades == 0 and "cancelled" in out, "install: answer no -> backup verified, no update entry sent")
    ok("own risk" in out and "Transporter" in out and "complete backup" in out, "install: the risk notice is printed before the question")

    dev = FakeFM1(image, identity="FM-1_905", name="Felucca")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "FM-1_905" in out and "running" in out and dev.upgrades == 0, "--info: identity of the connected FM-1")

    dev = FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "update loader" in out, "--info: device in update mode")

    dev = FakeFM1(image, identity="\x1b[2J\x1b]0;x\x07FM-1_905", name="Felucca")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "FM-1_905" in out and not any(c in out for c in "\x1b\x07"),
       "--info: control bytes in the device's identity are not printed")
    import fm1_sample_upload as U
    lines = U.slot_info([3, 80, 2, 0x41, 0x1B, 0x5B, 0x32, 0x4A, 0, 40, 0, 0, 0, 0, 0, 0])
    ok(lines[0] == "USR1: 2 zones, A?[2J, 40 KiB" and lines[1] == "USR2: empty", "sample upload: slot names printable only")
    try:
        U.slot_info([3, 80, 2, 0x41])
        ok(False, "sample upload: a malformed SMP_INFO reply is an error")
    except ValueError:
        ok(True, "sample upload: a malformed SMP_INFO reply is an error")


def errors():
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    rc, out, err = cli([p, "--yes"], FakeFM1(image, name="IAC Driver Bus 1"))
    ok(rc == 3 and "not found" in err and "IAC Driver" in err, "no FM-1 port -> exit 3, lists the MIDI inputs")
    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes", "--port", "Felucca"], dev)
    ok(rc == 3 and dev.upgrades == 0, "--port that matches nothing -> exit 3")
    dev = FakeFM1(image, name="My Interface")
    rc, out, err = cli(["--info", "--port", "my int"], dev)
    ok(rc == 0 and "FM-1_015" in out, "--port picks a port the default match skips")

    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, unplug_after=2))
    ok(rc == 4 and "disconnected" in err and "nothing was written" in err and time.monotonic() - t0 < 3,
       "unplugged in step 1 -> exit 4 'lost', nothing written")
    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", unplug_after=3))
    ok(rc == 4 and "run the install again" in err and time.monotonic() - t0 < 3, "unplugged during the write -> exit 4 at once")
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", stall_after=3))
    ok(rc == 4 and "stopped answering" in err, "loader stops answering -> exit 4 after the idle time")
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", bad_addr=len(image)))
    ok(rc == 4 and "outside the package" in err, "read past the package -> exit 4")

    rc, out, err = cli([p, "--yes"], FakeFM1(image, after_write="FM-1_015"))
    ok(rc == 6 and "reports FM-1_015" in err, "another identity after the restart -> exit 6")
    dev = FakeFM1(image, identity="XY-9_001", name="usb-midi")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0, "another model -> exit 6, nothing sent")

    class Stuck(FakeFM1):          # the loader never shows up
        def boot(self, identity, name):
            super().boot(identity, name)
            if identity.startswith("ota-"):
                self.connected = False
    rc, out, err = cli([p, "--yes"], Stuck(image))
    ok(rc == 5 and "loader did not appear" in err, "no loader after step 1 -> exit 5")

    plain = pkgfile("plain.fwsc", package(marker=False))
    dev = FakeFM1(image)
    rc, out, err = cli([plain, "--yes"], dev)
    ok(rc == 2 and "no Felucca update loader" in err and dev.sent == [], "package without the loader marker -> exit 2, no MIDI")
    dev = FakeFM1(I.logical_image(package(marker=False)))
    rc, out, err = cli([plain, "--yes", "--force"], dev)
    ok(rc == 0 and dev.bad == 0, "... installs with --force")
    rc, out, err = cli([pkgfile("short.fwsc", b"\0" * 100), "--yes"], FakeFM1(image))
    ok(rc == 2 and "too short" in err, "not a package -> exit 2")
    rc, out, err = cli([str(TMP / "missing.fwsc"), "--yes"], FakeFM1(image))
    ok(rc == 2, "missing file -> exit 2")


def against_js():
    clean = ROOT / "build/caravel.fwsc"
    if not shutil.which("node") or not clean.exists() or not (ROOT / "web/fm1pkg.js").exists():
        print("logical image vs fm1pkg.js: skipped (needs node, web/fm1pkg.js and build/caravel.fwsc)")
        return
    js = ("import { logicalImage, productOf } from %r; import { readFileSync } from 'node:fs';"
          "const p = readFileSync(process.argv[1]); process.stderr.write(productOf(p));"
          "process.stdout.write(logicalImage(p));") % str(ROOT / "web/fm1pkg.js")
    r = subprocess.run(["node", "--input-type=module", "-e", js, str(clean)], capture_output=True, check=True)
    raw = clean.read_bytes()
    ok(I.logical_image(raw) == r.stdout and I.product_of(raw) == r.stderr.decode(),
       f"logical_image / product_of == fm1pkg.js ({clean.name}, {I.product_of(raw)})")
    ok(I.LOADER_MARK in raw, f"{clean.name} carries the Felucca loader marker")


def official():
    """#32: back to the official V15 from Felucca, without --force (only the unmodified file)"""
    v15 = ROOT / "FM-1_v15.fwsc"
    if not v15.exists():
        print("official V15 restore: skipped (needs FM-1_v15.fwsc in the repo root)")
        return
    raw = v15.read_bytes()
    dev = FakeFM1(I.logical_image(raw), identity="FM-1_900", name="Felucca", after_write="FM-1_015")
    rc, out, err = cli([str(v15), "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and "FM-1_015" in out and dev.identity == "FM-1_015",
       "official V15 (FM-1.fwsc) from Felucca: installed without --force, back as FM-1_015")
    bad = bytearray(raw)
    bad[-1] ^= 1
    dev = FakeFM1(I.logical_image(bytes(bad)), identity="FM-1_900", name="Felucca")
    rc, out, err = cli([pkgfile("v15mod.fwsc", bytes(bad)), "--yes"], dev)
    ok(rc == 2 and "official V15" in err and dev.sent == [], "a modified V15 is still refused (no MIDI)")


wire()
target_binding()
if os.environ.get("CLI_TARGET_NEGATIVE"):
    print("target negative control: failed checks",failed)
    shutil.rmtree(TMP,ignore_errors=True)
    sys.exit(1 if failed else 0)
installs()
errors()
against_js()
official()
shutil.rmtree(TMP, ignore_errors=True)
print(f"INSTALL TESTS FAILED ({failed})" if failed else "install tests passed")
sys.exit(1 if failed else 0)
