// SPDX-License-Identifier: GPL-3.0-only
// Actual WASM audio across every ordered combination, including each engine as owner.
import fs from 'node:fs';
import assert from 'node:assert/strict';
const {instance:{exports:ex}}=await WebAssembly.instantiate(fs.readFileSync('build/engine-pair-morph/felucca.wasm'),{});
ex._initialize();ex.web_nor_erase();ex.web_boot();
function render(ms){
  let peak=0;
  for(let left=Math.round(ms*44.1);left>0;){
    const n=Math.min(128,left);left-=n;ex.web_render(n);
    for(const ptr of [ex.web_out_l(),ex.web_out_r()])for(const v of new Float32Array(ex.memory.buffer,ptr,n)){
      assert.ok(Number.isFinite(v));peak=Math.max(peak,Math.abs(v));
    }
  }
  return peak;
}
render(1200);
const engines=[0,2,3,4,5,6,7,8,9,11,12,14,15,16];
let combinations=0;
for(const a of engines)for(const b of engines){
  assert.equal(ex.web_pair_prepare(0,2,a,b),1,`prepare ${a}/${b}`);render(50);
  ex.web_pair_select(2);ex.web_keys(1<<7);
  assert.ok(render(150)>0.0001,`audible ${a}/${b}`);
  ex.web_pair_morph(2,0);render(20);
  assert.equal(ex.web_pair_get(12,0),32767);assert.equal(ex.web_pair_get(12,2),0);
  ex.web_pair_morph(0,32767);render(20);
  assert.equal(ex.web_pair_get(12,0),0);assert.equal(ex.web_pair_get(12,2),32767);
  assert.equal(ex.web_pair_get(13,0),2);assert.equal(ex.web_pair_get(13,2),0);assert.equal(ex.web_pair_get(13,1),-1);
  ex.web_keys(0);render(10);
  assert.equal(ex.web_pair_get(7,0),0);assert.equal(ex.web_pair_get(7,2),0);
  combinations++;
}
assert.equal(ex.web_pair_prepare(0,2,10,14),0);
assert.equal(ex.web_pair_prepare(0,2,13,14),0);
assert.equal(ex.web_pair_get(0,0),1,'refused prepare preserves existing pair');
console.log(`flex WASM: ${combinations} ordered combinations audible/finite; follower keyboard, both endpoints, live gain/partner state and note-off passed`);
