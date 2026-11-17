// SPDX-License-Identifier: GPL-3.0-only
// CAPTURE device plan C-b: the display reads a snapshot and derives the loop's band notes from the loop (no cap_dring).
//   node web/emu/capture_view_test.mjs BEFORE.wasm AFTER.wasm      (both built with -DCARAVEL_UX_CAPTURE=1)
// The same keys and buttons go to both builds; the screens (the firmware's framebuffer) are compared at the same moments:
//   LIVE (no loop): every frame equal. LOOP: everything outside the band (88..152 x 96..136) equal; inside, the band's own
//   notes now come from the loop (as captured, not as HARMONY/CHRD made them) - reported, and the loop's notes only differ
//   in the band; the ring, pointer, header and chips stay exactly as before.
import fs from "fs";
const [pa, pb] = [process.argv[2], process.argv[3]];
const B = { HOME: 6, PLAY: 10, REC: 11, SEQ: 9 };
let fails = 0;
const check = (what, ok) => { console.log(`view: ${what.padEnd(86)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
async function device(path) {
  const { instance } = await WebAssembly.instantiate(fs.readFileSync(path), {});
  const ex = instance.exports, mem = ex.memory;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot(); if (ex.web_test_build) ex.web_test_build();
  const d = { ex };
  d.render = (ms) => { const n = Math.round(ms * 44.1); for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
  d.tap = (b) => { ex.web_buttons(1 << B[b]); d.render(70); ex.web_buttons(0); d.render(90); };
  d.hold = (b, ms) => { ex.web_buttons(1 << B[b]); d.render(ms); ex.web_buttons(0); d.render(150); };
  d.screen = () => new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240).slice();
  return d;
}
const inBand = (i) => { const x = i % 240, y = (i / 240) | 0; return x >= 86 && x < 154 && y >= 94 && y < 138; };
const diffs = (a, b) => { let out = 0, inb = 0; for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) inBand(i) ? inb++ : out++; return { out, inb }; };
const A = await device(pa), N = await device(pb);
const both = (f) => { f(A); f(N); };
both((d) => { d.render(2500); d.hold("HOME", 1100); d.render(3500); d.tap("PLAY"); d.render(300); });
// LIVE: notes played, the band scrolls, no loop yet
// (C-c: event times are 1/4096 bar now, not 1/65536: a note's edge in the band may move one pixel, nothing else changes)
let worstOut = 0, worstIn = 0;
for (const key of [7, 12, 15]) {
  both((d) => { d.ex.web_keys(1 << key); d.render(250); d.ex.web_keys(0); d.render(300); });
  const x = diffs(A.screen(), N.screen()); worstOut = Math.max(worstOut, x.out); worstIn = Math.max(worstIn, x.inb);
}
both((d) => d.render(1200));
{ const x = diffs(A.screen(), N.screen()); worstOut = Math.max(worstOut, x.out); worstIn = Math.max(worstIn, x.inb); }
console.log(`view: LIVE: at most ${worstOut} px differ outside the band, ${worstIn} inside it (an edge moved by a pixel)`);
check("LIVE (three notes, no loop): nothing outside the band differs", worstOut === 0);
check("LIVE: inside the band only edges of notes (a few pixels, never a different picture)", worstIn < 40);
// LOOP: REC, then watch it run
both((d) => { d.tap("REC"); d.render(300); });
let outs = 0, ins = 0, frames = 0;
for (let k = 0; k < 12; k++) {
  both((d) => d.render(380));
  const x = diffs(A.screen(), N.screen()); outs += x.out; ins += x.inb; frames++;
}
console.log(`view: LOOP running, ${frames} frames: ${outs} px differ outside the band, ${ins} px inside it (the loop's own notes)`);
check("LOOP: the ring, the pointer, the header and the chips exactly as before (0 px outside the band)", outs === 0);
check("LOOP: inside the band only the loop's own notes differ (far from a different picture)", ins > 0 && ins < frames * 400);
both((d) => d.tap("SEQ"));
both((d) => d.render(800));
{ const x = diffs(A.screen(), N.screen()); console.log(`view: LOOP paused: ${x.out} outside, ${x.inb} inside`); check("LOOP paused: outside the band equal", x.out === 0); }
both((d) => { d.hold("SEQ", 900); d.render(600); });
{ const x = diffs(A.screen(), N.screen()); check("WIPE: the whole screen equal to before", x.out + x.inb === 0); }
console.log(fails ? `view: ${fails} failed` : "view: all passed");
process.exit(fails ? 1 : 0);
