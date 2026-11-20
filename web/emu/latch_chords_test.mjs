// SPDX-License-Identifier: GPL-3.0-only
// LATCH STACK is not for chords: a chord track (shape, HARMONY bus, EXT) falls back to NEW; a plain track keeps STACK.
//   node web/emu/latch_chords_test.mjs build/emu/felucca.wasm
import fs from "fs";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm"), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (ms) => { const n = Math.round(ms * 44.1); for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
render(2500);
let fails = 0; const check = (w, ok) => { console.log((ok ? "ok   " : "FAIL ") + w); if (!ok) fails++; };
ex.web_test_harm_reset(); ex.web_test_select(0); render(200);
ex.web_test_set_latch(0, 2); render(300); check("plain track: STACK stays", ex.web_test_latch(0) === 2);
ex.web_test_harm_bus(0); render(300); check("HARMONY bus chord: STACK becomes NEW", ex.web_test_latch(0) === 1);
ex.web_test_harm_reset(); ex.web_test_set_latch(1, 2); ex.web_test_harm_csrc(1, 1); render(300); check("CHORD EXT: STACK becomes NEW", ex.web_test_latch(1) === 1);
ex.web_test_harm_reset(); ex.web_test_set_latch(2, 2); render(300); check("after the role is plain again a new STACK is allowed", ex.web_test_latch(2) === 2);
ex.web_test_harm_reset(); ex.web_test_set_latch(3, 2); ex.web_test_harm_chrd(3, 3); render(300); check("a local chord shape keeps STACK", ex.web_test_latch(3) === 2);
console.log(fails ? fails + " FAILED" : "all ok");
