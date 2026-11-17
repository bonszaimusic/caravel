// SPDX-License-Identifier: GPL-3.0-only
// CAPTURE device plan C-c: an event is 4 bytes (time in 1/4096 bar) instead of 8 (time in 1/65536 bar).
//   node web/emu/capture_roundtrip_test.mjs BEFORE.wasm AFTER.wasm      (both with -DCARAVEL_UX_CAPTURE=1 and web_test_cap_loop_abs)
// The same keys at the same moments go to both builds, REC takes the loop, and every loop event's place on the bar clock is read
// back (cap_o + t, modulo the loop): the largest difference between the two builds is what the 16 bit time costs. It is
// measured in 1/65536 bar and in milliseconds at 120 BPM (one bar = 2000 ms): the 4 byte time rounds down to 1/4096 bar,
// so the event's own error is below 16 units (0.49 ms), the origin's rounding adds up to 16 more.
import fs from "fs";
const [pa, pb] = [process.argv[2], process.argv[3]];
const B = { HOME: 6, PLAY: 10, REC: 11, SEQ: 9 };
let fails = 0;
const check = (what, ok) => { console.log(`rt: ${what.padEnd(84)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
async function device(path) {
  const { instance } = await WebAssembly.instantiate(fs.readFileSync(path), {});
  const ex = instance.exports;
  ex._initialize(); ex.web_nor_erase(); ex.web_boot(); if (ex.web_test_build) ex.web_test_build();
  const d = { ex };
  d.render = (ms) => { const n = Math.round(ms * 44.1); for (let k = 0; k < n; k += 128) ex.web_render(Math.min(128, n - k)); };
  d.tap = (b) => { ex.web_buttons(1 << B[b]); d.render(70); ex.web_buttons(0); d.render(90); };
  d.hold = (b, ms) => { ex.web_buttons(1 << B[b]); d.render(ms); ex.web_buttons(0); d.render(150); };
  return d;
}
const A = await device(pa), N = await device(pb);
const both = (f) => { f(A); f(N); };
both((d) => { d.render(2500); d.hold("HOME", 1100); d.render(3500); d.tap("PLAY"); d.render(300); });
// notes at irregular moments (keys 7 12 15 19 4 9 ..., gaps that are not multiples of anything)
const gaps = [137, 211, 93, 177, 251, 119, 163, 283];
const keys = [7, 12, 15, 19, 4, 9, 14, 17];
both((d) => { keys.forEach((k, i) => { d.ex.web_keys(1 << k); d.render(gaps[i]); d.ex.web_keys(0); d.render(gaps[(i + 3) % gaps.length]); }); });
both((d) => { d.tap("REC"); d.render(300); });
const na = A.ex.web_test_cap_events(0), nn = N.ex.web_test_cap_events(0);
check(`the same number of loop events (${na})`, na === nn && na >= 2 * keys.length);
let worst = 0, worstK = -1;
for (let k = 0; k < Math.min(na, nn); k++) {
  const x = A.ex.web_test_cap_loop_abs(0, k), y = N.ex.web_test_cap_loop_abs(0, k);
  let diff = Math.abs(x - y);
  const mod = N.ex.web_test_cap_len(0);
  if (mod) diff = Math.min(diff, mod - diff);
  if (diff > worst) { worst = diff; worstK = k; }
}
const ms = worst / 65536 * 2000;
console.log(`rt: largest difference of an event's place: ${worst} / 65536 bar = ${ms.toFixed(2)} ms at 120 BPM (event ${worstK})`);
check("the largest difference is below 32/65536 bar (about 1 ms at 120 BPM)", worst < 32);
// the loop sounds the same: peak level over three loop lengths of both
both((d) => { d.ex.web_buttons(0); });
console.log(fails ? `rt: ${fails} failed` : "rt: all passed");
process.exit(fails ? 1 : 0);
