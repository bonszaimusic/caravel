// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// Local, complete musical archives. Requests name whitelisted objects, never flash addresses.
// 8: the FM6 patch bank of 1.0..1.0.2 (listed empty since 1.0.3: a restore of an older archive's bank moves its patches
// into the user presets restored before it); 9: the user presets' FM6 patches (1.0.3)
import { V1_HEADER, detectGeneration } from "./generation.js";
export const BACKUP_IDS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 32, 33, 34];
export const FORMAT2_BACKUP_IDS = [0, 1, 2, 3, 4, 5, 6, 7];
export const BACKUP_IDS_F2 = FORMAT2_BACKUP_IDS;
const BACKUP_IDS_V2 = BACKUP_IDS.filter((id) => id !== 9);              // 1.0..1.0.2, and their archives
const BACKUP_IDS_V1 = BACKUP_IDS_V2.filter((id) => id !== 8);           // firmware before FM6, and its archives
const idsOf = (n) => [BACKUP_IDS, BACKUP_IDS_V2, BACKUP_IDS_V1].find((ids) => ids.length === n) || null;
export const BACKUP_CMD = { LIST: 65, GET: 66, PUT: 67 };
const BACKUP_CHUNK = 256;
export const bkU32 = (n) => Array.from({ length: 5 }, (_, i) => (n >>> (i * 7)) & (i === 4 ? 15 : 127));
export const bkR32 = (a, off = 0) => {
  if (a.length < off + 5 || a[off + 4] > 15) throw new Error("Invalid archive number");
  return (a[off] | a[off + 1] << 7 | a[off + 2] << 14 | a[off + 3] << 21 | a[off + 4] << 28) >>> 0;
};
export function bkPack(bytes) {
  const a = [];
  for (let off = 0; off < bytes.length; off += 7) {
    const chunk = bytes.subarray(off, off + 7);
    a.push(chunk.reduce((m, b, i) => m | (b >>> 7) << i, 0), ...chunk.map((b) => b & 127));
  }
  return a;
}
export function bkUnpack(a, size) {
  const out = new Uint8Array(size); let i = 0, j = 0;
  while (j < size) {
    const n = Math.min(7, size - j), mask = a[i++];
    if (mask == null || mask >>> n) throw new Error("Invalid archive bytes");
    for (let k = 0; k < n; k++) {
      if (a[i] == null || a[i] > 127) throw new Error("Short archive chunk");
      out[j++] = a[i++] | (mask >>> k & 1) << 7;
    }
  }
  if (i !== a.length) throw new Error("Trailing archive bytes");
  return out;
}
export function bkCrc(bytes) {
  let c = 0xffffffff;
  for (const b of bytes) {
    c ^= b;
    for (let k = 0; k < 8; k++) c = c >>> 1 ^ (c & 1 ? 0xedb88320 : 0);
  }
  return (~c) >>> 0;
}
function bkCheck(rc) {
  if (rc) throw new Error(({ 1: "Invalid archive object", 2: "Archive data failed validation", 3: "Stop playback first", 4: "Flash write failed", 5: "Start a fresh backup" })[rc] || `Archive error ${rc}`);
}
export function bkManifest(a) {
  const protocol = a[0];
  if (protocol !== 1 && protocol !== 2) throw new Error("Unsupported archive protocol");
  bkCheck(a[1]);
  const ids = protocol === 2 ? BACKUP_IDS_F2 : idsOf(a[2]);
  if (!ids || a[2] !== ids.length || a.length !== 3 + a[2] * 11)
    throw new Error("Incomplete archive manifest");
  const objects = ids.map((id, i) => {
    const p = 3 + i * 11;
    if (a[p] !== id) throw new Error("Unexpected archive object");
    const size = bkR32(a, p + 1), crc = bkR32(a, p + 6);
    const max = protocol === 2 ? (id === 1 ? 3840 : id >= 6 ? 3080 : 6144) :
      (id >= 32 ? 81920 : (id === 0 || id >= 2 && id <= 5) ? 10000 : 3840);
    if (size > max || (!size && crc) ||
        (protocol === 2 && ((id === 0 && size !== 6144) ||
          (id >= 2 && id <= 5 && size !== 0 && size !== 6144) ||
          (id >= 6 && size !== 0 && size !== 3080))))
      throw new Error("Archive object size does not match firmware format");
    return { id, size, crc };
  });
  Object.defineProperty(objects, "protocol", { value: protocol });
  return objects;
}
const bkBase64 = (bytes) => { let s = ""; for (const b of bytes) s += String.fromCharCode(b); return btoa(s); };
const fnv = (bytes) => bytes.reduce((v,b) => Math.imul(v ^ b, 16777619) >>> 0, 2166136261);
export function validateFormat2Object(id, bytes) {
  if (id !== 0 && !bytes.length) return;
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (id >= 6) {
    if (bytes.length !== 3080 || v.getUint32(0,true) !== 0x32425055 ||
        v.getUint16(4,true) !== 192 || v.getUint16(6,true) !== 16)
      throw new Error("Invalid FORMAT 2 preset bank");
  } else if (id === 0 || id >= 2) {
    if (bytes.length !== 6144 || v.getUint32(0,true) !== 0x324d5048 ||
        v.getUint32(4,true) !== 6 || v.getUint32(8,true) !== 6144 ||
        fnv(bytes.subarray(32)) !== v.getUint32(16,true) ||
        fnv(bytes.subarray(0,20)) !== v.getUint32(20,true))
      throw new Error("Invalid HPM2 v6 capsule");
    const n = v.getUint32(24,true), rawLength = v.getUint32(28,true);
    if (!n || n > 3328 || rawLength !== 5610) throw new Error("Invalid FORMAT 2 snapshot length");
    const src = bytes.subarray(6144-3328,6144-3328+n), raw = new Uint8Array(rawLength);
    let at = 0, out = 0;
    while (at < src.length) {
      const tag = src[at++], count = tag < 128 ? tag + 1 : tag - 128 + 3;
      if (out + count > raw.length) throw new Error("Oversize FORMAT 2 snapshot");
      if (tag < 128) {
        if (at + count > src.length) throw new Error("Short FORMAT 2 literal");
        raw.set(src.subarray(at,at+count),out); at += count; out += count;
      } else {
        if (at + 2 > src.length) throw new Error("Short FORMAT 2 match");
        const distance = src[at++] | src[at++] << 8;
        if (!distance || distance > Math.min(out,4096)) throw new Error("Invalid FORMAT 2 match");
        for (let i=0;i<count;i++,out++) raw[out] = raw[out-distance];
      }
    }
    if (out !== raw.length) throw new Error("Short FORMAT 2 snapshot");
  }
}
export function readBackup(file) {
  if (typeof file === "string") file = JSON.parse(file);
  const generation = file?.format === "CaravelBackup2" && file.version === 2 ? 2 :
    file?.format === "felucca-backup" && file.version === 1 ? 1 : 0;
  const ids = generation && Array.isArray(file.objects) ?
    (generation === 2 ? FORMAT2_BACKUP_IDS : idsOf(file.objects.length)) : null;
  if (!ids || ids.length !== file.objects.length) throw new Error("Not a complete matching-generation backup");
  const objects = file.objects.map((o, i) => {
    if (!o || o.id !== ids[i] || !Number.isInteger(o.size) || o.size < 0 || o.size > (generation === 2 ? (o.id === 1 ? 3840 : o.id >= 6 ? 3080 : 6144) : (o.id >= 32 ? 81920 : (o.id === 0 || o.id >= 2 && o.id <= 5) ? 10000 : 3840)) ||
        !Number.isInteger(o.crc) || o.crc < 0 || o.crc > 0xffffffff || typeof o.data !== "string" ||
        o.data.length !== 4 * Math.ceil(o.size / 3) || !/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(o.data))
      throw new Error("Invalid backup object");
    const bytes = Uint8Array.from(atob(o.data), (c) => c.charCodeAt(0));
    if (bytes.length !== o.size || bkCrc(bytes) !== o.crc) throw new Error("Backup checksum mismatch");
    if (generation === 2) validateFormat2Object(o.id, bytes);
    if (o.id >= 32 && o.size) {
      if (o.size < 512) throw new Error("Short sample backup");
      const view = new DataView(bytes.buffer), length = view.getUint32(16, true), count = bytes[6];
      if (view.getUint32(0, true) !== 0x504d5346 || view.getUint16(4, true) !== 1 || !count || count > 16 ||
          length !== o.size - 512 || bkCrc(bytes.subarray(512)) !== view.getUint32(20, true)) throw new Error("Invalid sample backup");
      for (let z = 0; z < count; z++) {
        const off = 32 + z * 28, start = view.getUint32(off,true), n = view.getUint32(off+4,true),
          ls = view.getUint32(off+8,true), le = view.getUint32(off+12,true), rate = view.getUint32(off+16,true);
        if (!n || start > length || Math.ceil(n/2) > length - start || n > 163840 || ls > le || le >= n ||
            !rate || rate > 262144 || bytes[off+24] > 88 || bytes[off+25] > bytes[off+26]) throw new Error("Invalid sample zone");
      }
    }
    return { ...o, bytes };
  });
  if (!objects[0].size || !objects[1].size) throw new Error("Backup is missing the current music or settings");
  return { ...file, generation, objects };
}
export function classicEditorBackup(file) {
  const archive = readBackup(file);
  if (archive.generation !== 1) throw new Error("The Classic editor needs a Classic backup");
  // The frozen editor accepts this original JSON schema only. Never serialize
  // parsed typed arrays or ZIP directory fields into its restore file.
  return {
    format:"felucca-backup", version:1,
    firmware:archive.firmware || "CARAVEL v0.13.0",
    created:archive.created || new Date().toISOString(),
    objects:archive.objects.map(({id,size,crc,data})=>({id,size,crc,data}))
  };
}
// The CLI writes ZIP CaravelBackup1/2; the web capture writes JSON. Inspect the
// real ZIP directory, checksums and manifest instead of searching for text markers.
export async function readBackupFile(input) {
  const bytes = input instanceof Uint8Array ? input : new Uint8Array(input);
  if (bytes.length > 2 * 1024 * 1024) throw new Error("Backup file too large");
  const decode = b => new TextDecoder("utf-8", {fatal:true}).decode(b);
  if (bytes[0] !== 0x50 || bytes[1] !== 0x4b) return readBackup(decode(bytes));
  const v = new DataView(bytes.buffer,bytes.byteOffset,bytes.length);
  const u16 = p => v.getUint16(p,true), u32 = p => v.getUint32(p,true);
  let end = bytes.length - 22;
  while (end >= Math.max(0,bytes.length-65557) && u32(end) !== 0x06054b50) end--;
  if (end < Math.max(0,bytes.length-65557) || end + 22 + u16(end+20) !== bytes.length || u16(end+4) || u16(end+6) ||
      u16(end+8) !== u16(end+10) || !u16(end+10) || u16(end+10) > 32 ||
      u32(end+16) + u32(end+12) !== end) throw new Error("Invalid backup ZIP directory");
  const files = new Map(); let at = u32(end+16), total = 0;
  for (let i=0;i<u16(end+10);i++) {
    if (at + 46 > end || u32(at) !== 0x02014b50) throw new Error("Invalid ZIP entry");
    const flags=u16(at+8), method=u16(at+10), crc=u32(at+16), packed=u32(at+20), size=u32(at+24);
    const n=u16(at+28), extra=u16(at+30), comment=u16(at+32), local=u32(at+42);
    if (at+46+n+extra+comment > end || (flags & ~0x800) || ![0,8].includes(method) ||
        (u32(at+38) >>> 16 & 0xf000) === 0xa000 || (total += size) > 1024*1024)
      throw new Error("Unsupported or oversized ZIP entry");
    const name=decode(bytes.subarray(at+46,at+46+n));
    if (!/^(?:manifest\.json|[0-9]{2}-[A-Za-z0-9-]+\.bin)$/.test(name) || files.has(name) ||
        local+30 > u32(end+16) || u32(local) !== 0x04034b50 ||
        u16(local+6) !== flags || u16(local+8) !== method || u32(local+14) !== crc ||
        u32(local+18) !== packed || u32(local+22) !== size || u16(local+26) !== n)
      throw new Error("Invalid or duplicate ZIP object");
    const start=local+30+n+u16(local+28);
    if (decode(bytes.subarray(local+30,local+30+n)) !== name || start+packed > u32(end+16))
      throw new Error("ZIP local entry mismatch");
    let payload=bytes.subarray(start,start+packed);
    if (method === 8) {
      const reader = new Blob([payload]).stream().pipeThrough(new DecompressionStream("deflate-raw")).getReader();
      const chunks=[]; let count=0;
      try {
        for (;;) {
          const {done,value}=await reader.read(); if (done) break;
          if ((count+=value.length)>size) throw new Error("ZIP expands beyond declared size");
          chunks.push(value);
        }
      } finally { await reader.cancel().catch(()=>{}); }
      payload=new Uint8Array(count); let offset=0;
      for (const chunk of chunks) { payload.set(chunk,offset); offset+=chunk.length; }
    }
    if (payload.length !== size || bkCrc(payload) !== crc) throw new Error("ZIP object checksum mismatch");
    files.set(name,payload); at+=46+n+extra+comment;
  }
  if (at !== end || !files.has("manifest.json")) throw new Error("Incomplete backup ZIP");
  const manifest=JSON.parse(decode(files.get("manifest.json")));
  const generation=manifest.format === "CaravelBackup2" ? 2 : manifest.format === "CaravelBackup1" ? 1 : 0;
  const ids=generation === 2 ? FORMAT2_BACKUP_IDS : BACKUP_IDS;
  if (!generation || !Array.isArray(manifest.objects) || manifest.objects.length !== ids.length ||
      files.size !== ids.length+1) throw new Error("Wrong backup ZIP generation/inventory");
  const objects=manifest.objects.map((o,i)=>{
    if (o.id !== ids[i] || !files.has(o.file)) throw new Error("Missing backup ZIP object");
    return {...o,data:bkBase64(files.get(o.file))};
  });
  return readBackup({format:generation === 2 ? "CaravelBackup2" : "felucca-backup",version:generation,objects});
}
export async function captureBackup(request, firmware, onProgress = () => {}) {
  const reply = await request([BACKUP_CMD.LIST, []], { timeout: 3000, retries: 0 });
  const manifest = bkManifest(reply), generation = reply[0];
  const objects = [], total = manifest.reduce((n, o) => n + o.size, 0); let done = 0;
  for (const o of manifest) {
    const bytes = new Uint8Array(o.size);
    for (let off = 0; off < o.size; off += BACKUP_CHUNK) {
      const size = Math.min(BACKUP_CHUNK, o.size - off);
      const a = await request([BACKUP_CMD.GET, [o.id, ...bkU32(off), size & 127, size >>> 7]], { timeout: 1000, retries: 1 });
      bkCheck(a[1]);
      if (a[0] !== o.id || bkR32(a, 2) !== off || (a[7] | a[8] << 7) !== size) throw new Error("Unexpected backup reply");
      bytes.set(bkUnpack(a.slice(9), size), off);
      done += size; onProgress(done, total);
    }
    if (bkCrc(bytes) !== o.crc) throw new Error("The device changed during backup. Try again while stopped.");
    objects.push({ ...o, data: bkBase64(bytes) });
  }
  const file = { format: generation === 2 ? "CaravelBackup2" : "felucca-backup", version: generation,
    firmware, created: new Date().toISOString(), objects };
  readBackup(file); return file;
}
export async function restoreBackup(request, file, onProgress = () => {}) {
  const archive = readBackup(file); // Validate every byte before the first destructive request.
  const reply = await request([BACKUP_CMD.LIST, []], { timeout: 3000, retries: 0 });
  bkManifest(reply);
  if (reply[0] !== archive.generation) throw new Error("Backup and device generations differ; nothing written");
  const total = archive.objects.reduce((n, o) => n + o.size, 0); let done = 0;
  const ask = async (r, o = {}) => request(r, { timeout: 4000, retries: 0, ...o });
  const put = async (args) => { const a = await ask([BACKUP_CMD.PUT, args]); bkCheck(a[2]); return a; };
  // Restore live music last. Other objects commit individually; a disconnect can leave a partial restore.
  for (const o of [...archive.objects.slice(2), archive.objects[1], archive.objects[0]]) {
    if (o.id >= 32) {
      const slot = o.id - 32;
      const check = (a) => { if (a[0] !== slot) throw new Error("Unexpected sample reply"); bkCheck(a.at(-1)); };
      if (!o.size) check(await ask([14, [slot]]));
      else {
        if (o.size < 512) throw new Error("Short sample backup");
        check(await ask([11, [slot]]));
        for (let off = 512; off < o.size; off += BACKUP_CHUNK) {
          const chunk = o.bytes.subarray(off, off + BACKUP_CHUNK);
          check(await ask([12, [slot, off & 127, off >>> 7 & 127, off >>> 14 & 127, ...bkPack(chunk)]]));
          done += chunk.length; onProgress(done, total);
        }
        check(await ask([13, [slot, ...bkPack(o.bytes.subarray(0, 480))]]));
        done += 512; onProgress(done, total);
      }
    } else {
      if (o.id === 9) {                    // firmware before 1.0.3 does not take id 9 (rc 1 at begin, nothing written): skip it
        const a = await ask([BACKUP_CMD.PUT, [0, o.id, ...bkU32(o.size), ...bkU32(o.crc)]]);
        if (a[2] === 1) { done += o.size; onProgress(done, total); continue; }
        bkCheck(a[2]);
      } else await put([0, o.id, ...bkU32(o.size), ...bkU32(o.crc)]);
      try {
        for (let off = 0; off < o.size; off += BACKUP_CHUNK) {
          const chunk = o.bytes.subarray(off, off + BACKUP_CHUNK);
          await put([1, o.id, ...bkU32(off), ...bkPack(chunk)]);
          done += chunk.length; onProgress(done, total);
        }
        await put([2, o.id]);
      } catch (e) { await put([3, o.id]).catch(() => {}); throw e; }
    }
  }
  return archive;
}

