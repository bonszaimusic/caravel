// SPDX-License-Identifier: GPL-3.0-only
// Screenshots of the browser build in Node: the firmware's framebuffer (C code through WASM), not a browser screenshot.
//   node web/emu/shots.mjs build/emu/felucca.wasm OUTDIR script.json
// script.json: [ ["tap","ENV"], ["hold","HOME",800], ["turn","K1",3], ["keys",[7]], ["wait",200], ["shot","name"], ... ]
import fs from "fs";
import zlib from "zlib";
const [wasmPath, outDir, scriptPath] = process.argv.slice(2);
const B = { FX: 0, SCL: 1, SEL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12, OCTUP: 13 };
const EN = { SELECT: 0, ALGO: 1, PRESETS: 2, K1: 3, K2: 4, K3: 5, K4: 6 };
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports, mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot(); if (ex.web_test_build) ex.web_test_build();
const render = (ms) => { const n = Math.round(ms * 44.1); for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
const mask = (names) => names.reduce((m, n) => m | (1 << B[n]), 0);
function png(path, fb) {
  const W = 240, H = 240, raw = Buffer.alloc((W * 3 + 1) * H);
  for (let y = 0; y < H; y++) { raw[y * (W * 3 + 1)] = 0; for (let x = 0; x < W; x++) { const q = fb[y * W + x], p = ((q & 255) << 8) | (q >> 8), o = y * (W * 3 + 1) + 1 + x * 3;
    raw[o] = ((p >> 11) & 31) * 255 / 31 | 0; raw[o + 1] = ((p >> 5) & 63) * 255 / 63 | 0; raw[o + 2] = (p & 31) * 255 / 31 | 0; } }
  const crcT = new Int32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; return c; });
  const crc = (b) => { let c = -1; for (const x of b) c = crcT[(c ^ x) & 255] ^ (c >>> 8); return (c ^ -1) >>> 0; };
  const chunk = (t, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(H, 4); ihdr[8] = 8; ihdr[9] = 2;
  fs.writeFileSync(path, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr), chunk("IDAT", zlib.deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
}
fs.mkdirSync(outDir, { recursive: true });
render(2500);
for (const st of JSON.parse(fs.readFileSync(scriptPath, "utf8"))) {
  const [op, a, b] = st;
  if (op === "tap") { ex.web_buttons(mask([].concat(a))); render(70); ex.web_buttons(0); render(90); }
  else if (op === "hold") { ex.web_buttons(mask([].concat(a))); render(b || 800); }
  else if (op === "release") { ex.web_buttons(0); render(a || 120); }
  else if (op === "dtap") { for (let i = 0; i < 2; i++) { ex.web_buttons(mask([].concat(a))); render(50); ex.web_buttons(0); render(80); } }
  else if (op === "turn") { for (let i = 0; i < Math.abs(b); i++) { ex.web_enc(EN[a], Math.sign(b)); render(40); } render(60); }
  else if (op === "keys") { ex.web_keys(a.reduce((m, k) => m | (1 << k), 0)); render(b || 200); }
  else if (op === "keysoff") { ex.web_keys(0); render(a || 100); }
  else if (op === "wait") render(a);
  else if (op === "eng") { ex.web_test_engine(a, b || 0); render(300); }   // a track 0 engine (and its preset), for screens of every engine
  else if (op === "midi") { ex.web_midi(a[0], a[1], a[2]); render(b || 20); }
  else if (op === "ownfilter") { ex.web_test_filter_own(a); render(200); }
  else if (op === "shot") { render(120); png(`${outDir}/${a}.png`, new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240)); }
}
console.log("shots done");
