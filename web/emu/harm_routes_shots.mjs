// SPDX-License-Identifier: GPL-3.0-only
// HARM routing UI (BUILD orbit centre): real 240x240 framebuffer PNGs through the firmware WASM.
//   node web/emu/harm_routes_shots.mjs build/emu/felucca.wasm OUTDIR
import fs from "fs";
import zlib from "zlib";
const [wasmPath, outDir] = process.argv.slice(2);
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11 };
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports, mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot(); ex.web_test_build();
const render = (ms) => { const n = Math.round(ms * 44.1); for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
const tap = (b) => { ex.web_buttons(1 << B[b]); render(70); ex.web_buttons(0); render(90); };
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
const shot = (name) => { render(150); png(`${outDir}/${name}.png`, new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240)); };
render(2500);
const R = { NOTES: 0, FOLLOW: 1, CHORD: 2, EXT: 3 };
const prog = () => { [[0, 0], [9, 1], [5, 0], [7, 2]].forEach(([r, d], i) => ex.web_test_harm_prog(i, r, d)); };
const role = (t, r) => ex.web_test_harm_role(t, r);
// scenario: [name, setup(), selected track, SCL pages to shoot]
const scen = [
  ["follow_on",            () => { ex.web_test_harm_on(1); for (let t = 0; t < 4; t++) role(t, R.NOTES); role(1, R.FOLLOW); }, 0, 1],
  ["follow_bus_off_dim",   () => { ex.web_test_harm_on(0); for (let t = 0; t < 4; t++) role(t, R.NOTES); role(1, R.FOLLOW); }, 0, 1],
  ["chord_from_harm_on",   () => { ex.web_test_harm_on(1); for (let t = 0; t < 4; t++) role(t, R.NOTES); ex.web_test_harm_bus(1); }, 0, 1],
  ["chord_from_harm_off",  () => { ex.web_test_harm_on(0); for (let t = 0; t < 4; t++) role(t, R.NOTES); ex.web_test_harm_bus(1); }, 0, 1],
  ["ext_bus_ready",        () => { ex.web_test_harm_on(1); for (let t = 0; t < 4; t++) role(t, R.NOTES); ex.web_test_harm_csrc(1, 1); }, 0, 1],
  ["ext_bus_off_wait",     () => { ex.web_test_harm_on(0); for (let t = 0; t < 4; t++) role(t, R.NOTES); ex.web_test_harm_csrc(1, 1); }, 0, 1],
  ["ext_t1_wait",          () => { ex.web_test_harm_on(1); for (let t = 0; t < 4; t++) role(t, R.NOTES); ex.web_test_harm_csrc(1, 2); }, 0, 1],
  ["key_from_t1",          () => { ex.web_test_harm_on(0); ex.web_test_harm_key_mode(0); for (let t = 0; t < 4; t++) role(t, R.NOTES); ex.web_test_harm_link(1, 1); }, 1, 4],
  ["key_from_t1_two",      () => { ex.web_test_harm_on(0); ex.web_test_harm_key_mode(0); for (let t = 0; t < 4; t++) role(t, R.NOTES); ex.web_test_harm_link(1, 1); ex.web_test_harm_link(2, 1); ex.web_test_harm_link(3, 2); }, 2, 4],
  ["chord_local_from_key", () => { ex.web_test_harm_on(1); ex.web_test_harm_key_mode(0); for (let t = 0; t < 4; t++) role(t, R.CHORD); ex.web_test_harm_link(1, 1); ex.web_test_harm_link(2, 1); }, 1, 4],
  ["follow_playing_bar2",  () => { prog(); ex.web_test_harm_on(1); for (let t = 0; t < 4; t++) role(t, R.NOTES); role(1, R.FOLLOW); tap("PLAY"); render(5200); }, 0, 1],
  ["multi_targets",        () => { ex.web_test_harm_on(1); for (let t = 0; t < 4; t++) role(t, R.NOTES); role(0, R.FOLLOW); ex.web_test_harm_bus(1); ex.web_test_harm_csrc(2, 1); ex.web_test_harm_csrc(3, 3); }, 0, 1],
];
for (const [name, setup, sel, pages] of scen) {
  ex.web_test_harm_reset(); setup(); ex.web_test_select(sel); render(300);
  tap("HOME"); render(300);
  for (let k = 1; k <= pages; k++) { tap("SCL"); render(400); shot(`${name}_p${k}`); }
  tap("HOME"); render(200);
}
// track switching on the multi-target setup
ex.web_test_harm_reset(); scen[scen.length - 1][1]();
for (let sel = 0; sel < 4; sel++) { ex.web_test_select(sel); render(300); tap("HOME"); render(300); tap("SCL"); render(400); shot(`multi_targets_sel_T${sel + 1}`); tap("HOME"); }
console.log("done");
