// SPDX-License-Identifier: GPL-3.0-only
// Actual WASM panel, mixer and tail; host timings do not measure the FM-1 CPU.
import fs from "node:fs";
import assert from "node:assert/strict";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm"), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot(); ex.web_test_build();
const values = () => [0, 1, 2, 3].map(k => ex.web_test_afterburner(k));
let peak = 0, frames = 0;
const render = ms => {
  let total = Math.round(ms * 44.1);
  while (total > 0) {
    const n = Math.min(total, 128);
    ex.web_render(n);
    for (const p of [ex.web_out_l(), ex.web_out_r()]) {
      const audio = new Float32Array(ex.memory.buffer, p, n);
      for (const x of audio) {
        assert(Number.isFinite(x), "finite stereo PCM");
        peak = Math.max(peak, Math.abs(x));
      }
    }
    frames += n; total -= n;
  }
};
const tap = b => { ex.web_buttons(1 << b); render(70); ex.web_buttons(0); render(160); };
const turn = (slot, delta) => { ex.web_enc(3 + slot, delta); render(180); };
render(2800);
assert.deepEqual(values(), [0, 30, 32, 70], "fresh session defaults");
tap(0);
// Repeated quick FX taps intentionally lock its held layer. SELECT navigates
// the FX pages without triggering that separate double-tap gesture.
for (let i = 0; i < 16 && !ex.web_test_afterburner(4); i++) { ex.web_enc(0, 1); render(200); }
assert.equal(ex.web_test_afterburner(4), 1, "AFTER reachable through FX and SELECT navigation");
turn(0, 1); turn(1, 1000); turn(2, 1000); turn(3, -1000);
assert.deepEqual(values(), [1, 60, 127, 0], "all four AFTER knobs work through panel input");
ex.web_stasis_test_type(2); ex.web_test_scene_fx(100);
ex.web_keys((1 << 9) | (1 << 13) | (1 << 16)); render(2000);
ex.web_keys(0); render(3000);
const energy5 = ex.web_test_afterburner_energy();
assert(energy5 > 0.01, "actual HALL return feeds Afterburner");
ex.web_stasis_test_type(3); render(100);
assert(ex.web_test_afterburner_energy() > 0.01, "HALL-to-STASIS switch preserves the independent tail");
render(25000);
const energy30 = ex.web_test_afterburner_energy();
render(30000);
const energy60 = ex.web_test_afterburner_energy();
assert(energy30 < energy5 && energy60 < energy30, "integrated 60-second tail decays");
assert(peak > 0 && peak <= 1.01, "finite bounded stereo output");
turn(0, -1); render(800);
assert.equal(ex.web_test_afterburner(5), 0, "OFF fades out and stops the DSP");
assert.equal(ex.web_test_afterburner_energy(), 0, "OFF clears the tail");
turn(0, 1);
assert.equal(ex.web_test_project_save(0), 0, "project saved");
ex.web_test_project_load(0); render(800);
assert.deepEqual(values(), [1, 60, 127, 0], "AFTER settings are part of the saved PROJECT (FX extension, fc016bf) and come back on LOAD");
console.log(JSON.stringify({test:"afterburner-wasm", frames, peak, energy5, energy30, energy60, hostOnly:true}));
console.log("Afterburner WASM: panel controls, HALL/STASIS spillover, 60s decay, OFF clearing and PROJECT-persistent LOAD PASS");
