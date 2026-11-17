// SPDX-License-Identifier: GPL-3.0-only
// Random gestures against the UX layers build: no WASM trap, the screen stays alive. node web/emu/ux_fuzz.mjs build/emu/felucca.wasm [steps] [seed]
import fs from "fs";
const wasmPath = process.argv[2] || "build/emu/felucca.wasm", steps = +(process.argv[3] || 3000);
let seed = +(process.argv[4] || 1);
const rnd = (n) => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed % n; };
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports, mem = ex.memory;
ex._initialize(); ex.web_nor_erase(); ex.web_boot(); if (ex.web_test_build) ex.web_test_build();
const render = (ms) => { const n = Math.round(ms * 44.1); for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
render(2500);
let held = 0, keys = 0;
for (let i = 0; i < steps; i++) {
  const r = rnd(10);
  if (r < 3) { const b = rnd(14); ex.web_buttons(held | (1 << b)); render(30 + rnd(120)); ex.web_buttons(held); render(30 + rnd(80)); }
  else if (r < 4) { held = 1 << rnd(14); ex.web_buttons(held); render(300 + rnd(900)); held = 0; ex.web_buttons(0); render(60); }
  else if (r < 6) { ex.web_enc(rnd(7), rnd(2) ? 1 : -1); render(30); }
  else if (r < 8) { keys = rnd(2) ? keys | (1 << rnd(27)) : keys & ~(1 << rnd(27)); ex.web_keys(keys); render(40 + rnd(100)); }
  else if (r < 9) { ex.web_buttons(rnd(2) ? (1 << 6) : 0); render(800 + rnd(300)); ex.web_buttons(0); render(100); }
  else render(100 + rnd(400));
}
ex.web_buttons(0); ex.web_keys(0); render(1000);
const fb = new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240);
console.log(`fuzz ${steps} steps seed ${process.argv[4] || 1}: distinct colours on the screen: ${new Set(fb).size}`);
process.exit(new Set(fb).size > 2 ? 0 : 1);
