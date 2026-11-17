// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// fm1backup.js against a simulated device: 7-bit packing, CRC, manifest checks, a full capture,
// validation before the first write, restore order (live music last) and abort on a failed chunk.
import { BACKUP_IDS, BACKUP_IDS_F2, BACKUP_CMD, bkU32, bkR32, bkPack, bkUnpack, bkCrc, bkManifest, readBackup, captureBackup, restoreBackup } from "./fm1backup.js";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(72)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const throws = (f) => { try { f(); return false; } catch { return true; } };
const athrows = async (f) => { try { await f(); return false; } catch { return true; } };

const rnd = (n, seed) => Uint8Array.from({ length: n }, (_, i) => (i * 131 + seed * 17 + (i >> 3)) & 255);
ok(bkR32(bkU32(0xdeadbeef)) === 0xdeadbeef && bkCrc(new TextEncoder().encode("123456789")) === 0xcbf43926, "backup: numbers and CRC-32");
const b = rnd(1000, 1);
ok(bkUnpack(bkPack(b), b.length).every((v, i) => v === b[i]), "backup: 7-bit pack / unpack round trip");
ok(throws(() => bkUnpack([...bkPack(b.subarray(0, 14)), 0], 14)), "backup: trailing bytes refused");

/* a device: objects by id, sample slots, the order of writes */
function device(objs, opt = {}) {
  const d = { objs: new Map(objs), log: [], staged: null };
  d.request = async ([cmd, a]) => {
    if (cmd === BACKUP_CMD.LIST) {
      const out = [1, 0, BACKUP_IDS.length];
      for (const id of BACKUP_IDS) { const v = d.objs.get(id) || new Uint8Array(0); out.push(id, ...bkU32(v.length), ...bkU32(v.length ? bkCrc(v) : 0)); }
      return out;
    }
    if (cmd === BACKUP_CMD.GET) {
      const id = a[0], off = bkR32(a, 1), n = a[6] | a[7] << 7, v = d.objs.get(id);
      if (opt.changeOnGet === id && off === 0) v[0] ^= 1;
      return [id, 0, ...bkU32(off), n & 127, n >> 7, ...bkPack(v.subarray(off, off + n))];
    }
    if (cmd === BACKUP_CMD.PUT) {
      const [op, id] = a;
      if (op === 0 && id > (opt.maxId ?? 9)) { d.log.push(`refused ${id}`); return [op, id, 1]; }   // (an older firmware)
      if (op === 0) { d.staged = { id, size: bkR32(a, 2), crc: bkR32(a, 7), bytes: [] }; return [op, id, 0]; }
      if (op === 1) {
        if (opt.failChunk === id) return [op, id, 2];
        const off = bkR32(a, 2), p = a.slice(7), n = Math.min(256, d.staged.size - off);
        d.staged.bytes.push(...bkUnpack(p, n)); return [op, id, 0];
      }
      if (op === 2) { const s = Uint8Array.from(d.staged.bytes); const rc = bkCrc(s) === d.staged.crc ? 0 : 2; if (!rc) { d.objs.set(id, s); d.log.push(id); } return [op, id, rc]; }
      if (op === 3) { d.log.push(`abort ${id}`); return [op, id, 0]; }
    }
    if (cmd === 11 || cmd === 12 || cmd === 13 || cmd === 14) { if (cmd === 13 || cmd === 14) d.log.push(32 + a[0]); return [a[0], 0]; }
    throw new Error(`unexpected ${cmd}`);
  };
  return d;
}
const objs = [[0, rnd(3388, 2)], [1, rnd(1200, 3)], [2, rnd(3388, 4)], [6, rnd(3080, 5)]];
const dev = device(objs);
const file = await captureBackup(dev.request, "TEST");
ok(file.objects.length === BACKUP_IDS.length && file.objects[0].size === 3388 && file.objects[3].size === 0, "backup: capture lists every object, empty ones as 0");
ok(readBackup(JSON.stringify(file)).objects[2].bytes.every((v, i) => v === objs[2][1][i]), "backup: capture -> file -> bytes round trip");
ok(await athrows(() => captureBackup(device(objs.map(([i, v]) => [i, v.slice()]), { changeOnGet: 2 }).request, "TEST")), "backup: a device that changes during capture fails the capture");

const bad = JSON.parse(JSON.stringify(file)); bad.objects[2].crc ^= 1;
ok(throws(() => readBackup(bad)), "backup: a damaged file is refused");
const noRun = JSON.parse(JSON.stringify(file)); noRun.objects[0] = { ...noRun.objects[0], size: 0, crc: 0, data: "" };
ok(throws(() => readBackup(noRun)), "backup: a file without the current music is refused");
ok(throws(() => bkManifest([1, 0, 3])), "backup: a short manifest is refused");

