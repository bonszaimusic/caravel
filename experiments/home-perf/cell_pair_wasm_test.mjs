// SPDX-License-Identifier: GPL-3.0-only
// Full firmware WASM, actual panel chord, audible endpoints and stored projects.
import fs from 'node:fs';
import assert from 'node:assert/strict';
const wasm=fs.readFileSync('build/home-perf/felucca.wasm');
let cases=0;
for(let topology=3;topology<=5;topology++)for(let reverse=0;reverse<2;reverse++){
 const {instance:{exports:e}}=await WebAssembly.instantiate(wasm,{});
 e._initialize();e.web_nor_erase();e.web_boot();
 const render=ms=>{
  let peak=0;
  for(let left=Math.round(ms*44.1);left>0;){
   const n=Math.min(left,128);left-=n;e.web_render(n);
   for(const ptr of [e.web_out_l(),e.web_out_r()])for(const v of new Float32Array(e.memory.buffer,ptr,n)){
    assert.ok(Number.isFinite(v));peak=Math.max(peak,Math.abs(v));
   }
  }
  return peak;
 };
 render(1400);assert.equal(e.web_pair_prepare(0,2,reverse?16:14,reverse?14:16),1);
 const cell=reverse?0:2,poly=reverse?2:0;
 assert.equal(e.web_pair_param(cell,109,topology),1); // P_E3, unchanged native schema.
 render(15);assert.equal(e.web_pair_get(14,0),reverse?1:2);assert.equal(e.web_pair_get(3,0),4);
 e.web_pair_select(2);e.web_keys((1<<0)|(1<<4)|(1<<7));render(40);
 assert.equal(e.web_pair_get(7,poly),3);assert.equal(e.web_pair_get(7,cell),1);
 for(const morph of [0,32767]){e.web_pair_morph(2,morph);render(40);assert.ok(render(100)>0.00001);}
 e.web_keys(0);render(40);assert.equal(e.web_pair_get(7,poly),0);assert.equal(e.web_pair_get(7,cell),0);
 e.web_pair_control(0,3,2);render(15); // LATCH STACK is shared, even when A is DRONE.
 e.web_keys((1<<0)|(1<<4)|(1<<7));render(40);e.web_keys(0);render(40);
 assert.equal(e.web_pair_get(7,poly),3);assert.equal(e.web_pair_get(7,cell),1);
 e.web_pair_select(0);e.web_keys((1<<0)|(1<<4)|(1<<7));render(40);e.web_keys(0);render(40);
 assert.equal(e.web_pair_get(7,poly),0);assert.equal(e.web_pair_get(7,cell),0);
 e.web_pair_control(0,3,0);render(15);
 assert.equal(e.web_home_save(0),1);e.web_pair_off();assert.equal(e.web_home_load(0),1);
 assert.equal(e.web_pair_get(14,0),reverse?1:2);assert.equal(e.web_pair_get(3,0),4);
 const n=e.web_home_export(0),saved=new Uint8Array(e.memory.buffer,e.web_home_project_buf(),n).slice();
 assert.equal(n,10000);e.web_pair_off();new Uint8Array(e.memory.buffer,e.web_home_project_buf(),n).set(saved);
 assert.equal(e.web_home_import(n),1);assert.equal(e.web_pair_get(14,0),reverse?1:2);
 assert.equal(e.web_pair_get(7,poly),0);assert.equal(e.web_pair_get(7,cell),0);
 cases++;
}
console.log(`CELL PAIR WASM: ${cases} topology/role cases passed; panel chord, LATCH STACK, audible morph endpoints, A-D and HPM1.`);
