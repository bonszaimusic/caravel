// SPDX-License-Identifier: GPL-3.0-only
import fs from 'node:fs';
import assert from 'node:assert/strict';
const current=fs.readFileSync('build/engine-pair-morph/felucca.wasm');
const old=process.argv[2] ? fs.readFileSync(process.argv[2]) : null;
async function device(bytes=current,a=14,b=16){
  const {instance:{exports:e}}=await WebAssembly.instantiate(bytes,{});e._initialize();e.web_nor_erase();e.web_boot();
  function render(ms){const out=[];for(let left=Math.round(ms*44.1);left>0;){const n=Math.min(left,128);left-=n;e.web_render(n);out.push(...new Float32Array(e.memory.buffer,e.web_out_l(),n));}return out;}
  render(1200);assert.equal(e.web_pair_prepare(0,2,a,b),1);render(50);
  return {e,render};
}
const rms=a=>Math.sqrt(a.reduce((s,x)=>s+x*x,0)/a.length);
const peak=a=>a.reduce((s,x)=>Math.max(s,Math.abs(x)),0);
function highBand(audio){
  const N=2048;let energy=0;
  for(let offset=0;offset+N<=audio.length;offset+=N){
    const re=new Float64Array(N),im=new Float64Array(N);
    for(let i=0;i<N;i++)re[i]=audio[offset+i]*(.5-.5*Math.cos(2*Math.PI*i/(N-1)));
    for(let i=1,j=0;i<N;i++){let b=N>>1;for(;j&b;b>>=1)j^=b;j^=b;if(i<j)[re[i],re[j]]=[re[j],re[i]];}
    for(let len=2;len<=N;len<<=1)for(let at=0;at<N;at+=len)for(let k=0;k<len/2;k++){
      const a=-2*Math.PI*k/len,c=Math.cos(a),s=Math.sin(a),j=at+k,h=j+len/2;
      const tr=re[h]*c-im[h]*s,ti=re[h]*s+im[h]*c;re[h]=re[j]-tr;im[h]=im[j]-ti;re[j]+=tr;im[j]+=ti;
    }
    for(let i=Math.ceil(8000*N/44100);i<N/2;i++)energy+=(re[i]*re[i]+im[i]*im[i])/(N*N);
  }
  return energy;
}
async function unpair(bytes){
  const {e,render}=await device(bytes,0,0);
  for(const t of [0,2])for(const [id,v] of [[1,0],[2,0],[3,127],[4,127]])e.web_pair_param(t,id,v);
  e.web_pair_control(0,0,70);render(20);e.web_keys(1<<7);render(500);
  const before=render(40);e.web_pair_off();const after=render(40);
  return {before:rms(before),after:rms(after),lateAfter:rms(after.slice(662)),peakBefore:peak(before),peakAfter:peak(after)};
}
const stopped=await unpair(current);
assert.ok(stopped.after<stopped.before*.35 && stopped.lateAfter<stopped.before*.01,'unpair fades out instead of playing both engines at full gain');
assert.ok(stopped.peakAfter<=stopped.peakBefore*1.1,'unpair has no doubled tail peak');
console.log('new unpair:',stopped);
if(old)console.log('previous unpair:',await unpair(old));
const results=[];
for(const topo of [0,1,2,3,4,5])for(const motion of [0,1]){
  const {e,render}=await device();e.web_pair_param(2,109,topo);
  e.web_pair_param(2,108,116);e.web_pair_param(2,113,30);e.web_pair_param(0,111,120);
  e.web_pair_motion(motion);render(30);e.web_keys(1<<7);
  for(const t of [0,2]){e.web_pair_lfo(t,0,1,t?63:-64);e.web_pair_control(t,5,127);e.web_pair_lfo(t,1,11,t?-48:48);e.web_pair_lfo2_rate(t,119);}
  const audio=[];
  for(let i=0;i<100;i++){
    e.web_pair_morph(0,Math.round(i/99*32767));audio.push(...render(10));
    assert.equal(e.web_pair_get(12,0)+e.web_pair_get(12,2),32767,'unity-sum weights even during fast LFO sweeps');
  }
  assert.ok(audio.every(Number.isFinite)&&peak(audio)<1,'finite bounded sweep audio');
  e.web_keys(0);render(10);assert.equal(e.web_pair_get(7,0),0);assert.equal(e.web_pair_get(7,2),0);
  results.push({topo,motion,peak:peak(audio),rms:rms(audio),energyAbove8kHz:highBand(audio)});
}
for(let i=0;i<results.length;i+=2)assert.ok(results[i+1].energyAbove8kHz<=results[i].energyAbove8kHz*1.01+1e-10,'Motion does not add high-band energy in the high-feedback/resonance sweep fixtures');
console.log('fast LFO sweep metrics:',JSON.stringify(results));
// High-resonance SEM: the character macro must not open its resonant filter; DRONE coupling stays untouched.
for(const motion of [0,1]){
  const {e,render}=await device();e.web_pair_param(0,111,120);e.web_pair_param(2,108,116);
  e.web_pair_motion(motion);render(40);e.web_keys(1<<7);const audio=render(400);
  assert.equal(e.web_pair_param_get(2,108),116);assert.equal(e.web_pair_param_get(0,111),120);
  console.log(`high-resonance midpoint motion=${motion}:`,{peak:peak(audio),rms:rms(audio),energyAbove8kHz:highBand(audio)});
}
console.log('audio sweep: 12 topology/motion sweeps, rapid LFO1/LFO2, unity-sum gains, bounded samples and unpair transition passed');
