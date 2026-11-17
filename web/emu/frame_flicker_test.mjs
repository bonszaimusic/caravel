// SPDX-License-Identifier: GPL-3.0-only
// BUILD's outer frame must not flicker: a real panel shows the pixels between two draw calls of one frame, the emulator only the
// finished frame. Built with -DWEB_LCD_TRACE=1 the emulator records, after every lcd_fill / lcd_blit, four probe pixels on the
// 1 px frame (the top-left corner, the middle of the top row, and two on the sides inside the header rows). The frame is lit
// (the same colour as the finished image) after every draw call, so no probe may ever change while the page just stands there.
//   EMU_CFLAGS="-DCARAVEL_UX_CAPTURE=1 -DWEB_LCD_TRACE=1" sh web/emu/build.sh && node web/emu/frame_flicker_test.mjs build/emu/felucca.wasm
import fs from "fs";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm"), {});
const ex = instance.exports, mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot(); if (ex.web_test_build) ex.web_test_build();
const B = { FX: 0, ENV: 2, HOME: 6, SAVE: 7, PLAY: 10 };
let fails = 0;
const check = (what, ok) => { console.log(`flicker: ${what.padEnd(78)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const render = (ms) => { const n = Math.round(ms * 44.1); for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
const tap = (b) => { ex.web_buttons(1 << B[b]); render(70); ex.web_buttons(0); render(160); };
function transients(label) {
  ex.web_trace_reset(); render(1000);
  const n = ex.web_trace_count(), p = new Uint16Array(mem.buffer, ex.web_probe_ptr(), n * 6);
  let flips = 0;
  for (let i = 1; i < n; i++) for (let k = 0; k < 6; k++) if (p[i * 6 + k] !== p[(i - 1) * 6 + k]) flips++;
  console.log(`flicker: ${label}: ${n} draw calls in 1 s, ${flips} changes of the frame's probe pixels between draw calls`);
  return flips;
}
render(3000);
check("BUILD overview, standing still: the edge never changes between draw calls", transients("BUILD overview") === 0);
// the LFO family (RANDOM, JOURNEY, ... blit their block every frame) and the other families, one tap each
B.LFO = 3; B.SCL = 1; B.ARP = 8; B.GLO = 5; B.EDIT = 4; B.FX = 0; B.SEQ = 9; B.REC = 11;
for (const f of ["LFO", "LFO", "LFO", "LFO", "LFO", "LFO", "LFO", "LFO", "LFO"]) { tap(f); render(2500); const n = transients("LFO family page"); check("an LFO family page, standing still", n === 0); }
for (const f of ["SCL", "ARP", "GLO", "EDIT"]) { tap("HOME"); tap(f); render(3000); check(`${f} family, standing still`, transients(`${f}`) === 0); }
for (const f of ["SCL", "GLO", "FX", "EDIT", "REC", "SEQ"]) { tap("HOME"); ex.web_buttons(1 << B[f]); render(2500); check(`${f} layer held, standing still`, transients(`${f} layer`) === 0); ex.web_buttons(0); render(600); }
{   // every page of the FX, SCL, ARP, SEQ and GLO families, standing still (INSERT, SLICER, LINK, HARMONY, STEP, DETAIL, AUTOMATION, MIXER among them)
  for (const [f, n] of [["FX", 8], ["SCL", 4], ["ARP", 2], ["SEQ", 3]]) {
    tap("HOME"); render(300);
    for (let k = 0; k < n; k++) { tap(f); render(2000); check(`${f} family page ${k + 1}, standing still`, transients(`${f} page ${k + 1}`) === 0); }
  }
  tap("HOME"); render(300);
}
B.GLO = 5; B.OCTUP = 13; B.OCTDN = 12;
{   // BUILD's menu (GLO tapped twice): the tabs, the SYSTEM tab's ABOUT and INFO, standing still
  tap("HOME"); tap("GLO"); tap("GLO"); render(2500);
  check("BUILD MENU, standing still", transients("BUILD MENU") === 0);
  for (let k = 0; k < 4; k++) { ex.web_enc(1, 1); render(700); }
  for (let r = 0; r < 12; r++) { ex.web_enc(2, 1); render(80); }
  ex.web_buttons(1 << B.OCTUP); render(80); ex.web_buttons(0); render(2500);
  check("BUILD MENU ABOUT, standing still", transients("BUILD ABOUT") === 0);
  ex.web_buttons(1 << B.OCTDN); render(80); ex.web_buttons(0); render(500);
  ex.web_enc(2, -1); render(300); ex.web_buttons(1 << B.OCTUP); render(80); ex.web_buttons(0); render(2500);
  check("BUILD MENU INFO, standing still", transients("BUILD INFO") === 0);
  ex.web_buttons(1 << B.OCTDN); render(80); ex.web_buttons(0); render(300);
  tap("HOME"); render(600);
}
tap("ENV"); render(2500);
check("BUILD ENV page, standing still", transients("BUILD ENV") === 0);
tap("HOME"); tap("FX"); render(2500);
check("BUILD FX page, standing still", transients("BUILD FX") === 0);
tap("HOME"); tap("SAVE"); render(2500);
check("BUILD SAVE page, standing still", transients("BUILD SAVE") === 0);
{   // the hazard strip must stay while the page below repaints (a knob turned, the playing step moving): its pixels never take the page's background in between
  tap("HOME"); tap("FX"); render(800);
  ex.web_trace_reset();
  for (let k = 0; k < 24; k++) { ex.web_enc(3, k < 12 ? 1 : -1); render(40); }
  render(300);
  const n = ex.web_trace_count(), p = new Uint16Array(mem.buffer, ex.web_probe_ptr(), n * 6);
  let flips = 0;
  for (let i = 1; i < n; i++) for (const k of [4, 5]) if (p[i * 6 + k] !== p[(i - 1) * 6 + k]) flips++;
  console.log(`flicker: FX page, a knob turned: ${n} draw calls, ${flips} changes of the strip's pixels`);
  check("the hazard strip never changes while the page repaints", flips === 0);
  tap("HOME"); tap("SAVE"); render(800); ex.web_trace_reset();
  for (let k = 0; k < 12; k++) { ex.web_enc(3, 1); render(60); }
  const n2 = ex.web_trace_count(), p2 = new Uint16Array(mem.buffer, ex.web_probe_ptr(), n2 * 6);
  let f2 = 0;
  for (let i = 1; i < n2; i++) for (const k of [4, 5]) if (p2[i * 6 + k] !== p2[(i - 1) * 6 + k]) f2++;
  check("SAVE page, a knob turned: the strip never changes", f2 === 0);
}
{   // the same on every family's pages (a knob turned, the transport running): no draw call may take the strip's pixels
  const flipsOf = (fn) => {
    ex.web_trace_reset(); fn();
    const n = ex.web_trace_count(), q = new Uint16Array(mem.buffer, ex.web_probe_ptr(), n * 6);
    let f = 0;
    for (let i = 1; i < n; i++) for (const k of [4, 5]) if (q[i * 6 + k] !== q[(i - 1) * 6 + k]) f++;
    return f;
  };
  B.LFO = 3; B.SCL = 1; B.ARP = 8; B.SEQ = 9; B.EDIT = 4; B.GLO = 5; B.REC = 11;
  for (const [f, n] of [["FX", 8], ["SCL", 4], ["ARP", 2], ["SEQ", 3], ["EDIT", 6], ["ENV", 3], ["LFO", 9], ["SAVE", 5]]) {
    tap("HOME"); render(300);
    let bad = 0;
    for (let k = 0; k < n; k++) { tap(f); render(500); bad += flipsOf(() => { for (let j = 0; j < 6; j++) { ex.web_enc(3, j < 3 ? 1 : -1); render(50); } render(200); }); }
    check(`${f} family, every page, a knob turned: the strip never changes`, bad === 0);
  }
  {   // CHORD EXT's row (SCL family, SELECT twice from SCALE): the ring page, a knob turned
    tap("HOME"); render(300); tap("SCL"); render(500);
    const bad = flipsOf(() => { ex.web_enc(0, 1); render(300); ex.web_enc(0, 1); render(400); for (let j = 0; j < 4; j++) { ex.web_enc(3, j < 2 ? 1 : -1); render(60); } ex.web_enc(5, 1); render(200); });
    check("CHORD EXT row, a knob turned: the strip never changes", bad === 0);
  }
  for (const f of ["FX", "SCL", "EDIT", "GLO", "SEQ", "REC"]) {
    tap("HOME"); render(300);
    const bad = flipsOf(() => { ex.web_buttons(1 << B[f]); render(600); ex.web_enc(3, 1); render(200); ex.web_buttons(0); render(400); });
    check(`${f} layer held and let go: the strip never changes`, bad === 0);
  }
}

// Regression #60: the physical panel displays each SPI operation. A BUILD
// ring page must never send the Classic header first, nor restore its four
// warning rows separately after every redraw. Every header transfer during a
// parameter edit must be the complete, final BUILD 240x28 region.
function buildHeaderTransfers(label) {
  const n = ex.web_trace_count();
  const tr = new Uint16Array(mem.buffer, ex.web_trace_ptr(), n * 6);
  let whole = 0, split = 0;
  for (let i = 0; i < n; i++) {
    const [x, y, w, h, , kind] = tr.subarray(i * 6, i * 6 + 6);
    if (y >= 28 || y + h === 0) continue;
    if (x === 0 && y === 0 && w === 240 && h === 28 && kind === 1) whole++;
    else split++;
  }
  console.log(`flicker #60: ${label}: ${whole} whole BUILD header transfers, ${split} overlapping/split transfers`);
  return { whole, split };
}
{   // The MOVE > LFO screen from the owner's FM-1 video.
  tap("HOME"); tap("LFO"); render(3000);
  ex.web_trace_reset(); render(600);
  const idle = buildHeaderTransfers("MOVE/LFO idle");
  check("BUILD MOVE/LFO idle: no header or roadworks LCD transfers", idle.whole === 0 && idle.split === 0);
  ex.web_trace_reset();
  for (let j = 0; j < 16; j++) { ex.web_enc(3 + j % 4, j < 8 ? 1 : -1); render(55); }
  const turning = buildHeaderTransfers("MOVE/LFO knobs K1-K4");
  check("BUILD MOVE/LFO turning: no old/split header or redundant stripe transfer", turning.split === 0);
  check("BUILD MOVE/LFO turning: no excessive full title updates", turning.whole <= 20);
  render(3000);
  ex.web_trace_reset(); render(600);
  const settled = buildHeaderTransfers("MOVE/LFO settled");
  check("BUILD MOVE/LFO settled: zero transfers to stable 28-row header", settled.whole === 0 && settled.split === 0);
}
{   // Newly drawn BUILD layers must follow the same rule on entry and edits.
  tap("HOME"); render(300);
  ex.web_buttons(1 << B.SCL); render(500);
  ex.web_trace_reset(); ex.web_enc(3, 1); render(150);
  const held = buildHeaderTransfers("BUILD held SCL layer");
  check("BUILD held SCL: single combined header / warning band", held.split === 0);
  ex.web_buttons(0); render(1500);
}

console.log(fails ? `flicker: ${fails} failed` : "flicker: all passed");
process.exit(fails ? 1 : 0);
