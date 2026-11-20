// SPDX-License-Identifier: GPL-3.0-only
// PLAY HARM area: the chord selector (jump at the next bar line, progress kept), the LEFT countdown (longer / shorter / HOLD past 16). Through the firmware WASM.
//   node web/emu/harm_chord_select_test.mjs build/emu/felucca.wasm
import fs from "fs";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm"), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (ms) => { const n = Math.round(ms * 44.1); for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
const tap = (b) => { ex.web_buttons(1 << b); render(70); ex.web_buttons(0); render(90); };
render(2500);
let fails = 0; const check = (w, ok) => { console.log((ok ? "ok   " : "FAIL ") + w); if (!ok) fails++; };
const st = () => `chord ${ex.web_test_harm_hc(0)} left ${ex.web_test_harm_hc(2)} extra ${ex.web_test_harm_hc(3)} hold ${ex.web_test_harm_hc(4)} prog ${ex.web_test_harm_hc(5)}`;
function setup() {
  if (ex.web_playing()) { tap(10); render(400); }
  ex.web_test_harm_reset(); ex.web_test_harm_on(1); [[0,2],[9,1],[5,2],[7,1]].forEach(([r,d],i)=>ex.web_test_harm_prog(i,r,d)); ex.web_test_harm_role(0,1);
  tap(6); tap(1); render(300);                                  // HOME, SCL: the HARM area (PLAY)
}
function playTo(ms) { tap(10); render(ms); }
const firstChange = (from) => { for (let i = 0; i < 60; i++) { render(100); if (ex.web_test_harm_hc(0) !== from) return; } };
// 1: chord 0 (4 bars) half done -> chord 2 (4 bars): half done there
setup(); playTo(4100); console.log("before:", st());
let from = ex.web_test_harm_hc(0), eBefore = 4 - ex.web_test_harm_hc(2);
ex.web_enc(4, 2); firstChange(from); console.log("0 -> 2:", st());
check("jump 0 -> 2 lands in chord 2", ex.web_test_harm_hc(0) === 2);
check("progress kept: bar 3 of 4 in chord 0 at the bar line -> bar 3 of 4 in chord 2 (left 1)", ex.web_test_harm_hc(2) === 1);
// 2: chord 0 (4 bars) half done -> chord 1 (2 bars): second bar
setup(); playTo(4100); from = ex.web_test_harm_hc(0); ex.web_enc(4, 1); firstChange(from); console.log("0 -> 1:", st());
check("jump 0 -> 1 lands in chord 1", ex.web_test_harm_hc(0) === 1);
check("2-bar chord entered in its second bar (left 1)", ex.web_test_harm_hc(2) === 1);
// 2b: from the first bar of chord 0 to chord 1: its first bar
setup(); playTo(600); from = ex.web_test_harm_hc(0); ex.web_enc(4, 1); firstChange(from); console.log("start 0 -> 1:", st());
check("early jump: chord 1 from its first/second bar", ex.web_test_harm_hc(0) === 1 && ex.web_test_harm_hc(2) >= 1);
// 2c: stopped: at once
setup(); { const c = ex.web_test_harm_hc(0); ex.web_enc(4, 2); render(400); console.log("stopped:", st()); check("stopped: the jump is immediate", ex.web_test_harm_hc(0) === (c + 2) % 4); }
// 3: countdown longer / shorter
setup(); playTo(500); const l0 = ex.web_test_harm_hc(2);
ex.web_enc(5, 1); render(200); ex.web_enc(5, 1); render(200);
console.log("longer x2:", st()); check("LEFT +2", ex.web_test_harm_hc(2) === l0 + 2 && ex.web_test_harm_hc(3) === 2);
ex.web_enc(5, -1); render(200); check("LEFT -1 takes an extra bar back", ex.web_test_harm_hc(3) === 1 && ex.web_test_harm_hc(2) === l0 + 1);
for (let i = 0; i < 4; i++) { ex.web_enc(5, -1); render(150); } console.log("shorter:", st());
check("LEFT never below 1", ex.web_test_harm_hc(2) >= 1);
// 4: one extra bar: chord 0 (4 bars, 8 s) lasts 10 s
setup(); playTo(300); ex.web_enc(5, 1); render(150); render(8800); check("extra bar: still chord 0 after ~9 s", ex.web_test_harm_hc(0) === 0); render(2500); check("extra bar: chord 1 after ~11.5 s", ex.web_test_harm_hc(0) === 1);
setup(); playTo(300); for (let i = 0; i < 18; i++) { ex.web_enc(5, 1); render(60); } console.log("18x longer:", st());
check("past 16: HOLD (INF)", ex.web_test_harm_hc(4) === 1); render(14000); check("INF: still chord 0 after 14 s", ex.web_test_harm_hc(0) === 0);
ex.web_enc(5, -1); render(200); console.log("INF -1:", st()); check("INF -> 16 bars", ex.web_test_harm_hc(4) === 0 && ex.web_test_harm_hc(3) === 16);
// 5: the header bar's data: the chord's length as it stands, the elapsed bars keep counting through the repeats
setup(); playTo(300); ex.web_enc(5, 1); render(150); ex.web_enc(5, 1); render(150);
check("len = 4 + 2 after two bars more", ex.web_test_harm_hc(6) === 6);
render(7500); const el1 = ex.web_test_harm_hc(7); render(2000); const el2 = ex.web_test_harm_hc(7);
console.log("elapsed through the repeat:", el1, el2, st());
check("elapsed keeps counting into the repeated bars (no jump back)", el2 >= el1 && el2 >= 4 && ex.web_test_harm_hc(0) === 0);
check("len stays 6 while it repeats", ex.web_test_harm_hc(6) === 6);
console.log(fails ? fails + " FAILED" : "all ok");
