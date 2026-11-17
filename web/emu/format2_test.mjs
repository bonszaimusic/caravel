// SPDX-License-Identifier: GPL-3.0-only
// FORMAT 2 (CARAVEL_FORMAT2): the flash of an older build in the new one, and the other way round.
//   node web/emu/format2_test.mjs OLD.wasm NEW.wasm
// OLD = the same sources with CARAVEL_SLIM=1 but not CARAVEL_FORMAT2 (writes the old generation), NEW = with it.
// Checks: the old build's project, autosave and user preset are old data in the new one (found, not loaded, not
// converted); the notice (locked for 5 s, OCT+ held 3 s goes on, a tap does not); keeping leaves the old bytes as
// they were and protects the slots; erasing asks twice; what the new build writes the old one reads as empty and
// the old data it left comes back with the old build; a new project and preset round-trip; nothing of the notice
// returns once it was answered.
import fs from "fs";
import zlib from "zlib";
import { storeSectors, restoreSectors } from "./worklet.js";

const [oldPath,newPath]=process.argv[3] ? [process.argv[2],process.argv[3]] :
 [new URL("../../tests/fixtures/format1-emulator.wasm.gz",import.meta.url),process.argv[2]||"build/emu/felucca.wasm"];
const oldFile=fs.readFileSync(oldPath);
const oldBytes=oldFile[0]===0x1f&&oldFile[1]===0x8b?zlib.gunzipSync(oldFile):oldFile;
const newBytes=fs.readFileSync(newPath);
const B = { SAVE: 7, OCTDN: 12, OCTUP: 13, HOME: 6, PLAY: 10 };
const EN = { K1: 3 };
let fails = 0;
const check = (what, ok) => { console.log(`f2: ${what.padEnd(84)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };

async function device(bytes, saved) {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports, mem = ex.memory;
  ex._initialize();
  ex.web_nor_erase();
  if (saved) restoreSectors(new Uint8Array(mem.buffer, ex.web_nor(), ex.web_nor_size()), saved);
  ex.web_boot(); if (ex.web_test_build) ex.web_test_build();
  const d = { ex, mem };
  d.render = (ms) => { for (let k = 0; k < Math.round(ms * 44.1); k += 128) ex.web_render(128); };
  d.tap = (b) => { ex.web_buttons(1 << b); d.render(60); ex.web_buttons(0); d.render(60); };
  d.hold = (b, ms) => { ex.web_buttons(1 << b); d.render(ms); ex.web_buttons(0); d.render(120); };
  d.turn = (role, n) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(role, Math.sign(n)); d.render(40); } };
  d.nor = () => new Uint8Array(mem.buffer, ex.web_nor(), ex.web_nor_size());
  d.sectors = () => storeSectors(d.nor());
  d.screen = () => new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240).slice();
  d.f2 = (w) => (ex.web_test_f2 ? ex.web_test_f2(w) : -1);
  return d;
}
// F2_SHOTS=dir: the notice's screens as PNG (the framebuffer, as shots.mjs)
function png(name, fb) {
  const dir = process.env.F2_SHOTS; if (!dir) return;
  fs.mkdirSync(dir, { recursive: true });
  const W = 240, H = 240, raw = Buffer.alloc((W * 3 + 1) * H);
  for (let y = 0; y < H; y++) { raw[y * (W * 3 + 1)] = 0; for (let x = 0; x < W; x++) { const q = fb[y * W + x], p = ((q & 255) << 8) | (q >> 8), o = y * (W * 3 + 1) + 1 + x * 3;
    raw[o] = ((p >> 11) & 31) * 255 / 31 | 0; raw[o + 1] = ((p >> 5) & 63) * 255 / 63 | 0; raw[o + 2] = (p & 31) * 255 / 31 | 0; } }
  const crcT = new Int32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; return c; });
  const crc = (b) => { let c = -1; for (const x of b) c = crcT[(c ^ x) & 255] ^ (c >>> 8); return (c ^ -1) >>> 0; };
  const chunk = (t, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([l, td, c]); };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(H, 4); ihdr[8] = 8; ihdr[9] = 2;
  fs.writeFileSync(`${dir}/${name}.png`, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr), chunk("IDAT", zlib.deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
}
const same = (a, b) => {
  const ka = Object.keys(a), kb = Object.keys(b);
  return ka.length === kb.length && ka.every((k) => b[k] && a[k].length === b[k].length && a[k].every((v, i) => v === b[k][i]));
};
const hash = (a) => { let h = 2166136261; for (const v of a) h = Math.imul(h ^ v, 16777619) >>> 0; return h; };

// ---- the old build writes a project, a user preset and an autosave
const o = await device(oldBytes, null);
o.render(600);
o.ex.web_test_project_save(0);
o.render(300);
o.tap(B.SAVE); o.tap(B.OCTUP); o.tap(B.OCTUP); o.render(200);   // (the old build: SAVE opens USER)
o.turn(EN.K1, 6);
o.render(14000);                                    // the autosave writes after 10 s of quiet
const oldFlash = o.sectors();
check(`the old build kept a project, a preset and an autosave (${Object.keys(oldFlash).length} sectors)`,
      o.ex.web_test_project_used(0) === 1 && o.ex.web_test_user_preset(0) === 1 && Object.keys(oldFlash).length >= 4);
check("(it is the old generation: no tag in the commit record, no FORMAT 2 export)", o.f2(0) === -1);

// ---- the new build finds it
const n = await device(newBytes, oldFlash);
n.render(600);
const mask = n.f2(1);
check(`found at power-on: old mask ${mask.toString(2)} (project A, an autosave, the first preset bank)`,
      (mask & 1) === 1 && (mask & 0x20) === 0x20);
check("the notice is up (stage 1) and the project is not loaded", n.f2(0) === 1 && n.ex.web_test_project_used(0) === 0);
check("the user preset of the old build is not there", n.ex.web_test_user_preset(0) === 0);
const frame1 = hash(n.screen());
png("N1_locked", n.screen());
n.tap(B.OCTUP); n.render(200);
check("a tap on OCT+ does nothing", n.f2(0) === 1);
n.hold(B.OCTUP, 3300);
check("OCT+ held 3 s inside the first 5 s: still locked", n.f2(0) === 1);
n.render(2500);
png("N2_unlocked", n.screen());
n.ex.web_buttons(1 << B.OCTUP); n.render(1700); png("N3_hold", n.screen()); n.ex.web_buttons(0); n.render(120);
n.hold(B.OCTUP, 2000);
check("held only 2 s after the lock: still stage 1", n.f2(0) === 1);
n.hold(B.OCTUP, 3300);
check("held 3 s after the lock: stage 2 (BACKUP MADE?)", n.f2(0) === 2);
png("N4_backup", n.screen());
check("the screen changed with it", hash(n.screen()) !== frame1);
n.tap(B.HOME); n.render(100);
check("any other button on stage 2: back to stage 1", n.f2(0) === 1);
n.hold(B.OCTUP, 3300);
check("... and forward again", n.f2(0) === 2);
n.hold(B.OCTUP, 3300);
check("OCT+ held on stage 2: keep and go on (stage 0, answered)", n.f2(0) === 0 && n.f2(2) === 1);
check("the old project A stays protected", (n.f2(1) & 1) === 1);
n.ex.web_test_project_save(0);
n.render(300);
check("a save to the old project's slot is refused", n.ex.web_test_project_used(0) === 0);
check("... and puts the question up again (stage 2, unlocked)", n.f2(0) === 2);
n.hold(B.OCTUP, 3300);
check("keeping it again: back to the page", n.f2(0) === 0);
n.ex.web_test_project_save(1);
n.render(300);
check("a save to another slot works", n.ex.web_test_project_used(1) === 1);
n.tap(B.SAVE); n.tap(B.SAVE); n.tap(B.OCTUP); n.tap(B.OCTUP); n.render(200);   // (SAVE opens PROJECT; the second tap is USER, SAVE picked)
check("a user preset into the old bank is refused", n.ex.web_test_user_preset(0) === 0);

// ---- the old data is as it was (the new build wrote beside it only)
const keepFlash = n.sectors();
const guarded = Object.keys(oldFlash).filter((k) => (+k >= 0x97000 && +k < 0xE0000) || (+k >= 0xE8000 && +k < 0xF8000));
const untouched = guarded.length >= 2 && guarded.every((k) => keepFlash[k] && keepFlash[k].every((v, i) => v === oldFlash[k][i]));
check("the old bytes of the projects and presets are untouched", untouched);

// ---- rebooting: nothing of the notice, the protection stays
const n2 = await device(newBytes, keepFlash);
n2.render(600);
check("after a reboot: no notice (answered), project A still protected, B loads",
      n2.f2(0) === 0 && (n2.f2(1) & 1) === 1 && n2.ex.web_test_project_used(1) === 1);

// ---- a slot freed by the one who wants it: save -> question -> erase -> save works
n2.ex.web_test_project_save(0); n2.render(300);
check("reboot, save to project A: refused, the question is up", n2.f2(0) === 2 && n2.ex.web_test_project_used(0) === 0);
n2.hold(B.OCTDN, 3300);
check("OCT- held: asks once more", n2.f2(0) === 3);
n2.hold(B.OCTDN, 3300);
check("OCT- held again: all old data gone", n2.f2(0) === 0 && n2.f2(1) === 0);
n2.ex.web_test_project_save(0); n2.render(300);
check("project A can be saved now", n2.ex.web_test_project_used(0) === 1);

// ---- and back to the old firmware: the old data is there, what the new one wrote is not
const back = await device(oldBytes, keepFlash);
back.render(600);
check("the old firmware again: its project A is there", back.ex.web_test_project_used(0) === 1);
check("... and the new build's project B is empty to it (another generation)", back.ex.web_test_project_used(1) === 0);

// ---- erasing asks twice
const e = await device(newBytes, oldFlash);
e.render(600);
e.render(5200);
e.hold(B.OCTUP, 3300);
check("erase path: stage 2", e.f2(0) === 2);
e.hold(B.OCTDN, 3300);
check("OCT- held on stage 2: asks once more (stage 3)", e.f2(0) === 3);
png("N5_erase", e.screen());
e.tap(B.OCTUP); e.render(100);
check("OCT+ on stage 3: back to stage 2", e.f2(0) === 2);
e.hold(B.OCTDN, 3300);
e.hold(B.OCTDN, 3300);
check("OCT- held again: erased, answered", e.f2(0) === 0 && e.f2(1) === 0 && e.f2(2) === 1);
const wiped = e.sectors();
const e2 = await device(newBytes, wiped);
e2.render(600);
check("a reboot after the erase: nothing old, no notice", e2.f2(1) === 0 && e2.f2(0) === 0);
const e3 = await device(oldBytes, wiped);
e3.render(600);
check("the old firmware after the erase: its project is gone", e3.ex.web_test_project_used(0) === 0);

// ---- a new project and preset round-trip in the new build
const r = await device(newBytes, null);
r.render(600);
check("a fresh flash: no notice", r.f2(0) === 0 && r.f2(1) === 0);
r.ex.web_test_project_save(2); r.render(300);
r.tap(B.SAVE); r.tap(B.SAVE); r.tap(B.OCTUP); r.tap(B.OCTUP); r.render(200);   // (SAVE opens PROJECT; the second tap is USER, SAVE picked)
const rt = await device(newBytes, r.sectors());
rt.render(600);
check("project C and user preset 1 round-trip", rt.ex.web_test_project_used(2) === 1 && rt.ex.web_test_user_preset(0) === 1);
check("... with no old data and no notice", rt.f2(1) === 0 && rt.f2(0) === 0);

console.log(fails ? `f2: ${fails} failed` : "f2: all passed");
process.exit(fails ? 1 : 0);
