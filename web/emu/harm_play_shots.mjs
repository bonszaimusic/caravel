// SPDX-License-Identifier: GPL-3.0-only
// HARM in PLAY: real 240x240 framebuffer PNGs through the firmware WASM.
//   node web/emu/harm_play_shots.mjs build/emu/felucca.wasm OUTDIR   (PLAY: the HARM area with the harmony position)
import fs from "fs";
import zlib from "zlib";
const [wasmPath, outDir] = process.argv.slice(2);
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11 };
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports, mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot(); 
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
const hold = (b, ms) => { ex.web_buttons(1 << B[b]); render(ms); ex.web_buttons(0); render(200); };
const R = { NOTES: 0, FOLLOW: 1 };
const prog = () => { [[0, 0], [9, 1], [5, 0], [7, 2]].forEach(([r, d], i) => ex.web_test_harm_prog(i, r, d)); };   // C 2 bars, Am 4, F 2, G 8... (dur code 0..3 = 1/2/4/8 bars)
const go = (name, setup, ms) => { ex.web_test_harm_reset(); setup(); ex.web_test_select(0); render(300); tap("SCL"); render(400); if (ms) { tap("PLAY"); render(ms); } shot(name); if (ms) { tap("PLAY"); render(300); } };
go("play_harm_off_notes", () => { ex.web_test_harm_role(0, R.NOTES); }, 0);
go("play_harm_follow_stopped", () => { ex.web_test_harm_on(1); ex.web_test_harm_role(0, R.FOLLOW); }, 0);
go("play_harm_follow_bar1", () => { prog(); ex.web_test_harm_on(1); ex.web_test_harm_role(0, R.FOLLOW); }, 1500);
go("play_harm_follow_bar3", () => { prog(); ex.web_test_harm_on(1); ex.web_test_harm_role(0, R.FOLLOW); }, 5200);
go("play_harm_follow_bar5", () => { prog(); ex.web_test_harm_on(1); ex.web_test_harm_role(0, R.FOLLOW); }, 9000);
go("play_harm_notes_own_key", () => { ex.web_test_harm_on(1); ex.web_test_harm_role(0, R.NOTES); }, 0);
// K1/K2 on a FOLLOW track set the HARMONY key (H ROOT / H SCL)
ex.web_test_harm_reset(); ex.web_test_harm_on(1); ex.web_test_harm_role(0, R.FOLLOW); ex.web_test_select(0); render(300);
tap("SCL"); render(300); for (let i = 0; i < 3; i++) { ex.web_enc(3, 1); render(100); } ex.web_enc(4, 1); render(100); shot("play_harm_follow_key_edited");
console.log("done");
