// SPDX-License-Identifier: GPL-3.0-only
// Regression for the owner's FM-1 report: FX FILTER must belong to each track,
// not follow the selected track after an encoder change.
// Actual firmware UI/parameter code, not a JavaScript model.
// EMU_CFLAGS="-DCARAVEL_UX_LAYERS=1 -DCARAVEL_UX_EVOLVE=1 -DCARAVEL_UX_CAPTURE=1" \\
//   sh web/emu/build.sh && node web/emu/fx_filter_track_test.mjs build/emu/felucca.wasm
import fs from "fs";

const { instance } = await WebAssembly.instantiate(
  fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm"), {}
);
const ex = instance.exports;
const B = { FX: 0, EDIT: 4, HOME: 6 };
const E = { K1: 3 };
const P_FILT = 105;  // existing stable stored track param (tests/trackfilter_test.c)
let fails = 0;
const check = (what, ok) => {
  console.log(`filter-track: ${what.padEnd(74)} ${ok ? "ok" : "FAIL"}`);
  if (!ok) fails++;
};
const render = (ms) => {
  const n = Math.round(ms * 44.1);
  for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k));
};
const tap = (b) => { ex.web_buttons(1 << b); render(75); ex.web_buttons(0); render(160); };
const track = (t) => { ex.web_test_select(t); render(120); return ex.web_test_p(P_FILT); };
const turn = (enc, steps) => { ex.web_enc(enc, steps); render(240); };
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
if (ex.web_test_build) ex.web_test_build();
render(2800);
check("fresh BUILD tracks have filter OFF", [0, 1, 2, 3].every(t => track(t) === 0));

// The actual reported path: FX held, T4 FILTER K1 down; release FX,
// select another track and go back. T4 must continue to have its LP cutoff.
track(3);
ex.web_buttons(1 << B.FX); render(800);
turn(E.K1, -16);
ex.web_buttons(0); render(350);
const t4 = track(3);
check("BUILD FX held K1 writes T4's P_FILT (low-pass)", t4 < 0);
check("T1/T2/T3 remain OFF", [0, 1, 2].every(t => track(t) === 0));
track(0); render(350);
check("T4 retains the low-pass when a different track is selected", track(3) === t4);
check("return to T4 displays the same stored cutoff", ex.web_test_p(P_FILT) === t4);

// Verify a new FX gesture edits only the newly selected track.
track(0);
ex.web_buttons(1 << B.FX); render(800);
turn(E.K1, 10);
ex.web_buttons(0); render(300);
const t1 = track(0);
check("new FX K1 gesture writes only T1", t1 > 0 && track(3) === t4);
check("T2/T3 are still OFF", track(1) === 0 && track(2) === 0);

// PLAY SPACE/FILTER uses the same stored track property rather than
// a temporary global filter macro.
ex.web_buttons(1 << B.HOME); render(1300); ex.web_buttons(0); render(200);
track(2);
tap(B.EDIT);                         // move away from SPACE, so FX first tap selects SENDS
tap(B.FX); tap(B.FX);                 // SPACE group B = FILTER
turn(E.K1, -9);                      // PLAY K1 FILTER for T3
const t3 = track(2);
check("PLAY SPACE/FILTER K1 writes T3's filter", t3 < 0);
check("PLAY T3 edit never changes T4 or T1", track(3) === t4 && track(0) === t1);
ex.web_buttons(1 << B.HOME); render(1300); ex.web_buttons(0); render(250);
check("filter values survive PLAY-to-BUILD transition", track(2) === t3 && track(3) === t4);
console.log(fails ? `filter-track: ${fails} failed` : "filter-track: all passed");
process.exit(fails ? 1 : 0);
