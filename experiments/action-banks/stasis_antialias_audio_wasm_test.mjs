// SPDX-License-Identifier: GPL-3.0-only
// Real opt-in C/WASM DSP regression: detect high-band aliases, smoothness of
// grain overlap, and broadband sample jumps through the actual audio renderer.
import fs from 'node:fs';
import assert from 'node:assert/strict';
const {instance}=await WebAssembly.instantiate(fs.readFileSync('build/home-perf/felucca.wasm'),{});
const e=instance.exports;
e._initialize();e.web_nor_erase();e.web_boot();
function filteredRMS(freq,quality){
 e.web_stasis_probe_reset();
 const count=24576,delay=1024,amp=22000;
 let squared=0,n=0;
 for(let i=0;i<count;i++){
  const x=Math.round(amp*Math.sin(2*Math.PI*freq*i/44100));
  const value=e.web_stasis_probe_capture(x,quality);
  if(value!==65536 && i>=delay){squared+=value*value;n++;}
 }
 return Math.sqrt(squared/n);
}
const midHQ=filteredRMS(500,0);
const highHQ=filteredRMS(9000,0);
assert.ok(midHQ>14000 && highHQ>14000,'HQ remains unfiltered at 500Hz and 9kHz');
for(const [quality,highFreq] of [[1,15000],[2,9000]]){
 const low=filteredRMS(500,quality);
 const high=filteredRMS(highFreq,quality);
 assert.ok(low>9000,'LONG/DRONE preserves useful low band: q='+quality+' '+low);
 assert.ok(high/low<0.12,
   'out-of-band alias energy below -18.4dB for q='+quality+': ratio '+high/low);
}
// All three modes preserve equal-power complementary overlap at both
// boundaries, with zero derivative: no corners every 93/186/372 ms.
for(let grain=0;grain<3;grain++){
 const h=2048<<grain;
 const w=i=>e.web_stasis_grain_envelope(i,grain);
 assert.equal(w(0),0);
 assert.equal(w(h),32768);
 assert.ok(Math.abs(w(h/2)-16384)<=3);
 assert.ok(w(1)<=2 && 32768-w(h-1)<=2,
   'zero slope at both grain join points: mode '+grain);
 for(let i=32;i<h;i+=47)
  assert.ok(w(i)>=w(i-1) && w(i)<=w(i+1),'monotone smooth window');
}
// Playback with the SAME C/WASM FM-1 renderer, not a JS audio simulation.
const render=(ms,record=false)=>{
 let n=Math.ceil(ms*44.1),maxJump=0,absPeak=0,prev=null,observed=0;
 while(n){
  let k=Math.min(512,n);e.web_render(k);
  if(record){
   const l=new Float32Array(e.memory.buffer,e.web_out_l(),k);
   const r=new Float32Array(e.memory.buffer,e.web_out_r(),k);
   for(let i=0;i<k;i++){
    const x=l[i],y=r[i];assert.ok(Number.isFinite(x)&&Number.isFinite(y));
    absPeak=Math.max(absPeak,Math.abs(x),Math.abs(y));
    if(prev!==null)maxJump=Math.max(maxJump,Math.abs(x-prev));
    prev=x;observed++;
   }
  }
  n-=k;
 }
 return {maxJump,absPeak,observed};
};
render(1350);
e.web_stasis_test_type(3);
render(120);
e.web_test_drone(0);
assert.equal(e.web_pair_control(0,8,127),1);
e.web_keys(1<<7);
render(3500);
e.web_stasis_test_hold(1);
render(180);
let metrics=render(1700,true);
assert.ok(metrics.absPeak>0.02,'recorded HOLD produces audible texture');
assert.ok(metrics.maxJump<0.07,'default HOLD has no hard PCM jump: '+JSON.stringify(metrics));
// Switch grain sizes/scan/reverse while held: transition must be click-safe.
e.web_stasis_grain_open();
e.web_enc(3,1);e.web_enc(4,80);e.web_enc(6,1);
metrics=render(1700,true);
assert.ok(metrics.maxJump<0.07,'372ms reverse/scan grain must remain continuous: '+JSON.stringify(metrics));
e.web_enc(3,-2);e.web_enc(4,-80);e.web_enc(6,-1);
metrics=render(1700,true);
assert.ok(metrics.maxJump<0.07,'93ms forward grain transition must remain continuous: '+JSON.stringify(metrics));
console.log('STASIS ANTI-CRACKLE C/WASM PASS: 3 grains zero-slope endpoints, HQ fidelity, anti-aliased LONG/DRONE, live PCM HOLD, reverse/scan/size transitions.');