const target = device([]);
const damaged = JSON.parse(JSON.stringify(file)); damaged.objects[6].crc ^= 1;
ok(await athrows(() => restoreBackup(target.request, damaged)) && target.log.length === 0, "backup: restore validates every byte before the first write");
await restoreBackup(target.request, file);
const order = target.log.filter((x) => typeof x === "number");
ok(order.at(-1) === 0 && order.at(-2) === 1 && order.indexOf(2) < order.indexOf(1), "backup: restore order: projects, banks, samples, settings, live music last");
ok([0, 1, 2, 6].every((id) => target.objs.get(id).every((v, i) => v === objs.find((o) => o[0] === id)[1][i])), "backup: restored objects equal the source");
{   /* the FM6 patch bank (id 8) and the archives of firmware before it (11 objects, no id 8) */
  ok(BACKUP_IDS.length === 13 && file.objects.some((o) => o.id === 8) && file.objects.some((o) => o.id === 9),
    "backup: 13 objects: id 8 (the retired FM6 bank, empty) and id 9 (the user presets' FM6 patches)");
  const v2 = JSON.parse(JSON.stringify(file)); v2.objects = v2.objects.filter((o) => o.id !== 9);
  ok(readBackup(v2).objects.length === 12, "backup: an archive of 1.0..1.0.2 (12 objects, the bank as id 8) still reads");
  const old = JSON.parse(JSON.stringify(file)); old.objects = old.objects.filter((o) => o.id !== 8 && o.id !== 9);
  ok(readBackup(old).objects.length === 11, "backup: an archive of the 11 objects before FM6 still reads");
  const odd = JSON.parse(JSON.stringify(file)); odd.objects = odd.objects.filter((o) => o.id !== 33);
  ok(throws(() => readBackup(odd)), "backup: an archive missing another object is refused");
}
{   /* 1.0.3: an old archive with a bank restores its id 8 (after 6, 7); a new archive onto 1.0.2 skips id 9 */
  const bankBytes = rnd(3472, 8), patches = rnd(3728, 9);
  const v2dev = device([...objs, [8, bankBytes]]);
  const v2file = await captureBackup(async ([cmd, a]) => {   // (a 1.0.2 device: 12 objects)
    const r = await v2dev.request([cmd, a]);
    if (cmd !== BACKUP_CMD.LIST) return r;
    const n = r[2], keep = [];
    for (let i = 0; i < n; i++) if (r[3 + i * 11] !== 9) keep.push(...r.slice(3 + i * 11, 14 + i * 11));
    return [1, 0, keep.length / 11, ...keep];
  }, "1.0.2");
  const to103 = device([]);
  await restoreBackup(to103.request, v2file);
  const lg = to103.log.filter((x) => typeof x === "number");
  ok(v2file.objects.length === 12 && lg.indexOf(8) > lg.indexOf(7) && lg.indexOf(7) > lg.indexOf(6) &&
     to103.objs.get(8).every((v, i) => v === bankBytes[i]), "backup: a 1.0.2 archive (id 8, the bank) restores, its bank after the user presets");
  const newFile = await captureBackup(device([...objs, [9, patches]]).request, "1.0.3");
  const to102 = device([], { maxId: 8 });
  await restoreBackup(to102.request, newFile);
  ok(to102.log.includes("refused 9") && !to102.objs.has(9) && to102.log.at(-1) === 0,
    "backup: a 1.0.3 archive onto older firmware: id 9 skipped, the rest restored");
  const to103b = device([]);
  await restoreBackup(to103b.request, newFile);
  ok(to103b.objs.get(9).every((v, i) => v === patches[i]), "backup: a 1.0.3 archive restores id 9");
}
const failing = device([], { failChunk: 2 });
ok(await athrows(() => restoreBackup(failing.request, file)) && failing.log.includes("abort 2") && !failing.log.includes(0), "backup: a refused chunk aborts that object and stops before the live music");

{ // HPM1 performance capsules span 40 transport chunks rather than legacy 3840 bytes.
  const hpm = [0, 2, 3, 4, 5].map(id => [id, rnd(10000, id + 20)]);
  const big = await captureBackup(device([...hpm, [1, rnd(1200, 31)]]).request, "0.5.0");
  const dest = device([]); await restoreBackup(dest.request, big);
  ok(hpm.every(([id, bytes]) => dest.objs.get(id).length === 10000 &&
    dest.objs.get(id).every((v, i) => v === bytes[i])), "backup: 10000-byte WORK and all A-D capsules restore byte for byte");
  ok(await athrows(() => captureBackup(device([[0, rnd(10001, 1)]]).request, "0.5.0")),
    "backup: oversized performance capsule is refused");
  ok(await athrows(() => captureBackup(device([[0, rnd(10000, 1)], [1, rnd(3841, 2)]]).request, "0.5.0")),
    "backup: settings retain their separate legacy bound");
  const corrupt = JSON.parse(JSON.stringify(big)); corrupt.objects[5].crc ^= 1;
  const untouched = device([]);
  ok(await athrows(() => restoreBackup(untouched.request, corrupt)) && untouched.log.length === 0,
    "backup: a corrupt large capsule is rejected before any write");
}
{ // 0.13.0 writes HPM1 v4/v5 capsules (7200 B); the web tool must carry them byte for byte (it never interprets the capsule).
  const cap = (version) => { const b = rnd(7200, 70 + version); const v = new DataView(b.buffer); v.setUint32(0, 0x314d5048, true); v.setUint32(4, version, true); v.setUint32(8, 7200, true); return b; };
  const caps = [[2, cap(4)], [3, cap(5)], [0, cap(5)]];
  const el = await captureBackup(device([...caps, [1, rnd(1200, 41)]]).request, "0.13.0");
  const dst = device([]); await restoreBackup(dst.request, el);
  ok(caps.every(([id, bytes]) => dst.objs.get(id).length === 7200 && dst.objs.get(id).every((v, i) => v === bytes[i])),
    "backup: HPM1 v4/v5 capsules (7200 B) in slots and WORK restore byte for byte");
}

