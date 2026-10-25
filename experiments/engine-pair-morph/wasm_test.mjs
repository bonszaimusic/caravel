// SPDX-License-Identifier: GPL-3.0-only
import fs from 'node:fs';
import assert from 'node:assert/strict';
const bytes=fs.readFileSync(process.argv[2] || 'build/engine-pair-morph/felucca.wasm');
let checks=0;
const check=(value,message)=>{assert.ok(value,message);checks++;};
async function device(){
  const {instance}=await WebAssembly.instantiate(bytes,{}),ex=instance.exports;
  ex._initialize();ex.web_nor_erase();ex.web_boot();
  const render=(ms)=>{
    let out=[];
    for(let left=Math.round(ms*44.1);left>0;){
      const n=Math.min(128,left);left-=n;ex.web_render(n);
      const l=new Float32Array(ex.memory.buffer,ex.web_out_l(),n),r=new Float32Array(ex.memory.buffer,ex.web_out_r(),n);
      for(let i=0;i<n;i++)out.push(l[i],r[i]);
    }
    return out;
  };
  render(1200);check(ex.web_pair_prepare(0,2,14,16),'SEM/DRONE pair prepares in full emulator');render(80);
  return {ex,render};
}
async function sound(morph){
  const d=await device();d.ex.web_pair_morph(2,morph);d.render(30);
  d.ex.web_keys(1<<7);const samples=d.render(300);
  check(d.ex.web_pair_get(7,0)===1 && d.ex.web_pair_get(7,2)===1,'both engines sound at every endpoint');
  check(samples.every(Number.isFinite),'audio is finite');
  check(samples.some(v=>Math.abs(v)>.001),'audible audio');
  return {samples,...d};
}
const a=await sound(0),b=await sound(32767),mid=await sound(16384);
let max=0,err=0;
for(let i=0;i<a.samples.length;i++){
  const reference=(a.samples[i]*(32767-16384)+b.samples[i]*16384)/32767;
  max=Math.max(max,Math.abs(mid.samples[i]-reference));err+=(mid.samples[i]-reference)**2;
}
check(max<0.002,`midpoint follows endpoint audio mix (max ${max})`);
check(Math.sqrt(err/a.samples.length)<0.0002,'midpoint RMS rounding error is bounded');
const {ex,render}=mid;
ex.web_pair_select(2);ex.web_pair_morph(2,0);render(20);
check(ex.web_pair_get(2,0)===0 && ex.web_pair_get(7,0)===1 && ex.web_pair_get(7,2)===1,'held-note morph from follower');
ex.web_keys(0);ex.web_pair_panic();render(1500);
ex.web_pair_control(2,3,2);render(10);ex.web_keys(1<<7);render(40);ex.web_keys(0);render(40);
check(ex.web_pair_get(7,0)===1 && ex.web_pair_get(7,2)===1,'browser ABI latch is shared');
ex.web_pair_select(0);ex.web_keys(1<<7);render(40);ex.web_keys(0);render(100);
check(ex.web_pair_get(7,0)===0 && ex.web_pair_get(7,2)===0,'latch toggle from other member');
ex.web_midi(0xB0,119,127);render(30);check(ex.web_pair_get(1,0)===32767,'CC119 controls shared base');
ex.web_pair_lfo(0,0,1,24);ex.web_pair_lfo(2,1,11,-20);ex.web_pair_lfo2_rate(2,73);
ex.web_pair_lane(12,12345);ex.web_pair_morph(0,21000);
const size=ex.web_pair_snapshot_save();check(size===4032,'EPM1 envelope has explicit size');
const saved=new Uint8Array(ex.memory.buffer,ex.web_pair_snapshot_buf(),size).slice();
const fresh=await device();new Uint8Array(fresh.ex.memory.buffer,fresh.ex.web_pair_snapshot_buf(),size).set(saved);
check(fresh.ex.web_pair_snapshot_load(size)===1,'snapshot restores in a new WASM instance');
fresh.render(50);check(fresh.ex.web_pair_get(1,0)===21000 && fresh.ex.web_pair_depth_get(2,1)===-20,'base and independent MOD slot persist');
check(fresh.ex.web_pair_lfo2_rate_get(2)===73,'independent LFO2 config persists');
check(fresh.ex.web_pair_get(7,0)===0 && fresh.ex.web_pair_get(7,2)===0,'snapshot starts without held notes');
saved[40]^=1;new Uint8Array(fresh.ex.memory.buffer,fresh.ex.web_pair_snapshot_buf(),size).set(saved);
check(fresh.ex.web_pair_snapshot_load(size)===0 && fresh.ex.web_pair_get(1,0)===21000,'corrupt snapshot refused without changing pair');
console.log(`pair WASM: ${checks} checks passed; midpoint max error=${max}, RMS=${Math.sqrt(err/a.samples.length)}`);
