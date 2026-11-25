// SPDX-License-Identifier: GPL-3.0-only
// Private stage-2 WASM hardware-path test, using actual web_buttons/web_keys
// (not a substitute for FM-1 key-scanner/ISR/audio verification).
import fs from 'node:fs';
import assert from 'node:assert/strict';

const filename = process.argv[2] || 'build/emu/felucca.wasm';
const {instance} = await WebAssembly.instantiate(fs.readFileSync(filename), {});
const e = instance.exports;
e._initialize(); e.web_nor_erase(); e.web_boot();
const render = ms => {
  for (let n = Math.ceil(ms * 44.1); n > 0;) {
    const x = Math.min(128, n);
    e.web_render(x);
    n -= x;
  }
};
const note = (index, ms = 95) => {
  e.web_keys(1 << index); render(ms);
  e.web_keys(0); render(130);
};
render(1350);
assert.equal(e.web_stasis_ring_capacity(), 65536);
assert.equal(e.web_stasis2_space_groups(),5,'SPACE includes TEXTURE/REGION/TANK');
const st = (id) => (0xF4 << 16) | id;
assert.equal(e.web_stasis2_space_target(2,0),st(0),'TEXTURE: GRAIN');
assert.equal(e.web_stasis2_space_target(3,0),st(1),'REGION: POS');
assert.equal(e.web_stasis2_space_target(3,1),st(2),'REGION: LENGTH');
assert.equal(e.web_stasis2_space_target(3,2),st(3),'REGION: REVERSE');
assert.equal(e.web_stasis2_space_target(3,3),st(4),'REGION: SOURCE STATUS');
assert.equal(e.web_stasis2_space_target(2,3)>>>16,0xF1,'TEXTURE K4: TONE');
const trackKind = e.web_stasis2_space_target(0,0) >>> 16;   /* SENDS are track parameters */
assert.equal(e.web_stasis2_space_target(4,1),st(5),'TANK K2: FEED (grain to hall)');
assert.equal(e.web_stasis2_space_target(4,2),(trackKind << 16) | 80,'TANK: per-track DRY (retired id 80)');
assert.equal(e.web_stasis2_space_target(4,3),st(6),'TANK: HALL return level');
e.web_stasis_test_type(3); render(100);
/* The "no effect" id (PF_N) follows the effect table; ask the build instead of hard-coding it. */
const PF_NONE = e.web_stasis2_fx_key_map(255);
assert.equal(e.web_stasis2_fx_key_map(10), PF_NONE, 'B4 reserved from legacy FX');
assert.equal(e.web_stasis2_fx_key_map(11), PF_NONE, 'C5 must not invoke PHSR');
assert.equal(e.web_stasis2_fx_key_map(12), PF_NONE, 'D5 must not invoke VERB');
assert.equal(e.web_stasis2_fx_key_map(14), PF_NONE, 'F5 must not restore old effect');
assert.equal(e.web_stasis2_fx_key_map(15), PF_NONE, 'G5 must not restore old effect');
for (const p of [10, 11, 12])
  assert.equal(e.web_stasis2_fx_cell_state(p), 1, 'distinct idle LCD theme state');

/* Physical FX button held for longer than the HOLD threshold. */
e.web_buttons(1 << 0);
render(510);
assert.ok(e.web_test_page() & (1 << 9), 'FX overlay is actually open');
note(18);
assert.equal(e.web_action_stasis_capt(), 1, 'B4 toggles FREEZE ON');
assert.equal(e.web_stasis2_fx_cell_state(10), 2, 'B4 FREEZE active LCD state');
note(18);
assert.equal(e.web_action_stasis_capt(), 0, 'B4 toggles FREEZE OFF');

/* A short C5 tap must never clear recorded sound. */
note(19, 220);
assert.equal(e.web_stasis_granular_get(18), 0, 'short C5 is not NEW SOURCE');

/* The continuous C5 press fires exactly once after 800 ms. */
e.web_keys(1 << 19);
render(650);
assert.equal(e.web_stasis_granular_get(18), 0, 'NEW not yet fired');
render(220);
assert.equal(e.web_stasis2_fx_cell_state(11), 2, 'C5 highlighted while held');
assert.equal(e.web_stasis_granular_get(18), 1, 'audio thread saw NEW SOURCE');
render(300); // still held; duration may complete the asynchronous audio fade
assert.equal(e.web_stasis2_fx_cell_state(11), 2, 'held key remains highlighted');
e.web_keys(0); render(100);
note(21);
assert.equal(e.web_action_stasis_capt(), 0, 'D5 RELEASE clears CAPT');
e.web_buttons(0); render(200);
e.web_stasis_test_type(2); render(110);
assert.equal(e.web_stasis2_fx_cell_state(11), 0, 'no inactive STASIS LCD legend');
assert.notEqual(e.web_stasis2_fx_key_map(11), PF_NONE, 'old customized non-STASIS FX restored');
console.log('STASIS2 WASM private stage-2 FX B4/C5/D5 and PHSR-map checks PASS; no device claim');