// FORMAT 2: real fixture, simulated device for protocol 2, and CLI ZIP byte interop.
{
  const {readFileSync}=await import("node:fs");
  const {execFileSync}=await import("node:child_process");
  const {createHash}=await import("node:crypto");
  const capsule=new Uint8Array(readFileSync(new URL("../tests/format2_project_capsule.bin",import.meta.url)));
  const bank=new Uint8Array(3080),view=new DataView(bank.buffer);
  view.setUint32(0,0x32425055,true);
  view.setUint16(4,192,true);view.setUint16(6,16,true);
  const input=[[0,capsule],[1,rnd(1200,14)],[2,capsule],[6,bank]];
  function f2device(items) {
    const d=device(items,{maxId:7});
    const request=async packet=>{
      if(packet[0]!==BACKUP_CMD.LIST)return d.request(packet);
      const payload=[2,0,8];
      for(const id of BACKUP_IDS_F2){
        const bytes=d.objs.get(id)||new Uint8Array();
        payload.push(id,...bkU32(bytes.length),...bkU32(bytes.length?bkCrc(bytes):0));
      }
      return payload;
    };
    return {...d,request};
  }
  const src=f2device(input),backup=await captureBackup(src.request,"CARAVEL 1.0.0");
  ok(backup.version===2&&backup.format==="CaravelBackup2"&&backup.objects.length===8,
     "backup: protocol 2 selects exactly IDs 0..7 and CaravelBackup2");
  ok(backup.objects[0].size===6144&&backup.objects[6].size===3080&&
     readBackup(JSON.stringify(backup)).objects[0].bytes.every((x,i)=>x===capsule[i]),
     "backup: real FORMAT 2 capsule survives capture and Web JSON read");
  const dst=f2device(input.map(([id,data])=>[id,data.slice()]));
  await restoreBackup(dst.request,backup);
  ok(BACKUP_IDS_F2.every(id=>{
    const a=src.objs.get(id)||new Uint8Array(),b=dst.objs.get(id)||new Uint8Array();
    return a.length===b.length&&a.every((x,i)=>x===b[i]);
  })&&dst.log.at(-1)===0&&dst.log.at(-2)===1,
     "backup: protocol 2 restore preserves every byte, restores WORK and SETTINGS last");
  const classic=device([]),newTarget=f2device(input);
  ok(await athrows(()=>restoreBackup(classic.request,backup))&&classic.log.length===0,
     "backup: protocol 2 backup cannot restore onto protocol 1");
  ok(await athrows(()=>restoreBackup(newTarget.request,file))&&newTarget.log.length===0,
     "backup: protocol 1 backup cannot restore onto protocol 2");
  const corrupt=JSON.parse(JSON.stringify(backup));corrupt.objects[0].crc^=1;
  ok(await athrows(()=>restoreBackup(newTarget.request,corrupt))&&newTarget.log.length===0,
     "backup: corrupt protocol 2 archive fails before PUT");
  // JSON and ZIP containers are different. The uncompressed device objects must match.
  const py=[
    "import base64,hashlib,json,sys,tempfile",
    "from pathlib import Path",
    "sys.path.insert(0,'tools')",
    "import caravel_backup as cb",
    "archive=json.loads(sys.stdin.read())",
    "objects={o['id']:base64.b64decode(o['data']) for o in archive['objects']}",
    "assert tuple(objects)==cb.IDS2",
    "for k,v in objects.items():cb.validate(k,v,2)",
    "with tempfile.TemporaryDirectory() as t:",
    " path=Path(t)/'objects.zip'",
    " cb.save_archive(path,objects,2)",
    " fmt,got=cb.load_archive(path)",
    " assert fmt==2 and got==objects",
    " print(json.dumps({str(k):hashlib.sha256(v).hexdigest() for k,v in got.items()},sort_keys=True))"
  ].join("\n");
  let equal=false;
  try{
    const digests=JSON.parse(execFileSync("python3",["-c",py],{
      input:JSON.stringify(backup),encoding:"utf8"
    }));
    equal=backup.objects.every(o=>
      digests[o.id]===createHash("sha256").update(Buffer.from(o.data,"base64")).digest("hex"));
  }catch(e){console.error("FORMAT 2 CLI interoperability:",String(e.stderr||e));}
  ok(equal,"backup: every FORMAT 2 object byte equals Python CLI ZIP roundtrip");
}
console.log(fails ? `BACKUP WEB TESTS FAILED (${fails})` : "backup web tests passed");
process.exit(fails ? 1 : 0);
