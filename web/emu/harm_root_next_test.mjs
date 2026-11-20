// SPDX-License-Identifier: GPL-3.0-only
// PLAY HARM area: H ROOT turned waits for the next chord (at once while stopped).
//   node web/emu/harm_root_next_test.mjs build/emu/felucca.wasm
import fs from "fs";
const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm"), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot();
const render = (ms) => { const n = Math.round(ms * 44.1); for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
const tap = (b) => { ex.web_buttons(1 << b); render(70); ex.web_buttons(0); render(90); };
render(2500);
let fails = 0; const check = (w, ok) => { console.log((ok ? "ok   " : "FAIL ") + w); if (!ok) fails++; };
const root = () => ex.web_test_harm_key(0), next = () => ex.web_test_harm_key(4), chord = () => ex.web_test_harm_key(3);
function setup() { if (ex.web_playing()) { tap(10); render(400); } ex.web_test_harm_reset(); ex.web_test_harm_on(1); [[0,1],[9,1],[5,1],[7,1]].forEach(([r,d],i)=>ex.web_test_harm_prog(i,r,d)); ex.web_test_harm_role(0,1); tap(6); tap(1); render(300); }
// stopped: at once
setup(); const r0 = root(); ex.web_enc(3, 2); render(300);
check("stopped: the root changes at once", root() === (r0 + 2) % 12 && next() === 0);
// playing: waits for the next chord (chords of 2 bars = 4 s)
setup(); tap(10); render(600); const c0 = chord(), r1 = root(); ex.web_enc(3, 3); render(300);
check("playing: the root has not changed yet, a new one waits", root() === r1 && next() === ((r1 + 3) % 12) + 1);
let guard = 0; while (chord() === c0 && guard++ < 80) render(100);
render(200); check("at the next chord the new root is in", chord() !== c0 && root() === (r1 + 3) % 12 && next() === 0);
// back on the sounding root cancels
setup(); tap(10); render(600); const r2 = root(); ex.web_enc(3, 1); render(200); ex.web_enc(3, -1); render(200);
check("turning back to the sounding root cancels the wait", next() === 0 && root() === r2);
console.log(fails ? fails + " FAILED" : "all ok");
