// SPDX-License-Identifier: GPL-3.0-only
import fs from 'node:fs';
import assert from 'node:assert/strict';
const wasm=fs.readFileSync('build/engine-pair-morph/felucca.wasm');
let checks=0;
const check=(v,message)=>{assert.ok(v,message);checks++;};
async function device(bytes=wasm,morph=16384,motion=0){
  const {instance}=await WebAssembly.instantiate(bytes,{}),ex=instance.exports;
  ex._initialize();ex.web_nor_erase();ex.web_boot();
  function render(ms,keep=false){
    const audio=[];
    for(let left=Math.round(ms*44.1);left>0;){
      const n=Math.min(left,128);left-=n;ex.web_render(n);
      if(keep)audio.push(...new Float32Array(ex.memory.buffer,ex.web_out_l(),n));
    }
    return audio;
  }
  render(1200);check(ex.web_pair_prepare(0,2,14,16)===1,'prepare SEM/DRONE');
  ex.web_pair_morph(0,morph);if(ex.web_pair_motion)ex.web_pair_motion(motion);
  render(80);ex.web_keys(1<<7);
  const audio=render(500,true);
  check(audio.every(Number.isFinite) && Math.max(...audio.map(Math.abs))<1,'finite bounded audio');
  return {ex,render,audio};
}
const off=await device(),on=await device(wasm,16384,1);
check(off.ex.web_pair_get(9,0)===0 && on.ex.web_pair_get(9,0)===1,'switch is optional, default off');
let difference=0;
for(let i=0;i<off.audio.length;i++)difference+=(on.audio[i]-off.audio[i])**2;
check(Math.sqrt(difference/off.audio.length)>0.0001,'bounded tone movement changes timbre without a centre gain boost');
check(on.ex.web_pair_param_get(0,110)===off.ex.web_pair_param_get(0,110),'SEM CUT base remains independent');
check(on.ex.web_pair_param_get(2,108)===off.ex.web_pair_param_get(2,108),'DRONE COUPLE base remains independent');
check(on.ex.web_pair_get(7,0)===1 && on.ex.web_pair_get(7,2)===1,'toggle keeps both notes sounding');
on.ex.web_pair_motion(0);on.render(30);
check(on.ex.web_pair_get(10,0)===0,'switch off settles fully');
check(on.ex.web_pair_get(7,0)===1 && on.ex.web_pair_get(7,2)===1,'switch off does not release held notes');
for(const morph of [0,32767]){
  const a=await device(wasm,morph,0),b=await device(wasm,morph,1);
  assert.deepEqual(a.audio,b.audio);checks++;
}
on.ex.web_pair_motion(1);on.render(20);
const n=on.ex.web_pair_snapshot_save();
const saved=new Uint8Array(on.ex.memory.buffer,on.ex.web_pair_snapshot_buf(),n).slice();
check(new TextDecoder().decode(saved.subarray(0,4))==='EPM2','motion snapshots use a versioned EPM2 envelope');
const restored=await device();
new Uint8Array(restored.ex.memory.buffer,restored.ex.web_pair_snapshot_buf(),n).set(saved);
check(restored.ex.web_pair_snapshot_load(n)===1 && restored.ex.web_pair_get(9,0)===1,'snapshot retains switch');
const legacy=fs.readFileSync('experiments/engine-pair-morph/fixtures/legacy-epm1.epm');
new Uint8Array(restored.ex.memory.buffer,restored.ex.web_pair_snapshot_buf(),legacy.length).set(legacy);
check(restored.ex.web_pair_snapshot_load(legacy.length)===1,'original pre-motion EPM1 fixture loads');
check(restored.ex.web_pair_get(9,0)===0 && restored.ex.web_pair_get(1,0)===21000,'legacy snapshot defaults to original morph');
if(process.argv[2]){
  const baseline=await device(fs.readFileSync(process.argv[2]));
  assert.deepEqual(off.audio,baseline.audio);checks++;
  console.log('motion OFF: audio bit-identical to previous prototype');
}
console.log(`motion WASM: ${checks} checks passed; optional curve, audible tone movement, endpoints, held notes and EPM1/EPM2 snapshots`);
