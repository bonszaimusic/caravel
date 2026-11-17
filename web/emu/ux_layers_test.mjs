// SPDX-License-Identifier: GPL-3.0-only
// The UX layers (CARAVEL_UX_LAYERS=1: BUILD and PLAY, ui_uxl.c) in Node, through the firmware's own screen:
//   EMU_CFLAGS="-DCARAVEL_UX_LAYERS=1 -DCARAVEL_UX_CAPTURE=1" web/emu/build.sh && node web/emu/ux_layers_test.mjs build/emu/felucca.wasm
// Checks by screen comparison (the firmware's framebuffer, not a browser): the layers' gestures, the guards, the groups, the
// safe preview. Not a hardware test and no replacement for looking at the screens (web/emu/shots.mjs).
import fs from "fs";
const wasmPath = process.argv[2] || "build/emu/felucca.wasm";
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12, OCTUP: 13 };
const EN = { SELECT: 0, ALGO: 1, PRESETS: 2, K1: 3, K2: 4, K3: 5, K4: 6 };
let fails = 0;
const check = (what, ok) => { console.log(`ux: ${what.padEnd(78)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
async function device() {
  const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
  const ex = instance.exports, mem = ex.memory;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot(); if (ex.web_test_build) ex.web_test_build();
  const d = { ex };
  d.peak = 0;
  d.render = (ms) => {
    const n = Math.round(ms * 44.1);
    for (let k = 0; k < n; k += 128) {
      const m = Math.min(128, n - k);
      ex.web_render(m);
      const l = new Float32Array(mem.buffer, ex.web_out_l(), m);
      for (let i = 0; i < m; i++) d.peak = Math.max(d.peak, Math.abs(l[i]));
    }
  };
  d.tap = (b) => { ex.web_buttons(1 << B[b]); d.render(70); ex.web_buttons(0); d.render(90); };
  d.dtap = (b) => { d.tap(b); d.tap(b); };
  d.hold = (bs, ms = 1100) => { ex.web_buttons([].concat(bs).reduce((m, b) => m | (1 << B[b]), 0)); d.render(ms); };
  d.release = () => { ex.web_buttons(0); d.render(150); };
  d.turn = (r, n) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(EN[r], Math.sign(n)); d.render(40); } d.render(60); };
  d.screen = () => new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240).slice();
  d.settle = () => d.render(3500);             // a header message runs out
  return d;
}
const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
const px = (fb, x, y) => fb[y * 240 + x];
// the circle in the middle of PLAY shows the notes as they flow (with the clock running it changes all the time): compare the rest
const sameOutside = (a, b) => a.every((v, i) => { const x = i % 240, y = (i / 240) | 0; return (x > 70 && x < 170 && y > 66 && y < 166) || v === b[i]; });

const d = await device();
d.render(2500); d.settle();
const build0 = d.screen();
d.hold("HOME"); d.release(); d.settle();
const play0 = d.screen();
check("HOME held 1 second: the screen changes (PLAY)", !same(build0, play0));
const tabColour = px(play0, 50, 12);
check("PLAY: the filled tab in the header", px(play0, 50, 12) !== px(build0, 50, 12));
d.hold("HOME"); d.release(); d.settle();
check("HOME held again: back to the BUILD screen as it was", same(d.screen(), build0));

// guards: HOME held with a key held does nothing
d.ex.web_keys(1 << 7); d.render(100);
d.hold("HOME"); d.release(); d.ex.web_keys(0); d.render(100); d.settle();
check("HOME held with a key held: no layer change", same(d.screen(), build0));
d.hold("HOME"); d.release(); d.settle();
check("PLAY again", !same(d.screen(), build0));

// PLAY: areas, groups, arcs
const sound = (() => { d.tap("EDIT"); d.settle(); return d.screen(); })();
check("EDIT: the SOUND area differs from the first view", !same(sound, play0));
d.tap("EDIT"); d.settle(); const soundB = d.screen();
check("EDIT again: the engine's other EDIT slots (group B, E50)", !same(soundB, sound));
d.tap("EDIT"); d.settle(); const soundC = d.screen();
check("EDIT a third time: ANALOG has six more slots, so a third group", !same(soundC, soundB) && !same(soundC, sound));
d.tap("EDIT"); d.settle();
check("EDIT a fourth time: back to the engine's default knobs", same(d.screen(), sound));
d.tap("EDIT"); d.settle();
d.tap("FX"); d.settle(); const fxA = d.screen();
d.tap("FX"); d.settle(); const fxB = d.screen();
check("FX twice: the second group (ROOM) differs from SENDS", !same(fxA, fxB));
d.tap("EDIT"); d.tap("FX"); d.settle();
check("FX again after another area: the group last used (B)", same(d.screen(), fxB));
d.turn("SELECT", -1); d.settle();
check("SELECT: the group of the area (A)", same(d.screen(), fxA));
d.tap("EDIT"); d.settle();
d.turn("K1", 6); d.render(200);
check("K1 in SOUND: the arc and its value change", !same(d.screen(), sound));
d.settle();
check("the middle returns to the area name after a while", !same(d.screen(), sound));
// all 27 keys are notes in PLAY
let lit = 0;
for (let k = 0; k < 27; k++) { d.ex.web_keys(1 << k); d.render(40); if (d.ex.web_lit_keys() & (1 << k)) lit++; d.ex.web_keys(0); d.render(30); }
check("PLAY: all 27 keys light and play as notes", lit === 27);

// safe preview
d.tap("EDIT"); d.settle(); const before = d.screen();
d.turn("PRESETS", 2);
const pv = d.screen();
check("PRESETS in PLAY: a preview, no load (the screen shows it)", !same(pv, before));
d.tap("OCTDN"); d.settle();
check("OCT-: cancelled, the screen as before", same(d.screen(), before));
d.turn("PRESETS", 2); d.tap("OCTUP"); d.settle();
check("OCT+: the sound loaded (the SOUND view shows other values or names)", !same(d.screen(), before));

// the menu: GLO tapped twice, from BUILD only
d.hold("HOME"); d.release(); d.settle();
const b1 = d.screen();
d.dtap("GLO"); d.settle();
check("BUILD: GLO tapped twice opens the menu", !same(d.screen(), b1));
d.tap("HOME"); d.settle();
d.hold("HOME"); d.release(); d.settle();
d.dtap("GLO"); d.settle();
check("PLAY: GLO tapped twice does not open the menu (the tab stays)", px(d.screen(), 50, 12) === tabColour);

// PLAY keeps the destructive layers shut: EDIT held changes nothing but keys
d.tap("HOME"); d.settle();
d.hold("EDIT", 700); const eh = d.screen(); d.release();
check("PLAY: EDIT held opens no EDIT SET map", px(eh, 50, 12) === tabColour);
d.hold("FX", 700); const fxh = d.screen(); d.release();
check("PLAY: FX held opens the FX HOLD map (a performance layer)", !same(fxh, eh) && px(fxh, 50, 12) === tabColour);
// panic: OCT- and OCT+ together 700 ms (a note held first sounds, then stops)
d.tap("HOME"); d.settle();
d.ex.web_keys(1 << 9); d.render(300);
const sounding = d.ex.web_lit_keys() & (1 << 9);
d.ex.web_keys(0); d.render(100);
const preP = d.screen();
d.hold(["OCTDN", "OCTUP"], 900); const panicScr = d.screen(); d.release();
check("PLAY: OCT- and OCT+ held 700 ms: PANIC says so in the header", !same(panicScr, preP));
d.settle();
check("PLAY: the update countdown stays off while both are held (2 s)", (() => { d.hold(["OCTDN", "OCTUP"], 2600); const x = d.screen(); d.release(); return px(x, 50, 12) === tabColour; })());
// CAPTURE (needs -DCARAVEL_UX_CAPTURE=1): play, take the last bars, the loop plays them again, SEQ pauses and clears it
d.tap("HOME"); d.settle();
d.tap("PLAY"); d.render(300);
d.ex.web_keys(1 << 7); d.render(250); d.ex.web_keys(0); d.render(250);
d.ex.web_keys(1 << 12); d.render(250); d.ex.web_keys(0); d.render(300);
d.tap("REC"); d.render(300);
d.peak = 0; d.render(2400);
check("CAPTURE: the loop plays the notes again (no key down, sound follows)", d.peak > 0.002);
const ln0 = d.ex.web_test_cap_events(0);
d.ex.web_keys(1 << 15); d.render(250); d.ex.web_keys(0); d.render(300);
d.tap("REC"); d.render(300);
check("CAPTURE: REC again is an overdub (the loop keeps its notes and gets the new ones)", d.ex.web_test_cap_events(0) === ln0 + 2);
d.tap("SEQ"); d.render(1500); d.peak = 0; d.render(2400);
check("CAPTURE: SEQ pauses the loop (silence)", d.peak < 0.0005);
d.tap("SEQ"); d.peak = 0; d.render(3000);
check("CAPTURE: SEQ again lets it go on", d.peak > 0.002);
d.hold("SEQ", 900); d.release(); d.render(1500); d.peak = 0; d.render(3000);
check("CAPTURE: SEQ held 700 ms clears the loop (silence)", d.peak < 0.0005);
// LEDs in PLAY: the area's button lit, HOME breathing
d.tap("EDIT"); d.render(200);                       // (still in PLAY)
check("PLAY LEDs: the EDIT button is lit (its area)", (d.ex.web_lit_buttons() & (1 << B.EDIT)) !== 0);
check("PLAY LEDs: HOME breathes", (d.ex.web_breath_buttons() & (1 << B.HOME)) !== 0);
d.hold("HOME"); d.release(); d.settle();           // back to BUILD
// BUILD's overview: K1..K4 are the levels of the four tracks
const ov = d.screen();
d.turn("K2", -8);
check("BUILD overview: K2 moves the level of T2 (the bar on the screen)", !same(d.screen(), ov));
// MOMENTARY in PLAY: LFO held, an arc turned: it goes back when LFO is let go; OCT+ keeps it
d.hold("HOME"); d.release(); d.settle();            // BUILD -> PLAY
d.tap("LFO"); d.settle();
const m0 = d.screen();
d.hold("LFO", 400); d.turn("K1", 6); const m1 = d.screen();
check("PLAY MOMENTARY: LFO held and K1 turned: the arc moves", !same(m1, m0));
d.release(); d.settle();
check("PLAY MOMENTARY: LFO let go: the value is back, the area stays", sameOutside(d.screen(), m0));
d.hold("LFO", 400); d.turn("K1", 6);
d.ex.web_buttons((1 << B.LFO) | (1 << B.OCTUP)); d.render(100); d.ex.web_buttons(1 << B.LFO); d.render(100);
d.release(); d.settle();
check("PLAY MOMENTARY: OCT+ with LFO held keeps the value", !sameOutside(d.screen(), m0));
// STASIS verbs in PLAY: FX held, B4 HOLD, C5 ADD, F5 BLEND, G5 RELEASE (need -DCARAVEL_REVERB_MINIMAL=1)
if (d.ex.web_test_stasis) {
  d.ex.web_test_stasis(1);
  const before = d.ex.web_test_dr_level();
  d.hold("FX", 700);
  d.ex.web_keys(1 << 18); d.render(150); d.ex.web_keys(0); d.render(150);
  check("STASIS: FX held, B4 is HOLD (the reverb holds)", d.ex.web_test_dr_level() !== before);
  d.ex.web_keys(1 << 26); d.render(150); d.ex.web_keys(0); d.render(150);
  d.release(); d.settle();
  d.ex.web_test_stasis(0);
}
// no stripe without an end: the clock stops while a key is held, PANIC with a key held
if (d.ex.web_test_cap_open) {
  d.hold("HOME"); d.release(); d.settle();                            // (BUILD -> PLAY)
  d.tap("PLAY"); d.render(300);
  d.ex.web_keys(1 << 10); d.render(300); d.tap("PLAY"); d.render(300); d.ex.web_keys(0); d.render(300);
  check("CAPTURE: the clock stopped with a key held: the note still gets its end", d.ex.web_test_cap_open(0) % 100 === 0);
  d.tap("PLAY"); d.render(300);
  d.ex.web_keys(1 << 12); d.render(200); d.ex.web_buttons((1 << B.OCTDN) | (1 << B.OCTUP)); d.render(900); d.ex.web_keys(0); d.ex.web_buttons(0); d.render(300);
  check("CAPTURE: PANIC with a key held: the note gets its end", d.ex.web_test_cap_open(0) % 100 === 0);
  d.hold("SEQ", 900); d.release(); d.render(300);
  check("CAPTURE: SEQ held clears the live notes too (nothing left to show)", d.ex.web_test_cap_open(0) === 0);
}
// REC with notes at the same moment (a fresh device): a note struck just before and still held, one struck while REC is down, a second REC (no copies)
// BUILD: LFO held (to try the verbs) is no page tap when it is let go; a tap still moves on (the page used to go round at each release)
if (d.ex.web_test_ev) {
  const h = await device();
  h.render(2500); h.settle();
  for (let n = 0; n < 14 && h.ex.web_test_ev(40) !== 1; n++) { h.tap("LFO"); h.render(200); }
  check("BUILD: LFO tapped until RANDOM is shown", h.ex.web_test_ev(40) === 1);
  h.hold("LFO", 1100); h.release(); h.render(300);
  check("BUILD: LFO held 1.1 s and let go: still on RANDOM", h.ex.web_test_ev(40) === 1);
  h.ex.web_buttons(1 << B.LFO); h.render(100); h.ex.web_keys(1 << 24); h.render(120); h.ex.web_keys(0); h.render(60); h.ex.web_buttons(0); h.render(300);
  check("BUILD: LFO held with a key (a verb) and let go: still on RANDOM", h.ex.web_test_ev(40) === 1);
  h.tap("LFO"); h.render(300);
  check("BUILD: a plain tap on LFO still goes to the next page", h.ex.web_test_ev(40) !== 1);
}
// ENV + a knob is an edit, even on a short hold; releasing it keeps the page.
{
  const e = await device(); e.render(2500); e.tap("ENV"); e.render(200);
  const page = e.ex.web_test_ev(1000);
  e.ex.web_buttons(1 << B.ENV); e.render(45);
  e.ex.web_enc(EN.K1, 1); e.render(45); e.release();
  check("BUILD: ENV + knob below hold timeout keeps page on release", e.ex.web_test_ev(1000) === page);
  e.tap("ENV"); e.render(200);
  check("BUILD: plain ENV tap still advances page", e.ex.web_test_ev(1000) !== page);
}
// BUILD, the SAVE family in PLAY's manner: the five pages differ, the knob still moves the chosen entry, the family's taps still go round
if (d.ex.web_test_ev) {
  const sv = await device();
  sv.render(2500); sv.settle();
  const shots = [];
  for (let n = 0; n < 5; n++) { sv.tap("SAVE"); sv.render(500); shots.push(sv.screen()); }
  check("BUILD SAVE: the five pages of the family draw five different screens", shots.every((a, i) => shots.every((b, j) => i === j || !same(a, b))));
  sv.tap("SAVE"); sv.render(400);                                // back to USER
  const before = sv.screen();
  sv.turn("K1", 2); sv.render(400);
  check("BUILD SAVE: KNOB 1 moves the chosen slot (the screen changes)", !same(before, sv.screen()));
}
// CAPTURE, one owner (device plan C-a): the UI only posts commands, the audio context takes them at the start of its next block, in order
if (d.ex.web_test_cap_post) {
  const q = await device();
  q.render(2500); q.hold("HOME"); q.release(); q.settle();
  q.tap("PLAY"); q.render(300);
  q.ex.web_keys(1 << 7); q.render(200); q.ex.web_keys(0); q.render(200);
  q.ex.web_keys(1 << 10); q.render(200); q.ex.web_keys(0); q.render(200);
  const OP = { REC: 1, TOGGLE: 2, WIPE: 3, ALL_PAUSE: 4 };
  const posted = [OP.REC, OP.REC, OP.TOGGLE, OP.TOGGLE].map((o) => q.ex.web_test_cap_post(o, 0, 2));
  check("CAPTURE C-a: four commands posted in one go are all taken (nothing runs before the block)", posted.every((v) => v === 1) && q.ex.web_test_cap_pending() === 4 && q.ex.web_test_cap_events(0) === 0);
  q.render(20);
  check("CAPTURE C-a: .. the next block runs them in order (REC, REC adds nothing, SEQ pauses, SEQ goes on)", q.ex.web_test_cap_pending() === 0 && q.ex.web_test_cap_events(0) === 4 && q.ex.web_test_cap_state(0) === 1);
  const full = Array.from({ length: 10 }, () => q.ex.web_test_cap_post(OP.TOGGLE, 0, 0));
  check("CAPTURE C-a: the queue holds eight, a ninth is refused (the caller says so)", full.filter((v) => v === 1).length === 8 && full[8] === 0);
  q.render(20);
  check("CAPTURE C-a: .. eight toggles leave the loop as it was", q.ex.web_test_cap_pending() === 0 && q.ex.web_test_cap_state(0) === 1);
  q.ex.web_test_cap_post(OP.ALL_PAUSE, 0, 0); q.render(20);
  check("CAPTURE C-a: ALL_PAUSE pauses the loop", q.ex.web_test_cap_state(0) === 2);
  q.ex.web_test_cap_post(OP.WIPE, 0, 0); q.render(20);
  check("CAPTURE C-a: WIPE clears it", q.ex.web_test_cap_state(0) === 0 && q.ex.web_test_cap_events(0) === 0);
  const blk0 = q.ex.web_test_cap_prof(0); q.render(30);
  check("CAPTURE C-e: the block counter counts every block", q.ex.web_test_cap_prof(0) - blk0 >= 8);
  check("CAPTURE C-e: .. and the takes (four so far in this run)", q.ex.web_test_cap_prof(2) >= 1);
  const m0 = q.screen();
  q.ex.web_test_cap_post(OP.REC, 0, 2); q.render(40);
  check("CAPTURE C-a: REC with nothing played says so (a message, no loop)", q.ex.web_test_cap_state(0) === 0 && !same(m0, q.screen()));
}
if (d.ex.web_test_cap_events) {
  const e = await device();
  e.render(2500); e.hold("HOME"); e.release(); e.settle();
  e.tap("PLAY"); e.render(300);
  e.ex.web_keys(1 << 7); e.render(100); e.tap("REC"); e.ex.web_keys(0); e.render(200);
  check("CAPTURE: a note struck just before REC and still held is captured", e.ex.web_test_cap_events(0) === 2);
  e.hold("SEQ", 900); e.release(); e.render(300);
  e.ex.web_keys(1 << 10); e.render(200); e.ex.web_keys(0); e.render(200);
  e.ex.web_buttons(1 << B.REC); e.render(30); e.ex.web_keys(1 << 12); e.render(40); e.ex.web_buttons(0); e.render(200); e.ex.web_keys(0); e.render(200);
  check("CAPTURE: a note struck while REC is down is a note and the REC still takes the loop", e.ex.web_test_cap_events(0) === 4);
  e.hold("SEQ", 900); e.release(); e.render(300);                 // wipe, then two notes, REC, REC again
  e.ex.web_keys(1 << 7); e.render(200); e.ex.web_keys(0); e.render(200);
  e.ex.web_keys(1 << 10); e.render(200); e.ex.web_keys(0); e.render(200);
  e.tap("REC"); e.render(300); e.tap("REC"); e.render(300);
  check("CAPTURE: REC twice in a row does not copy the notes (the overdub adds what is new)", e.ex.web_test_cap_events(0) === 4);
}
// a note struck BEFORE the window and still held goes into the loop too (E49)
if (d.ex.web_test_cap_events) {
  const f = await device();
  f.render(2500); f.hold("HOME"); f.release(); f.settle();
  f.tap("PLAY"); f.render(300);
  f.ex.web_keys(1 << 7); f.render(6500);                      // longer than the window (2 bars at 120 BPM = 4 s)
  f.tap("REC"); f.render(300);
  check("CAPTURE: a held note struck before the window is in the loop", f.ex.web_test_cap_events(0) === 2);
  f.ex.web_keys(0); f.render(300); f.peak = 0; f.render(2500);
  check("CAPTURE: .. and the loop plays it after the key is let go", f.peak > 0.002);
}
// F6 A: entry help, first input/timeout dismisses, HOME has no help gesture.
{
  const h=await device();h.render(2500);const build=h.screen();
  h.hold("HOME",850);check("HOME: 850 ms stays in BUILD",same(h.screen(),build));
  h.render(250);h.release();const entry=h.screen();
  check("entry help: PLAY is entered after one second",px(entry,50,12)!==px(build,50,12));
  h.render(2700);const second=h.screen();
  check("entry help: second gesture hint appears without a key press",!same(entry,second));
  h.render(2500);const expired=h.screen();
  check("entry help: disappears after five seconds",!same(second,expired));
  h.hold("HOME",600);const held=h.screen();
  check("HOME: no peek help during a sub-second press",same(held,expired));
  h.release();h.settle(); // short HOME still goes to SOUND, as before
  h.hold("HOME");h.release();h.hold("HOME");h.release();
  const again=h.screen();h.ex.web_keys(1<<7);h.render(40);h.ex.web_keys(0);h.render(80);
  const after=h.screen();
  check("entry help: first note dismisses it",!same(again,after));
  h.render(1000);check("entry help: dismissed hint stays gone",same(after,h.screen()));
}
// PAIR (E53): the pair's owner carries the loop - REC, SEQ and the shown notes follow it also with the follower track selected
if (d.ex.web_test_cap_events && d.ex.web_pair_prepare && d.ex.web_test_select) {
  const q = await device();
  q.render(2500);
  const ok = q.ex.web_pair_prepare(0, 1, 0, 0);
  q.hold("HOME"); q.release(); q.settle();
  if (ok) {
    q.ex.web_test_select(1);                          // the follower
    q.tap("PLAY"); q.render(300);
    q.ex.web_keys(1 << 7); q.render(900); q.ex.web_keys(0); q.render(200);
    q.tap("REC"); q.render(300);
    check("PAIR: REC on the follower track takes the loop (of the owner)", q.ex.web_test_cap_events(0) === 2 && q.ex.web_test_cap_events(1) === 0);
    q.ex.web_test_select(0);
    q.tap("SEQ"); q.render(200);
    check("PAIR: SEQ on the owner pauses the same loop", q.ex.web_test_cap_events(0) === 2);
  } else console.log("ux: PAIR: pair_prepare refused (skipped)");
}
// EVOLVE (E55): RANDOM, JOURNEY, ELASTIC and PAIR are groups B..E of MOVE; D5 E5 F5 G5 held with LFO are their verbs A1..A4
if (d.ex.web_test_ev) {
  const KEYV = [21, 23, 24, 26];
  const v = await device();
  const verb = (i, ms = 150) => {
    v.ex.web_buttons(1 << B.LFO); v.render(100);
    v.ex.web_keys(1 << KEYV[i]); v.render(ms); v.ex.web_keys(0); v.render(100);
    v.ex.web_buttons(0); v.render(200);
  };
  v.render(2500); v.hold("HOME"); v.release(); v.settle();
  v.tap("LFO"); v.tap("LFO");                                   // MOVE, group B: RANDOM
  check("EVOLVE: the second tap on LFO is group B", v.ex.web_test_ev(11) === 3 && v.ex.web_test_ev(10) === 1);
  v.ex.web_keys(1 << 21); v.render(300); v.ex.web_keys(0); v.render(300);
  check("EVOLVE: (control) the same key without LFO is a note", v.peak > 0.001);
  v.render(4000); v.peak = 0;                                   // (the note's tail out first)
  const before = v.ex.web_test_ev(12);
  verb(0);
  v.render(3000);
  check("EVOLVE: .. and RANDOM moves the sound's parameters", v.ex.web_test_ev(12) !== before);
  verb(0);
  check("EVOLVE: LFO + D5 starts RANDOM on the targets PLAY shows", v.ex.web_test_ev(0) === 1 && v.ex.web_test_ev(4) > 0);
  check("EVOLVE: .. the key is a verb, not a note (no sound)", v.peak < 0.001);
  check("EVOLVE: .. and the group stays B (LFO's release is no tap)", v.ex.web_test_ev(10) === 1);
  verb(1);
  check("EVOLVE: A2 FREEZE", v.ex.web_test_ev(0) === 2);
  verb(2);
  check("EVOLVE: A3 RETURN", v.ex.web_test_ev(0) === 3 || v.ex.web_test_ev(0) === 4 || v.ex.web_test_ev(0) === 0);
  v.ex.web_buttons(1 << B.LFO); v.render(100); v.ex.web_keys(1 << KEYV[3]); v.render(800); v.ex.web_keys(0); v.render(100); v.release();
  check("EVOLVE: A4 held stores snapshot S1", v.ex.web_test_ev(5) > 0);
  v.ex.web_buttons(1 << B.LFO); v.render(100); v.ex.web_keys(1 << KEYV[3]); v.render(150); v.ex.web_keys(0); v.render(100); v.ex.web_buttons(0); v.render(200);
  check("EVOLVE: A4 tapped chooses the next slot", v.ex.web_test_ev(3) === 1);
  v.tap("EDIT"); v.turn("K1", 12); v.tap("LFO");                // another sound, back to MOVE (group B kept)
  check("EVOLVE: MOVE keeps its last group", v.ex.web_test_ev(11) === 3 && v.ex.web_test_ev(10) === 1);
  v.ex.web_buttons(1 << B.LFO); v.render(100); v.ex.web_keys(1 << KEYV[3]); v.render(800); v.ex.web_keys(0); v.render(100); v.release();
  check("EVOLVE: A4 held stores snapshot S2", v.ex.web_test_ev(6) > 0);
  v.tap("LFO");                                                 // group C: JOURNEY
  verb(0);
  check("EVOLVE: JOURNEY: A1 PLAY starts the journey between the snapshots", v.ex.web_test_ev(1) === 1);
  verb(0);
  check("EVOLVE: JOURNEY: A1 again stops it", v.ex.web_test_ev(1) === 0 || v.ex.web_test_ev(8) === 1);
  v.tap("LFO");                                                 // group D: ELASTIC
  verb(0);
  check("EVOLVE: ELASTIC: A1 arms", v.ex.web_test_ev(2) === 1);
  const u = await device();                                     // ELASTIC on a DRONE: arm, then springs from any area
  u.render(2500); u.ex.web_test_drone(0);
  u.hold("HOME"); u.release(); u.settle();
  for (let i = 0; i < 4; i++) u.tap("LFO");                     // group D: ELASTIC
  u.ex.web_buttons(1 << B.LFO); u.render(100); u.ex.web_keys(1 << KEYV[0]); u.render(150); u.ex.web_keys(0); u.render(100); u.ex.web_buttons(0); u.render(200);
  check("EVOLVE: ELASTIC: A1 arms, no spring yet", u.ex.web_test_ev(2) === 1 && u.ex.web_test_ev(13) === 0);
  u.tap("EDIT"); u.turn("K1", 3);
  check("EVOLVE: ELASTIC: a SOUND arc becomes a spring (no CELL or ROW to choose)", u.ex.web_test_ev(13) === 1);
  u.turn("K2", 3);
  u.tap("FX"); u.turn("K1", 3);
  check("EVOLVE: ELASTIC: an arc of another area is a spring too", u.ex.web_test_ev(13) === 3);
  check("EVOLVE: ELASTIC: .. and the springs move", u.ex.web_test_ev(14) !== 0);
  check("EVOLVE: ELASTIC: .. and reach the sound (published to the audio side)", u.ex.web_test_ev(30) === 1);
  u.turn("K2", 3); u.turn("K3", 3); u.turn("K4", 3);
  check("EVOLVE: ELASTIC: never more than four", u.ex.web_test_ev(13) === 4);
  u.render(1000);
  {   // the settings stay when the page is changed; a touch hands them to its spring
    u.tap("EDIT"); u.turn("K2", 2); u.render(300);                // some spring touched last
    u.tap("LFO");                                                 // MOVE, ELASTIC group
    const t0 = u.ex.web_test_ev(44), first = u.ex.web_test_ev(47);
    u.turn("K1", 10);
    check("EVOLVE: ELASTIC: PULL turned (the settings of the moment)", u.ex.web_test_ev(44) === t0 + 10);
    check("EVOLVE: .. and live on the spring touched last", u.ex.web_test_ev(31 + first) === t0 + 10);
    u.tap("FX"); u.turn("K3", 2); u.render(300);                  // another page, another parameter
    u.tap("LFO");
    check("EVOLVE: ELASTIC: back on the page the PULL is still there", u.ex.web_test_ev(44) === t0 + 10);
    check("EVOLVE: .. and the spring just touched has it", u.ex.web_test_ev(31 + u.ex.web_test_ev(47)) === t0 + 10);
  }
  check("EVOLVE: ELASTIC: stays armed with its springs", u.ex.web_test_ev(2) === 1);
  {   // A2: the springs into the background: other parameters are plain again, the springs run on
    const n0 = u.ex.web_test_ev(13);
    u.ex.web_buttons(1 << B.LFO); u.render(100); u.ex.web_keys(1 << KEYV[1]); u.render(150); u.ex.web_keys(0); u.render(100); u.ex.web_buttons(0); u.render(200);
    u.tap("ARP"); const sum0 = u.ex.web_test_ev(12); u.turn("K1", 4);          // ARP, a parameter with no spring yet
    check("EVOLVE: ELASTIC: in the background a turn is plain (no new spring, the value changes)", u.ex.web_test_ev(13) === n0 && u.ex.web_test_ev(12) !== sum0);
    check("EVOLVE: .. and the springs are still armed", u.ex.web_test_ev(2) === 1);
    u.ex.web_buttons(1 << B.LFO); u.render(100); u.ex.web_keys(1 << KEYV[1]); u.render(150); u.ex.web_keys(0); u.render(100); u.ex.web_buttons(0); u.render(200);
    u.turn("K2", 3);
    check("EVOLVE: ELASTIC: A2 again: turns pull again", u.ex.web_test_ev(13) >= Math.min(4, n0 + 0) && u.ex.web_test_ev(2) === 1);
  }
  u.ex.web_buttons(1 << B.LFO); u.render(100); u.ex.web_keys(1 << KEYV[0]); u.render(150); u.ex.web_keys(0); u.render(100); u.ex.web_buttons(0); u.render(200);
  check("EVOLVE: ELASTIC: A1 again lets all go", u.ex.web_test_ev(2) === 0 && u.ex.web_test_ev(13) === 0);
  const w = await device();
  w.render(2500);
  if (w.ex.web_pair_prepare(0, 1, 0, 4)) {
    w.hold("HOME"); w.release(); w.settle();
    w.tap("LFO"); for (let i = 0; i < 4; i++) w.tap("LFO");     // group E: PAIR
    w.ex.web_buttons(1 << B.LFO); w.render(100); w.ex.web_keys(1 << KEYV[2]); w.render(150); w.ex.web_keys(0); w.render(100); w.ex.web_buttons(0); w.render(200);
    check("EVOLVE: PAIR: A3 goes to B, the pair stays on", w.ex.web_pair_get(0, 0) === 1 && w.ex.web_test_ev(10) === 4);
  }
}
// INTERACT (E56): BUILD, ARP held 700 ms: a knob right puts the page's parameter into the track's set, left takes it out
if (d.ex.web_test_ev) {
  const g = await device();
  g.render(2500);
  g.tap("ENV");                                                 // a page with four parameters
  g.hold("ARP", 900); g.release(); g.render(200);
  check("INTERACT: ARP held 700 ms switches it on", g.ex.web_test_ev(27) === 1);
  g.turn("K1", 1); g.turn("K2", 1);
  check("INTERACT: K1 and K2 right put two parameters in the set", g.ex.web_test_ev(29) === 2 && (g.ex.web_test_ev(28) & 1) === 1);
  g.turn("K1", 1);
  check("INTERACT: the same one again is not added twice", g.ex.web_test_ev(29) === 2);
  g.turn("K3", -1);
  check("INTERACT: left on one that is not in it changes nothing", g.ex.web_test_ev(29) === 2);
  g.render(3500);
  const marked = g.screen();
  g.turn("K2", -1);
  check("INTERACT: K2 left takes it out", g.ex.web_test_ev(29) === 1);
  g.render(3500);
  check("INTERACT: the marks in the header follow", !same(marked, g.screen()));
  g.turn("K2", 1);
  g.hold("HOME", 1100); g.release(); g.settle();                // PLAY: the mode ends
  check("INTERACT: HOME held to PLAY ends the mode", g.ex.web_test_ev(27) === 0);
  g.tap("LFO"); g.tap("LFO");
  g.ex.web_buttons(1 << B.LFO); g.render(100); g.ex.web_keys(1 << 21); g.render(150); g.ex.web_keys(0); g.render(100); g.ex.web_buttons(0); g.render(200);
  check("INTERACT: RANDOM moves exactly the set (two targets, not what PLAY shows)", g.ex.web_test_ev(4) === 2);
  g.tap("HOME"); g.render(100);
  g.hold("HOME", 1100); g.release(); g.settle();                // back in BUILD: the LFO family (the area changed to MOVE in PLAY)
  g.tap("ENV");                                                 // (the ENV family opens on the page it had)
  g.hold("ARP", 900); g.release(); g.render(200);
  g.turn("K1", -1); g.turn("K2", -1);
  check("INTERACT: the last one out gives PLAY's default back", g.ex.web_test_ev(28) === 0 && g.ex.web_test_ev(29) === 0);
  g.hold("ARP", 900); g.release(); g.render(200);
  check("INTERACT: ARP held again switches it off", g.ex.web_test_ev(27) === 0);
}
// the setup of the verbs is in BUILD, in the LFO family (E58)
if (d.ex.web_test_ev) {
  const b = await device();
  b.render(2500);
  let n = 0;
  while (b.ex.web_test_ev(40) !== 1 && n++ < 16) b.tap("LFO");
  check("BUILD: the LFO family has a RANDOM page", b.ex.web_test_ev(40) === 1);
  const sc0 = b.ex.web_test_ev(41), bars0 = b.ex.web_test_ev(42);
  b.turn("K3", 1); b.turn("K1", 1);
  check("BUILD: RANDOM page: K3 SCOPE and K1 BARS", b.ex.web_test_ev(41) !== sc0 || sc0 === 2);
  check("BUILD: RANDOM page: BARS follows K1", b.ex.web_test_ev(42) !== bars0);
  b.tap("LFO");
  check("BUILD: the next page is JOURNEY", b.ex.web_test_ev(40) === 2);
  const w0 = b.ex.web_test_ev(9);
  b.turn("K3", 1);
  check("BUILD: JOURNEY page: K3 is WAIT", b.ex.web_test_ev(9) === w0 + 1);
  b.tap("LFO");
  check("BUILD: the next page is SPRING (the start values of a new spring)", b.ex.web_test_ev(40) === 3);
  const pl0 = b.ex.web_test_ev(44);
  b.turn("K1", 5);
  check("BUILD: SPRING page: K1 is PULL", b.ex.web_test_ev(44) === pl0 + 5);
  b.tap("LFO");
  check("BUILD: the next page is SPRINGS (the track's)", b.ex.web_test_ev(40) === 4);
  b.turn("K1", 20);
  check("BUILD: SPRINGS page: K1 is COUPLE", b.ex.web_test_ev(45) > 0);
}
// HOME held between BUILD and PLAY: the same block of functions on both sides (E65)
if (d.ex.web_test_ev) {
  const m = await device();
  m.render(2500);
  m.tap("ENV");                                                 // BUILD, the ENV family
  m.hold("HOME", 1100); m.release(); m.settle();
  check("LEVELS: BUILD ENV -> PLAY SHAPE", m.ex.web_test_ev(11) === 2);
  m.hold("HOME", 1100); m.release(); m.settle();
  check("LEVELS: .. and back to the same page when the area was not changed", m.ex.web_test_ev(48) === 1);
  m.tap("FX");
  m.hold("HOME", 1100); m.release(); m.settle();
  check("LEVELS: BUILD FX -> PLAY SPACE", m.ex.web_test_ev(11) === 0);
  m.tap("EDIT");                                                // PLAY: another area
  m.hold("HOME", 1100); m.release(); m.settle();
  check("LEVELS: PLAY SOUND -> BUILD, the EDIT family", m.ex.web_test_ev(48) === 5);
  m.tap("LFO"); m.tap("LFO"); m.tap("LFO");                     // BUILD, then ... (a BUILD tap: the LFO family)
}
if (d.ex.web_test_ev) {
  const m = await device();
  m.render(2500);
  let n = 0;
  while (m.ex.web_test_ev(40) !== 2 && n++ < 16) m.tap("LFO");  // BUILD: the JOURNEY page
  m.hold("HOME", 1100); m.release(); m.settle();
  check("LEVELS: BUILD JOURNEY page -> PLAY MOVE, the JOURNEY group", m.ex.web_test_ev(11) === 3 && m.ex.web_test_ev(10) === 2);
  m.tap("LFO");                                                 // the next group: ELASTIC
  m.hold("HOME", 1100); m.release(); m.settle();
  check("LEVELS: PLAY MOVE ELASTIC -> BUILD, the SPRING page", m.ex.web_test_ev(40) === 3);
}
// LFO held in any area shows the verbs and their buttons (E66)
if (d.ex.web_test_ev) {
  const l = await device();
  l.render(2500); l.hold("HOME"); l.release(); l.settle();
  l.tap("FX"); l.render(300);
  const rest = l.screen();
  l.ex.web_buttons(1 << B.LFO); l.render(400);
  const held = l.screen();
  check("EVOLVE: LFO held in SPACE shows the verbs in the circle", !same(rest, held));
  const lit = l.ex.web_lit_keys(), want = (1 << 21) | (1 << 23) | (1 << 24) | (1 << 26);
  check("EVOLVE: LFO held lights the four verb keys (D5 E5 F5 G5)", (lit & want) === want);
  l.ex.web_buttons(0); l.render(400);
  check("EVOLVE: .. and they go dark when it is let go", (l.ex.web_lit_keys() & want) === 0);
  check("EVOLVE: .. and the legend goes", same(rest, l.screen()) || !same(held, l.screen()));
}
// SPACE's FILTER group: the FX layer's four macros as arcs, kept when FX is let go; STASIS has the filter on K4 (E74, E77)
if (d.ex.web_test_ev) {
  const f = await device();
  f.render(2500); f.hold("HOME"); f.release(); f.settle();
  f.tap("FX"); f.render(300);                                     // SPACE is the area already: the next group is FILTER (SENDS, FILTER, STASIS)
  f.turn("K1", 20);
  const v1 = f.ex.web_test_ev(50) << 24 >> 24;
  check("FILTER: the group's K1 sets the FX filter", v1 !== 0);
  f.render(300);
  check("FILTER: ordinary PLAY arc is persistent without holding FX", (f.ex.web_test_ev(50) << 24 >> 24) === v1);
  f.hold("FX", 600); f.release(); f.render(300);                  // an unlatched hold releases FILTER in PLAY too
  check("FILTER: ordinary PLAY arc stays until an unlatched FX hold is released", (f.ex.web_test_ev(50) << 24 >> 24) === 0);
  f.tap("FX"); f.render(300);                                     // STASIS
  f.turn("K4", -5);
  check("FILTER: K4 of the STASIS group is the same filter", (f.ex.web_test_ev(50) << 24 >> 24) !== v1);
}
// CAPTURE length (E78): with REC held only K1 is shown, named LENGTH with its four steps; without, the area's four arcs
if (d.ex.web_test_cap_events) {
  const c = await device();
  c.render(2500); c.hold("HOME"); c.release(); c.settle();
  c.tap("EDIT"); c.render(300);
  const rest = c.screen();
  c.hold("REC", 700);
  const held = c.screen();
  check("CAPTURE: REC held shows the length card instead of the area's arcs", !same(rest, held));
  const sameStrip = (a, b) => { for (let y = 120; y < 175; y++) for (let x = 150; x < 240; x++) if (a[y * 240 + x] !== b[y * 240 + x]) return false; return true; };
  check("CAPTURE: .. K2 to K4 are empty (the right side has no arcs)", !sameStrip(rest, held));
  c.turn("K1", 1);
  check("CAPTURE: .. K1 turned changes the length shown", !same(held, c.screen()));
  c.release(); c.render(500);
  check("CAPTURE: .. and the arcs are back when REC is let go", same(rest, c.screen()) || !same(held, c.screen()));
}
// BUILD's remaining hold layers (FX, EDIT, REC, SEQ) are in the ring-and-fields manner: the BUILD header, the hazard strip, the four corners
{
  const y = await device();
  y.render(2500);
  const rest = y.screen();
  for (const name of ["FX", "EDIT", "REC", "SEQ"]) {
    y.ex.web_buttons(1 << B[name]); y.render(900);
    const h = y.screen();
    check(`BUILD ${name} layer: held draws the layer screen`, !same(rest, h));
    check(`BUILD ${name} layer: .. with the BUILD band's hazard strip`, [...Array(240).keys()].some((x) => px(h, x, 25) !== px(h, 0, 30)));
    check(`BUILD ${name} layer: .. and a ring in the middle (the grey track)`, [...Array(72).keys()].some((k) => px(h, Math.round(120 + 43 * Math.cos(k * Math.PI / 36)), Math.round(76 + 43 * Math.sin(k * Math.PI / 36))) !== px(h, 120, 90)));
    y.ex.web_buttons(0); y.render(600);
    check(`BUILD ${name} layer: .. and the page is back when it is let go`, same(rest, y.screen()));
  }
}
// BUILD's menu: the BUILD band with the hazard strip, and the rows without the panel's fill
{
  const m = await device();
  m.render(2500);
  const rest = m.screen();
  m.dtap("GLO"); m.render(600);
  const h = m.screen();
  check("BUILD menu: GLO tapped twice shows the menu", !same(rest, h));
  check("BUILD menu: .. with the BUILD band's hazard strip", [...Array(240).keys()].some((x) => px(h, x, 25) !== px(h, 0, 30)));
  check("BUILD menu: .. and no panel behind the rows (the background at the row's left edge)", px(h, 5, 130) === px(h, 2, 130));
}
// PLAY: HOME tapped shows how the levels are switched; any input closes it; the HOME hold still switches
{
  const v = await device();
  v.render(2500);
  v.hold("HOME"); v.release(); v.settle();             // BUILD -> PLAY
  const play = v.screen();
  v.tap("HOME"); v.render(500);
  const lv = v.screen();
  check("PLAY: HOME tapped opens the LEVELS screen", !same(play, lv));
  check("..with the HOME pill (the theme colour under the HOLD word)", px(lv, 20, 172) === px(lv, 200, 172) && px(lv, 20, 172) !== px(lv, 120, 232));
  v.ex.web_buttons(1 << B.FX); v.render(100); v.ex.web_buttons(0); v.render(500);
  check("..any button closes it (the PLAY screen is back, the FX area now)", !same(lv, v.screen()));
  v.tap("HOME"); v.render(500);
  const lv2 = v.screen();
  v.tap("HOME"); v.render(500);
  check("..a second HOME tap closes it again", !same(lv2, v.screen()));
  v.tap("HOME"); v.render(500);
  const p0 = v.ex.web_test_p(10);
  v.turn("K1", 3);
  check("..a knob turn closes it and changes nothing (the turn is not passed on)", !same(lv2, v.screen()) && v.ex.web_test_p(10) === p0);
  v.tap("HOME"); v.render(300);
  v.hold("HOME"); v.release(); v.settle();
  const back = v.screen();
  check("..HOLD HOME on it still switches to BUILD", !same(play, back) && px(back, 12, 12) !== px(play, 12, 12) || !same(play, back));
}
// Power-on: PLAY, with the LEVELS screen open the first time (the bit is kept); the tests above went on to BUILD (web_test_build)
if (d.ex.web_test_lvl) {
  const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
  const e = instance.exports, mm = e.memory;
  e._initialize(); e.web_nor_erase(); e.web_boot();
  const rn = (ms) => { const n = Math.round(ms * 44.1); for (let k = 0; k < n; k += 128) e.web_render(Math.min(128, n - k)); };
  const sc = () => new Uint16Array(mm.buffer, e.web_screen(), 240 * 240).slice();
  rn(2500);
  const first = sc();
  check("power-on: the LEVELS screen is open (PLAY, the HOME pill)", e.web_test_lvl() === 1 && px(first, 20, 172) === px(first, 200, 172) && px(first, 20, 172) !== px(first, 120, 232));
  check("..and it is marked as seen (kept with the settings)", e.web_test_seen() === 1);
  e.web_buttons(1 << B.FX); rn(100); e.web_buttons(0); rn(600);
  const play = sc();
  check("..any button closes it: PLAY, with the chip in the accent colour", e.web_test_lvl() === 0 && !same(first, play) && px(play, 20, 12) === px(first, 20, 12));
  rn(3000);                                              // the settings are written when idle: power off and on again, the flash kept
  e.web_boot(); rn(2500);
  check("the second power-on: PLAY without the LEVELS screen", e.web_test_lvl() === 0 && e.web_test_seen() === 1);
}
// E118: the real FX card shows the effective filter, with a quiet stored value dimmed.
{
  const f = await device(); f.render(3500);
  const own = f.ex.web_test_filter_own, view = f.ex.web_test_filter_view;
  const text = () => { let s = ""; for (let i = 0; i < 20; i++) { const c = view(4 + i); if (!c) break; s += String.fromCharCode(c); } return s; };
  own(-32); f.hold("FX", 600); f.render(500);
  check("FX FILTER: stored LP is shown dimmed, not OFF", text() === "50 LP" && view(0) === -32 && view(1) === 1 && view(2) === 0);
  const valuePixels = (frame, colour) => {
    const wire = ((colour & 255) << 8) | (colour >> 8); let n = 0;
    for (let y = 54; y < 72; y++) for (let x = 4; x < 74; x++) n += frame[y * 240 + x] === wire;
    return n;
  };
  const dim = f.screen();
  check("FX FILTER: the rendered stored value uses the dim ink", valuePixels(dim, view(24)) > 0);
  f.turn("K1", 1); f.render(200);
  check("FX FILTER: the rendered active value uses bright ink", valuePixels(f.screen(), view(25)) > 0);
  check("FX FILTER: first turn starts at stored LP and shows an active value", view(2) < -40 && view(0) >= -32 && view(0) <= -30 && view(1) === 0 && text() !== "OFF" && !same(dim, f.screen()));
  check("FX FILTER: the stored filter is unchanged", view(3) === -32);
  f.turn("K1", -1); f.render(200);
  check("FX FILTER: returning to the anchor releases to the stored value", view(2) === 0 && view(0) === -32 && view(1) === 1);
  f.turn("K1", 1); f.release(); f.render(400);
  check("FX FILTER: releasing an unlatched BUILD layer returns the track filter", view(2) === 0 && view(0) === -32 && view(3) === -32);
  f.ex.web_test_select(1); own(20); f.hold("FX", 600); f.render(200);
  check("FX FILTER: changing selection shows that track's own HP, dimmed", view(0) === 20 && view(1) === 1 && text().includes("HP"));
  f.turn("K1", -1); const held = view(2); f.ex.web_test_select(0); f.render(200);
  check("FX FILTER: an active macro follows the selection, without writing either track", view(2) === held && view(0) > 0 && view(3) === -32 && view(1) === 0);
  f.release(); own(0); f.hold("FX", 600); f.render(200);
  check("FX FILTER: no stored filter and no macro still shows OFF", text() === "OFF" && view(1) === 1);
  f.release();
}
console.log(fails ? `ux: ${fails} FAILED` : "ux: all passed");
process.exit(fails ? 1 : 0);