// Installer connection: no editor page or watcher owns this input simultaneously.
export class BackupConnection {
  constructor(input, output) {
    this.input = input; this.output = output; this.pending = null; this.header = V1_HEADER;
    input.onmidimessage = (e) => {
      const d = e.data;
      if (d.length < 6 || d[0] !== 240 || !this.pending ||
          !this.pending.header.every((v,i) => d[i+1] === v) || d.at(-1) !== 247) return;
      if (this.pending?.cmd === d[4]) { const p = this.pending; this.pending = null; clearTimeout(p.timer); p.resolve(Array.from(d.slice(5, -1))); }
    };
  }
  async request([cmd, args], opt = {}) {
    if (this.updateOnly && ![1,65,66].includes(cmd))
      throw new Error("Older 1.x development firmware is read-only; use the update installer");
    if (this.pending || this.closed || this.input.state === "disconnected" || this.output.state === "disconnected") throw new Error("Backup connection closed or busy");
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => { this.pending = null; reject(new Error("Backup timed out. Keep the FM-1 connected and stopped.")); }, opt.timeout || 1200);
      const header = opt.header || this.header;
      this.pending = { cmd, timer, resolve, reject, header };
      try { this.output.send([240, ...header, cmd, ...args, 247]); }
      catch (e) { clearTimeout(timer); this.pending = null; reject(e); }
    });
  }
  async identify({allowLegacyFormat2Update = false} = {}) {
    const info = await detectGeneration((header,cmd,args) =>
      this.request([cmd,args], {header, timeout:700}), {allowLegacyFormat2Update});
    this.header = info.header;
    this.updateOnly = !!info.updateOnly;
    return info;
  }
  close() {
    this.closed = true; this.input.onmidimessage = null;
    if (this.pending) { const p = this.pending; this.pending = null; clearTimeout(p.timer); p.reject(new Error("Backup connection closed")); }
  }
}
