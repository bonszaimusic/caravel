// SPDX-License-Identifier: GPL-3.0-only
import fs from 'node:fs';
import assert from 'node:assert/strict';
const wasm=fs.readFileSync('build/home-perf/felucca.wasm');
const store=await import('data:text/javascript;base64,'+Buffer.from(fs.readFileSync('build/home-perf/worklet.js')).toString('base64'));
async function device(sectors){const {instance}=await WebAssembly.instantiate(wasm,{}),e=instance.exports;e._initialize();e.web_nor_erase();if(sectors)store.restoreSectors(new Uint8Array(e.memory.buffer,e.web_nor(),e.web_nor_size()),sectors);e.web_boot();const d={e,render(ms){for(let i=0;i<ms*44.1;i+=128)e.web_render(Math.min(128,Math.ceil(ms*44.1-i)));},tap(b){e.web_buttons(1<<b);d.render(65);e.web_buttons(0);d.render(130);},turn(k,n){e.web_enc(k,n);d.render(65);}};d.render(1100);return d;}
const d=await device(),e=d.e;
assert.equal(e.web_home_get(0,0,0,0),1);e.web_home_apply(0,0);d.tap(2);d.turn(3,2);e.web_buttons(1<<5);d.render(70);e.web_buttons((1<<5)|(1<<6));d.render(70);e.web_buttons(0);d.render(200);assert.equal(e.web_home_get(3,0,0,0),1);assert.equal(e.web_home_get(4,0,0,0),1);
e.web_home_open(0);for(let row=1;row<32;row++)assert.equal(e.web_home_assign(0,row,0,1,0,0),1);d.turn(0,31);assert.equal(e.web_home_get(1,0,0,0),31);d.turn(1,1);assert.equal(e.web_pair_get(4,0),1);d.turn(1,-1);assert.equal(e.web_home_get(1,0,0,0),31);
e.web_home_apply(0,2);assert.equal(e.web_home_save(0),1);e.web_home_apply(0,0);assert.equal(e.web_home_save(1),1);assert.equal(e.web_home_load(0),1);assert.equal(e.web_home_get(0,0,0,0),2);
// Hardware-sized NOR roundtrip in a genuinely fresh WASM instance.
e.web_home_default(2);d.render(12000);const sectors=store.storeSectors(new Uint8Array(e.memory.buffer,e.web_nor(),e.web_nor_size()));assert.ok(Object.keys(sectors).some(k=>+k>=0xE8000 && +k<0xFC000));const reboot=await device(sectors);assert.equal(reboot.e.web_home_get(2,0,0,0),2);assert.equal(reboot.e.web_home_load(0),1);assert.equal(reboot.e.web_home_get(0,0,0,0),2);assert.equal(reboot.e.web_home_load(1),1);assert.equal(reboot.e.web_home_get(3,0,0,0),0);
// Export/import validates CRC before mutating the current performance.
const n=e.web_home_export(0),ptr=e.web_home_project_buf();assert.equal(n,e.web_home_project_size());const valid=new Uint8Array(e.memory.buffer,ptr,n).slice();new Uint8Array(e.memory.buffer,ptr,n)[100]^=1;assert.equal(e.web_home_import(n),0);new Uint8Array(e.memory.buffer,ptr,n).set(valid);assert.equal(e.web_home_import(n),1);
e.web_keys(1<<7);d.render(250);let peak=0;for(let k=0;k<128;k++)peak=Math.max(peak,Math.abs(new Float32Array(e.memory.buffer,e.web_out_l(),128)[k]));assert.ok(peak>0.00001);e.web_keys(0);d.render(500);
console.log(`HOME WASM: actual GLO+HOME, rows, track focus, A-D, one-MiB NOR reboot, default, CRC/export and audible firmware passed; capsule ${n} bytes.`);

// A capsule written by the previously delivered V1 emulator stays compatible.
const legacy=await device(),le=legacy.e,fixture=fs.readFileSync('experiments/home-perf/fixtures/home-v1.hpm');
assert.equal(fixture.length,le.web_home_project_size());new Uint8Array(le.memory.buffer,le.web_home_project_buf(),fixture.length).set(fixture);assert.equal(le.web_home_import(fixture.length),1);assert.equal(le.web_home_get(0,0,0,0),3);assert.equal(le.web_home_get(3,0,0,0),6);assert.equal(le.web_home_macro_get(0,0),64);assert.equal(le.web_home_get(6,0,2,0),2);
console.log('V1 -> current: unchanged HPM1 project schema, three HOME rows, foreign track cell and macro restored.');
