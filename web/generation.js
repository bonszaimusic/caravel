// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 bonszaimusic
// Read-only generation discovery. Names and package text never authorize access.
export const V1_HEADER = [0x7d, 0x43, 0x41];
export const CLASSIC_HEADER = [0x7d, 0x46, 0x4c];
export const V1_PROTOCOL = 18;
const ENGINES = ["ANALOG", "PHYS", "SEM", "HADES", "DRONE", "ENSEMBLE"];

export function generationInfo(a) {
  if (!Array.isArray(a) || a.some(v => !Number.isInteger(v) || v < 0 || v > 127))
    throw new Error("Invalid generation INFO");
  let at = 0;
  const byte = () => { if (at >= a.length) throw new Error("Short generation INFO"); return a[at++]; };
  const string = () => { let s = "", b; while ((b = byte())) s += String.fromCharCode(b); return s; };
  const version = string(), n = byte();
  const counts = [byte(), byte(), byte(), byte()];
  if (!n || n > 32 || !counts[0] || !counts[1]) throw new Error("Invalid generation counts");
  const engines = Array.from({length:n}, string);
  if (byte() !== 4) throw new Error("Unsupported device track protocol");
  const rows = byte(), tags = new Map();
  while (at < a.length) {
    const tag = byte(), rev = byte();
    if (tags.has(tag)) throw new Error("Duplicate generation capability");
    const width = tag === 0x4d || tag === 0x46 || tag === 0x41 ? 2 : 1;
    tags.set(tag, [rev, ...Array.from({length:width}, byte)]);
  }
  const format = tags.get(0x60);
  if (format) {
    if (rows !== 0 || format[0] !== 2 || format[1] !== V1_PROTOCOL ||
        engines.join("|") !== ENGINES.join("|")) throw new Error("Unsupported 1.x protocol revision");
    return {generation:2, protocol:V1_PROTOCOL, version};
  }
  if (rows !== 16 || tags.get(0x55)?.[0] !== 1 || tags.get(0x42)?.[0] !== 1)
    throw new Error("Unsupported Classic protocol");
  return {generation:1, protocol:"Classic capabilities / LIST 1", version};
}

export async function detectGeneration(request) {
  // request(header, command, args): strictly read-only here, before any mutation.
  let info, header;
  try {
    info = generationInfo(await request(V1_HEADER, 1, []));
    if (info.generation !== 2) throw new Error("Wrong generation on 1.x transport");
    header = V1_HEADER;
  } catch (modernError) {
    const legacy = await request(CLASSIC_HEADER, 1, []);
    // A running 1.x rejects the old framing explicitly; never treat it as Classic.
    if (legacy.length === 4 && legacy[0] === 0 && legacy[1] === 0x60)
      throw new Error("1.x generation handshake failed; reconnect using the matching 1.x editor");
    info = generationInfo(legacy);
    if (info.generation !== 1) throw modernError;
    header = CLASSIC_HEADER;
  }
  const catalog = await request(header, 65, []);
  if (catalog[0] !== info.generation || catalog[1] !== 0 ||
      catalog.length !== 3 + catalog[2] * 11)
    throw new Error("Device INFO / backup LIST generation mismatch (stop playback and retry)");
  const ids = Array.from({length:catalog[2]}, (_, i) => catalog[3 + i * 11]);
  const expected = info.generation === 2 ? [0,1,2,3,4,5,6,7] : [0,1,2,3,4,5,6,7,8,9,32,33,34];
  if (ids.join() !== expected.join()) throw new Error("Unsupported device backup inventory");
  return {...info, header, catalog};
}

export function updateEntry(generation) {
  if (generation === 1) return [0xf0,0x22,0x24,0x35,0x7f,0xf7];
  if (generation === 2) return [0xf0,...V1_HEADER,0x7f,V1_PROTOCOL,2,0xf7];
  throw new Error("Unknown device generation; update refused");
}

export function requireClassicReturnBackup(archive) {
  if (archive?.generation !== 1) throw new Error("Choose the Classic backup saved before upgrading");
  for (const o of archive.objects.filter(o => o.id === 0 || o.id >= 2 && o.id <= 5)) {
    if (o.id !== 0 && !o.bytes.length) continue;
    const v = new DataView(o.bytes.buffer,o.bytes.byteOffset,o.bytes.byteLength);
    if (o.bytes.length !== 7200 || v.getUint32(0,true) !== 0x314d5048 ||
        ![4,5].includes(v.getUint32(4,true)) || v.getUint32(8,true) !== 7200)
      throw new Error("A complete Classic 0.13 backup is required for this return");
    const fnv = b => b.reduce((h,x)=>Math.imul(h^x,16777619)>>>0,2166136261);
    const n=v.getUint32(24,true), length=v.getUint32(28,true);
    if (fnv(o.bytes.subarray(32)) !== v.getUint32(16,true) ||
        fnv(o.bytes.subarray(0,20)) !== v.getUint32(20,true) || !n || n>3328 || length!==5594)
      throw new Error("Classic capsule checksum/snapshot mismatch");
    const packed=o.bytes.subarray(3872,3872+n); let at=0,out=0;
    while(at<packed.length) {
      const tag=packed[at++], count=tag<128?tag+1:tag-128+3;
      if(out+count>length) throw new Error("Oversize Classic snapshot");
      if(tag<128) {
        if(at+count>packed.length)throw new Error("Short Classic snapshot literal");
        at+=count;
      } else {
        if(at+2>packed.length)throw new Error("Short Classic snapshot match");
        const distance=packed[at++]|packed[at++]<<8;
        if(!distance||distance>Math.min(out,4096))throw new Error("Invalid Classic snapshot match");
      }
      out+=count;
    }
    if(out!==length)throw new Error("Short Classic snapshot");
  }
  return archive;
}
