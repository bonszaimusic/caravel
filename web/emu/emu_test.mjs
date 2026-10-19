// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// The browser build (build/emu/felucca.wasm, web/emu/felucca_web.c) in Node, as the worklet runs it. After X0X's
// tests/host/emu_test.mjs (charlesvestal/fm1-x0x, GPL-3.0).
//   node web/emu/emu_test.mjs build/emu/felucca.wasm [NATIVE.f32]
// Checks: it boots (splash, then HOME on the screen, the HOME LED; the power-on LED sweep under the splash, over by
// 1 s), a held key sounds and lights its LED, PLAY
// starts the sequencer (PLAY's green LED), a user preset saved from the panel (SAVE, OCT+, OCT+) reaches the flash,
// and the flash sectors kept as the page keeps them bring it back in a fresh instance; the same gestures twice
// give the same audio (deterministic), with NATIVE.f32 (web/emu/native_check.c) the native build's, bit for bit.
// Then the cost: 1 s of a heavy song (FM6, PHYS, GRAIN chords, DRUM) against real time (BENCH_MAX, default 0.5:
// fails above half of real time on this machine).
import fs from "fs";
import { storeSectors, restoreSectors, midiKnob, KNOB_CC, MASTER_CC } from "./worklet.js";

const wasmPath = process.argv[2] || "build/emu/felucca.wasm", nativePath = process.argv[3];
const bytes = fs.readFileSync(wasmPath);
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12, OCTUP: 13 };
const EN = { SELECT: 0, ALGO: 1, PRESETS: 2, K1: 3, K2: 4, K3: 5, K4: 6 };
let fails = 0;
const check = (what, ok) => { console.log(`emu: ${what.padEnd(76)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };

async function device(saved) {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports, mem = ex.memory;
  ex._initialize();
  ex.web_nor_erase();
  if (saved) restoreSectors(new Uint8Array(mem.buffer, ex.web_nor(), ex.web_nor_size()), saved);
  ex.web_boot();
  const d = { ex, mem, peak: 0, sig: 0, rec: false, frames: 0, kept: [] };
  d.render = (ms) => {
    const n = Math.round(ms * 44.1);
    for (let k = 0; k < n; k += 128) {
      const m = Math.min(128, n - k);
      ex.web_render(m);
      const l = new Float32Array(mem.buffer, ex.web_out_l(), m), r = new Float32Array(mem.buffer, ex.web_out_r(), m);
      for (let i = 0; i < m; i++) {
        d.peak = Math.max(d.peak, Math.abs(l[i]), Math.abs(r[i]));
        if (d.rec) {
          d.sig = (d.sig * 31 + Math.round(l[i] * 32768) + 7 * Math.round(r[i] * 32768)) % 1000000007;
          d.kept.push(l[i], r[i]);
        }
      }
      d.frames += m;
    }
  };
  d.tap = (b) => { ex.web_buttons(1 << b); d.render(60); ex.web_buttons(0); d.render(60); };
  d.turn = (role, n) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(role, Math.sign(n)); d.render(40); } };
  Object.defineProperty(d, "samples", { get: () => new Float32Array(d.kept) });
  d.screen = () => new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240).slice();
  return d;
}
const colours = (fb) => new Set(fb).size;

// ---- boot, keys, screen, LEDs
const a = await device(null);
a.render(200);
const splash = colours(a.screen());
{ // the power-on LED sweep (hal/fm1_led_anim.h) under the splash: at 200 ms its head on the middle of the keyboard, the
  // keys it passed glowing (DIM HI), the ones ahead dark, the buttons not yet; the UI's LEDs untouched until it ends
  const p = a.ex.web_anim_levels(), lv = p ? Array.from(new Uint8Array(a.mem.buffer, p, 41)) : [];
  const keys = lv.slice(14), head = keys.indexOf(Math.max(...keys.map((q) => q & 127)) | 128);
  check(`power-on LED sweep: at 200 ms the head on key ${head}, glow behind, dark ahead, buttons dark`,
        lv.length === 41 && head >= 8 && head <= 18 && keys.slice(0, head - 6).every((q) => q === 128) &&
        keys.slice(head + 3).every((q) => q === 0) && lv.slice(0, 14).every((q) => q === 0) &&
        a.ex.web_lit_keys() === 0 && a.ex.web_lit_buttons() === 0);
}
a.render(800);
check("  over by 1 s (~0.70 s)", a.ex.web_anim_levels() === 0);
const home = a.screen();
check(`boots: the splash, then HOME (${colours(home)} colours), the HOME LED lit`,
      splash > 1 && colours(home) > 4 && (a.ex.web_lit_buttons() >> B.HOME & 1) === 1);
check("silent at rest", a.peak === 0);
a.ex.web_keys(1 << 7);                                        // C4
a.render(300);
const keyLit = (a.ex.web_lit_keys() >> 7) & 1;
a.ex.web_keys(0);
a.render(400);
check(`a key sounds (peak ${a.peak.toFixed(3)}) and lights its LED`, a.peak > 0.01 && keyLit === 1);
a.render(1500);
a.peak = 0;
a.ex.web_midi(0x90, 64, 100);                                  // Web MIDI: E4 on channel 1
a.render(300);
a.ex.web_midi(0x80, 64, 0);
a.render(300);
check(`a MIDI note in (USB-MIDI's queue) sounds (peak ${a.peak.toFixed(3)})`, a.peak > 0.01);
const draws = a.ex.web_screen_draws();
a.turn(EN.K1, 3);
check("KNOB 1 redraws the screen", a.ex.web_screen_draws() > draws);
a.tap(B.PLAY);
a.render(1000);
check("PLAY: the sequencer runs, PLAY's green LED", a.ex.web_playing() === 1 && (a.ex.web_lit_buttons() >> 14 & 1) === 1);
a.tap(B.PLAY);
a.render(300);
check("PLAY again stops it", a.ex.web_playing() === 0);
a.ex.web_buttons(1 << B.FX);                                  // #119: FX held, its map: the effects breathe
a.render(800);
const brK = a.ex.web_breath_keys(), brB = a.ex.web_breath_buttons(), litK = a.ex.web_lit_keys();
a.ex.web_buttons(0);
a.render(300);
check("FX held: its button and the effects' keys breathe, none lit; let go: nothing",
      (brB >> B.FX & 1) === 1 && brK !== 0 && (brK & litK) === 0 && litK === 0 &&
      a.ex.web_breath_keys() === 0 && a.ex.web_breath_buttons() === 0);
{ // the page's breathing peaks against the firmware's (hal/fm1_input.h FM1_LED_BREATH_PK / _LO, /256 of lit): the
  // colour at 50 % between the dark and the lit one by the LED's share, gamma-encoded (sRGB), within 0.06
  const html = fs.readFileSync(new URL("./index.html", import.meta.url), "utf8");
  const hal = fs.readFileSync(new URL("../../firmware/hal/fm1_input.h", import.meta.url), "utf8");
  const pk = (n) => +hal.match(new RegExp(`#define ${n} (\\d+)u`))[1] / 256;
  const srgb = (x) => x <= 0.0031308 ? 12.92 * x : 1.055 * x ** (1 / 2.4) - 0.055;
  const red = (c) => parseInt(c.slice(1, 3), 16);
  const share = (name, lit) => {                        // (the red channel: the peak's share of dark .. lit)
    const m = html.match(new RegExp(`@keyframes ${name} \\{ 0%, 100% \\{ fill: (#[0-9a-f]{6}); \\} 50% \\{ fill: (#[0-9a-f]{6})`));
    return (red(m[2]) - red(m[1])) / (lit - red(m[1]));
  };
  const hi = srgb(pk("FM1_LED_BREATH_PK")), lo = srgb(pk("FM1_LED_BREATH_PK_LO")), litBtn = 0xff, litSlit = 0xec;
  const got = [share("breath-btn", litBtn), share("breath-slit", litSlit), share("breath-btn-lo", litBtn), share("breath-slit-lo", litSlit)];
  check(`the page's breath peaks ${got.map((x) => x.toFixed(2)).join(" ")} ~ ${hi.toFixed(2)} (HI) ${lo.toFixed(2)} (LO)`,
        Math.abs(got[0] - hi) <= 0.06 && Math.abs(got[1] - hi) <= 0.06 && Math.abs(got[2] - lo) <= 0.06 &&
        Math.abs(got[3] - lo) <= 0.06);
}

// ---- master FX edges through the actual REC + FX/key gestures
const fx = await device(null);
fx.render(1000);
fx.tap(B.REC);
fx.ex.web_buttons(1 << B.FX); fx.render(700);
fx.ex.web_keys(1 << 7); fx.render(25); // C4: default LPF, short press
fx.ex.web_keys(0); fx.render(20);
check("FX special: REC + held FX/key records short press and release", fx.ex.web_test_fx_events() === 2 &&
      fx.ex.web_test_fx_live() === 0 && fx.ex.web_playing() === 1);
fx.ex.web_buttons(0); fx.render(100);
fx.tap(B.REC); // disarm
fx.tap(B.PLAY); // stop
check("FX special: STOP releases sequencer FX and retains both edges", fx.ex.web_test_fx_replay() === 0 && fx.ex.web_test_fx_events() === 2);
fx.tap(B.PLAY);
let fxHeard = false;
for (let i = 0; i < 400; i++) { fx.render(5); fxHeard ||= fx.ex.web_test_fx_replay() !== 0; }
check("FX special: next pass replays without FX button or live key", fxHeard && fx.ex.web_test_fx_live() === 0);
fx.tap(B.PLAY);

// ---- edit the captured FX edges through SEQ > AUTOMATION's real knobs
const fxOn = fx.ex.web_test_fx_edge(1), fxOff = fx.ex.web_test_fx_edge(0);
// Space page taps so the panel's double-tap shortcut does not lock SEQ TOOLS.
for (let i = 0; i < 3; i++) { fx.tap(B.SEQ); fx.render(320); }
fx.turn(EN.K1, 2); // first FX edge after PLAY / QUANTIZE
fx.turn(EN.K2, 1);
check("FX edit: KNOB2 moves ON one step and leaves OFF unchanged",
      fx.ex.web_test_fx_edge(1) === fxOn + 256 && fx.ex.web_test_fx_edge(0) === fxOff);
fx.turn(EN.K4, 1);
check("FX edit: KNOB4 adjusts unsigned ON phase by 1/256",
      fx.ex.web_test_fx_edge(1) === fxOn + 256 + ((fxOn & 255) < 255 ? 1 : 0));
fx.turn(EN.K1, -1); // ON moved after the short take's OFF; select that OFF
fx.turn(EN.K2, 2);
check("FX edit: independent OFF move changes duration and retains both edges",
      fx.ex.web_test_fx_edge(0) === fxOff + 512 && fx.ex.web_test_fx_events() === 2);

// ---- a user preset saved from the panel, kept across a fresh instance
const w0 = a.ex.web_flash_writes_count();
a.tap(B.SAVE);
a.tap(B.OCTUP);
a.tap(B.OCTUP);
a.render(200);
const saved = storeSectors(new Uint8Array(a.mem.buffer, a.ex.web_nor(), a.ex.web_nor_size()));
check(`SAVE, OCT+, OCT+: user preset 1 written (${Object.keys(saved).length} sectors kept)`,
      a.ex.web_test_user_preset(0) === 1 && a.ex.web_flash_writes_count() > w0 && Object.keys(saved).length > 0);
const kept = JSON.parse(JSON.stringify(Object.fromEntries(     // as the page keeps them (base64 in localStorage)
  Object.entries(saved).map(([k, v]) => [k, Buffer.from(v).toString("base64")]))));
const b = await device(Object.fromEntries(Object.entries(kept).map(([k, v]) => [k, new Uint8Array(Buffer.from(v, "base64"))])));
b.render(600);
const c = await device(null);
c.render(600);
check("a fresh instance with the kept sectors has it; one without does not",
      b.ex.web_test_user_preset(0) === 1 && c.ex.web_test_user_preset(0) === 0);

// ---- four parts saved from the real PROJECT UI during playback, committed only after STOP
const live = await device(null);
live.render(1000);
live.ex.web_test_heavy();
for (let i = 0; i < 4; i++) live.tap(B.SAVE);    // USER -> PRESETS -> PHRASES -> PROJECT
live.tap(B.PLAY);
const writesBeforeLive = live.ex.web_flash_writes_count();
let liveOk = true;
for (let slot = 0; slot < 4; slot++) {
  live.ex.web_test_scene_fx(20 + slot * 20);
  if (slot) live.turn(EN.K1, 1);
  live.turn(EN.K4, 1);                          // pick SAVE
  live.tap(B.OCTUP);                           // NAME
  live.tap(B.OCTUP);                           // capture current part
  liveOk &&= live.ex.web_playing() === 1 && live.ex.web_test_project_used(slot) === 1 &&
             live.ex.web_test_project_ram_mask() === (1 << (slot + 1)) - 1 &&
             live.ex.web_flash_writes_count() === writesBeforeLive;
}
check("PROJECT UI: live-save A-D without stopping or any flash write", liveOk);
live.ex.web_test_scene_fx(123);
live.turn(EN.K1, -3);                          // newly saved A, available without STOP
live.turn(EN.K3, 1);
live.tap(B.OCTUP);
live.render(100);
check("PROJECT UI: live LOAD accepts a newly RAM-saved slot", live.ex.web_playing() === 1 &&
      live.ex.web_test_project_ram_mask() === 15 && live.ex.web_flash_writes_count() === writesBeforeLive);
live.render(2500);
check("PATTERN live LOAD keeps current track and global FX", live.ex.web_test_scene_fx_value() === (123 | 123 << 8));
live.ex.web_test_live_load_mode(1);
live.tap(B.OCTUP);
live.render(2500);
check("FULL live LOAD restores track and global FX from RAM without STOP", live.ex.web_playing() === 1 &&
      live.ex.web_test_scene_fx_value() === (20 | 20 << 8) && live.ex.web_flash_writes_count() === writesBeforeLive);
live.tap(B.PLAY);
live.render(200);
check("STOP: all A-D RAM snapshots committed automatically", live.ex.web_playing() === 0 &&
      live.ex.web_test_project_ram_mask() === 0 && live.ex.web_flash_writes_count() > writesBeforeLive);
const liveSaved = storeSectors(new Uint8Array(live.mem.buffer, live.ex.web_nor(), live.ex.web_nor_size()));
const liveReboot = await device(liveSaved);
liveReboot.render(600);
check("A-D remain saved in a fresh emulator instance", [0, 1, 2, 3].every((i) => liveReboot.ex.web_test_project_used(i) === 1) &&
      liveReboot.ex.web_test_project_ram_mask() === 0);

// ---- STEP REC from the panel while the playhead keeps running
const stepLive = await device(null);
stepLive.render(1000);
stepLive.ex.web_test_heavy();
stepLive.tap(B.SEQ);
stepLive.turn(EN.K1, 8);
stepLive.tap(B.PLAY);
const modeGesture = () => {
  stepLive.ex.web_buttons(1 << B.SEQ); stepLive.render(60);
  stepLive.ex.web_buttons((1 << B.SEQ) | (1 << B.REC)); stepLive.render(60);
  stepLive.ex.web_buttons(1 << B.SEQ); stepLive.render(60);
  stepLive.ex.web_buttons(0); stepLive.render(60);
};
modeGesture();
check("SEQ + REC: STEP REC armed during playback, independent cursor at step 8",
      stepLive.ex.web_test_step_record_mask() === 1 && stepLive.ex.web_playing() === 1 && stepLive.ex.web_test_cursor() === 8);
stepLive.ex.web_keys(1 << 1); stepLive.render(60);
stepLive.ex.web_keys((1 << 1) | (1 << 7)); stepLive.render(60);
check("STEP REC: physical keys write a chord at the cursor, playhead unchanged",
      stepLive.ex.web_test_step_count(8) === 2 && stepLive.ex.web_test_cursor() === 8 &&
      stepLive.ex.web_test_step_count(stepLive.ex.web_test_seq_idx()) === 4);
stepLive.ex.web_keys(0); stepLive.render(60);
const headBeforeStep = stepLive.ex.web_test_seq_idx();
stepLive.render(600);
check("STEP REC: releasing advances cursor once, playback advances separately",
      stepLive.ex.web_test_cursor() === 9 && stepLive.ex.web_playing() === 1 &&
      stepLive.ex.web_test_seq_idx() !== headBeforeStep);
stepLive.turn(EN.K1, -5); // choose step 4, away from the live playhead
modeGesture();
const beforeLiveNotes = Array.from({length: 16}, (_, i) => stepLive.ex.web_test_step_note(i, 3));
stepLive.ex.web_keys(1 << 26); stepLive.render(60); // G5 is absent from the prepared chords
stepLive.ex.web_keys(0); stepLive.render(60);
check("SEQ + REC again: LIVE REC keeps the cursor, records at the playhead",
      stepLive.ex.web_test_step_record_mask() === 0 && stepLive.ex.web_playing() === 1 &&
      stepLive.ex.web_test_cursor() === 4 && stepLive.ex.web_test_step_note(4, 3) === beforeLiveNotes[4] &&
      beforeLiveNotes.some((note, i) => stepLive.ex.web_test_step_note(i, 3) !== note));
stepLive.tap(B.REC);
check("normal REC disarms without stopping playback", stepLive.ex.web_playing() === 1 &&
      !(stepLive.ex.web_lit_buttons() & (1 << B.REC)));
// ---- the panel's knobs from a MIDI controller: the page turns them by midiKnob (worklet.js), as a drag does
{
  const last = [], cc = KNOB_CC + EN.PRESETS, k = (c, v, mode) => midiKnob(last, c, v, mode);
  const a = [k(cc, 40), k(cc, 43), k(cc, 41), k(MASTER_CC, 127), k(74, 10), k(28, 10)];
  check("absolute (the default) CC 22 (PRESETS): its first value takes its place, then +3, -2; CC 27 MASTER; CC 74, 28 the firmware's",
        a[0].role === EN.PRESETS && a[0].n === 0 && a[1].n === 3 && a[2].n === -2 && a[3].master === 1023 &&
        a[4] === null && a[5] === null);
  const n = (mode, vs) => vs.map((v) => k(cc, v, mode).n).join();
  check("relative CC 22: 64+-n, two's complement, sign bit; CC 27 absolute in each",
        n("bin", [65, 63, 64, 70, 0, 127]) === "1,-1,0,6,-64,63" && n("twos", [1, 127, 0, 63, 64]) === "1,-1,0,63,-64" &&
        n("sign", [1, 65, 0, 64, 127]) === "1,-1,0,0,-63" &&
        ["bin", "twos", "sign"].every((m) => k(MASTER_CC, 0, m).master === 0 && k(MASTER_CC, 127, m).master === 1023) &&
        k(20, 65, "bin").role === EN.SELECT && k(26, 65, "bin").role === EN.K4 && k(19, 65, "bin") === null);
  const t = await device(null), u = await device(null);             // u: left alone
  t.render(1200);
  u.render(1200);
  t.render(40);
  t.turn(EN.PRESETS, 3);
  t.render(300);
  u.render(460);
  const ts = t.screen(), us = u.screen();
  for (const [mode, vs] of [["abs", [64, 65, 66, 67]], ["bin", [65, 65, 65]], ["twos", [1, 1, 1]], ["sign", [1, 1, 1]]]) {
    const m = await device(null), knob = [];                   // a controller's knob: three detents
    m.render(1200);
    for (const v of vs) {
      const r = midiKnob(knob, cc, v, mode);
      if (r.n) m.ex.web_enc(r.role, r.n);
      m.render(40);
    }
    if (mode !== "abs") m.render(40);                           // as many 40 ms steps as the four absolute values
    m.render(300);
    const ms = m.screen();
    check(`CC 22 ${mode} ${vs.join(" ")} turns PRESETS as three detents of the panel's knob do`,
          ms.some((v, i) => v !== us[i]) && ms.every((v, i) => v === ts[i]));
  }
}

// ---- the new bass engine runs in the matching wasm, all eight dry presets
for (let preset = 0; preset < 8; preset++) {
  const h = await device(null);
  h.render(1000);
  h.ex.web_test_hades(preset);
  h.ex.web_midi(0x90, 36, 100);
  h.render(500);
  const peak = h.peak;
  h.ex.web_midi(0x80, 36, 0);
  h.render(2000);
  h.peak = 0;
  h.render(500);
  check(`HADES preset ${preset + 1}: MIDI bass sounds, note-off leaves silence`, peak > 0.005 && h.peak < 0.001);
}

// ---- coupled drone presets in the actual WebAssembly render path
for (let preset = 0; preset < 8; preset++) {
  const d = await device(null);
  d.render(1000);
  d.ex.web_test_drone(preset);
  d.ex.web_midi(0x90, 48, 100);
  d.render(6000);
  if (preset >= 5) {
    for (const note of [60, 67]) {
      d.peak = 0;
      d.ex.web_midi(0x90, note, 100);
      d.render(2000);
      const gesturePeak = d.peak;
      d.ex.web_midi(0x80, note, 0);
      d.render(2000);
      d.peak = 0;
      d.render(500);
      check(`DRONE cell preset ${preset + 1}: gesture ${note} released, anchor 48 still sounds`,
            gesturePeak > 0.005 && gesturePeak < 1 && d.peak > 0.005 && d.peak < 1);
    }
  }
  const peak = d.peak;
  d.ex.web_midi(0x80, 48, 0);
  d.render(8000);
  d.peak = 0;
  d.render(500);
  check(`DRONE preset ${preset + 1}: held MIDI note sounds, long release reaches silence`,
        peak > 0.005 && peak < 1 && d.peak < 0.001);
}

// ---- deterministic: the same gestures, the same samples
async function song() {
  const d = await device(null);
  d.render(500);
  d.ex.web_test_heavy();
  d.tap(B.PLAY);
  d.rec = true;
  d.render(2000);
  return d;
}
const s1 = await song(), s2 = await song();
check(`the heavy song plays (peak ${s1.peak.toFixed(3)}), twice the same samples`, s1.peak > 0.05 && s1.sig === s2.sig);
if (nativePath) {                                             // web/emu/native_check.c: the same song, built with cc
  const nat = new Float32Array(new Uint8Array(fs.readFileSync(nativePath)).buffer);
  const web = s1.samples;
  let diff = nat.length === web.length ? 0 : Infinity;
  for (let i = 0; i < web.length && diff === 0; i++) if (nat[i] !== web[i]) diff = i + 1;
  check(`the same song from the native build (cc): ${web.length / 2} frames, ${diff === 0 ? "bit for bit" : "differs at " + diff}`, diff === 0);
}

// ---- the cost: 1 s of the heavy song against real time (the best of 3, after 2 s to settle)
let best = Infinity;
s1.rec = false;
for (let i = 0; i < 3; i++) {
  const t0 = performance.now();
  s1.render(1000);
  best = Math.min(best, performance.now() - t0);
}
const ratio = best / 1000, max = +(process.env.BENCH_MAX || 0.5);
check(`1 s of the heavy song (FM6 PHYS GRAIN chords + DRUM) in ${best.toFixed(1)} ms: ${(ratio * 100).toFixed(1)} % of real time`, ratio < max);
console.log(fails ? `emu: ${fails} FAILED` : "emu: all ok");
process.exit(fails ? 1 : 0);

