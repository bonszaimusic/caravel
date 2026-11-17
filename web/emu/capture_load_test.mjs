// SPDX-License-Identifier: GPL-3.0-only
// CAPTURE device plan C-e, the host half: how long a block takes while four tracks each play a near-full 32-bar loop (128 events),
// against the same synth with no loop, and the block that runs a REC (the take sorts and copies up to 128 events).
//   node web/emu/capture_load_test.mjs [felucca.wasm]       (built with -DCARAVEL_UX_CAPTURE=1)
// This is the browser build on the host (WebAssembly), not the JieLi CPU: the numbers say how the cost moves between a
// quiet block, a block with loops running and a take, not how many milliseconds it is on the device. The device half is the
// instruction count of the same code in the target disassembly (tools/capture_cost.py), and then a listening test.
import fs from "fs";
const wasmPath = process.argv[2] || "build/emu/felucca.wasm";
const B = { HOME: 6, PLAY: 10 };
let fails = 0;
const check = (what, ok) => { console.log(`load: ${what.padEnd(88)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports;
ex._initialize(); ex.web_nor_erase(); ex.web_boot(); if (ex.web_test_build) ex.web_test_build();
const BLOCK = 128;
const render = (ms) => { const n = Math.round(ms * 44.1); for (let k = 0; k < n; k += BLOCK) ex.web_render(Math.min(BLOCK, n - k)); };
const timed = (blocks) => { const t = []; for (let k = 0; k < blocks; k++) { const a = process.hrtime.bigint(); ex.web_render(BLOCK); t.push(Number(process.hrtime.bigint() - a) / 1e3); } return t; };
const stat = (t) => { const s = [...t].sort((a, b) => a - b); return { med: s[s.length >> 1], p99: s[Math.floor(s.length * 0.99)], max: s[s.length - 1] }; };
const tap = (b) => { ex.web_buttons(1 << B[b]); render(70); ex.web_buttons(0); render(90); };
render(2500); ex.web_buttons(1 << B.HOME); render(1100); ex.web_buttons(0); render(3500); tap("PLAY"); render(300);
const OP = { REC: 1 };
const quiet = stat(timed(2000));
// four tracks: 60 short notes each (the ring holds 128 events), then a take of 32 bars
for (let tr = 0; tr < 4; tr++) {
  ex.web_test_select(tr);
  for (let i = 0; i < 60; i++) { ex.web_keys(1 << (3 + ((i * 5 + tr * 3) % 20))); render(110); ex.web_keys(0); render(90); }
  ex.web_test_cap_post(OP.REC, tr, 32);
  render(60);
}
const evs = [0, 1, 2, 3].map((t) => ex.web_test_cap_events(t));
check(`four loops running with ${evs.join(", ")} events (full: 128)`, evs.every((n) => n >= 112) && [0, 1, 2, 3].every((t) => ex.web_test_cap_state(t) === 1));
const run = stat(timed(6000));
// REC again (an overdub with new notes) on each track: the block that runs the take
const takes = [];
for (let tr = 0; tr < 4; tr++) {
  ex.web_test_select(tr);
  ex.web_keys(1 << 9); render(110); ex.web_keys(0); render(90);
  ex.web_test_cap_post(OP.REC, tr, 32);
  takes.push(...timed(1).map((x) => x));
  render(200);
}
const take = Math.max(...takes);
const f = (x) => x.toFixed(1);
console.log(`load: block of ${BLOCK} frames, host WebAssembly, in microseconds`);
console.log(`load:   no loop      median ${f(quiet.med)}  p99 ${f(quiet.p99)}  max ${f(quiet.max)}`);
console.log(`load:   four loops   median ${f(run.med)}  p99 ${f(run.p99)}  max ${f(run.max)}`);
console.log(`load:   REC block    max ${f(take)} (four takes)`);
console.log(`load:   ratios       loops median ${(run.med / quiet.med).toFixed(2)}x of quiet, REC block ${(take / quiet.med).toFixed(1)}x of a quiet block`);
check("four running loops do not move the median block time by more than a factor of 3 (they sound, the synth works)", run.med < quiet.med * 3 + 50);
console.log("load:   (p99 and max of every row contain the frame the UI draws inside the same web_render call: they say nothing about CAPTURE)");
check("the REC block (take, sort, copy) stays below 20 quiet blocks", take < quiet.med * 20 + 100);
console.log(fails ? `load: ${fails} failed` : "load: all passed");
process.exit(fails ? 1 : 0);
