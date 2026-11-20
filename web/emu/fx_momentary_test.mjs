// SPDX-License-Identifier: GPL-3.0-only
// PLAY, SPACE area, FILTER group (the FX macros): LFO held = momentary, the values go back when LFO is let go.
//   node web/emu/fx_momentary_test.mjs build/emu/felucca.wasm
import fs from "fs";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm"), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (ms) => { const n = Math.round(ms * 44.1); for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
const tap = (b) => { ex.web_buttons(1 << b); render(70); ex.web_buttons(0); render(90); };
let fails = 0; const check = (w, ok) => { console.log((ok ? "ok   " : "FAIL ") + w); if (!ok) fails++; };
const fx = () => [0, 1, 2, 3].map((i) => ex.web_test_fxk(i));
render(2500);
tap(0); render(300);                                          // FX: the SPACE area (its FILTER group is the one shown)
// without LFO: the values stay
const a0 = fx(); for (let k = 3; k <= 6; k++) for (let i = 0; i < 8; i++) { ex.web_enc(k, 1); render(20); }
const a1 = fx(); console.log("plain turn:", a0.join(","), "->", a1.join(","));
check("a plain turn changes the macros and they stay", a1.some((v, i) => v !== a0[i]));
render(600); check("they stay after a while", fx().join() === a1.join());
// with LFO held: they go back
ex.web_buttons(1 << 3); render(400);
const b0 = fx(); for (let k = 3; k <= 6; k++) for (let i = 0; i < 8; i++) { ex.web_enc(k, 1); render(20); }
const b1 = fx(); console.log("mom mask:", ex.web_test_mom()); console.log("LFO held:", b0.join(","), "->", b1.join(","));
check("with LFO held the macros move", b1.some((v, i) => v !== b0[i]));
ex.web_buttons(0); render(500);
console.log("after release:", fx().join(","));
check("LFO let go: every macro is back where it was", fx().join() === b0.join());
console.log(fails ? fails + " FAILED" : "all ok");
