// SPDX-License-Identifier: GPL-3.0-only
// Exercise the real worklet message/render protocol in Node. This is not a browser UI test.
import fs from 'node:fs';
import assert from 'node:assert/strict';
let Processor;
globalThis.sampleRate=44100;globalThis.currentTime=0;
globalThis.AudioWorkletProcessor=class { constructor(){ this.port={messages:[],postMessage(m){this.messages.push(m);}}; } };
globalThis.registerProcessor=(_name,p)=>{Processor=p;};
await import('../../build/engine-pair-morph/worklet.js');
const p=new Processor();
await p.onMessage({type:'load',wasm:fs.readFileSync('build/engine-pair-morph/felucca.wasm'),sectors:{}});
assert.ok(p.port.messages.some(m=>m.type==='ready'));
function render(ms){
  for(let i=0;i<Math.ceil(ms*44.1/128);i++){
    p.process([],[[new Float32Array(128),new Float32Array(128)]]);
    globalThis.currentTime+=128/44100;
  }
  p.publish();
}
const send=(op,args=[])=>p.onMessage({type:'pair',op,args});
render(1200);await send('prepare',[0,2,14,16]);render(100);
let state=p.port.messages.filter(m=>m.type==='pair-state').at(-1);
assert.equal(state.on,1);assert.equal(state.budget,1);
assert.deepEqual(state.partners,[2,-1,0,-1]);
assert.deepEqual(state.engines.filter((_,t)=>t===0||t===2),[14,16]);
await send('page');render(20);
assert.equal(state.motion,0); await send('motion',[1]);render(30);
state=p.port.messages.filter(m=>m.type==='pair-state').at(-1);assert.equal(state.motion,1);
await send('select',[2]);await p.onMessage({type:'keys',mask:1<<7});render(100);
state=p.port.messages.filter(m=>m.type==='pair-state').at(-1);
assert.equal(state.sel,2);assert.deepEqual([state.gates[0],state.gates[2]],[1,1]);
await send('morph',[2,32767]);render(50);
state=p.port.messages.filter(m=>m.type==='pair-state').at(-1);assert.equal(state.effective,32767);
assert.equal(state.gains[0],0);assert.equal(state.gains[2],32767);
await send('lfo',[2,1,11,-16]);await send('lfo2_rate',[2,67]);render(30);
state=p.port.messages.filter(m=>m.type==='pair-state').at(-1);assert.equal(state.depths[1],-16);assert.equal(state.rate2,67);
await send('save');const snapshot=p.port.messages.filter(m=>m.type==='pair-snapshot').at(-1);
assert.equal(snapshot.bytes.length,4032);
await send('off');render(40);await p.onMessage({type:'pair',op:'restore',bytes:snapshot.bytes});render(40);
assert.ok(p.port.messages.some(m=>m.type==='pair-result' && m.message.startsWith('Snapshot restored')));
state=p.port.messages.filter(m=>m.type==='pair-state').at(-1);assert.equal(state.on,1);assert.equal(state.base,32767);
assert.equal(state.motion,1);
assert.deepEqual([state.gates[0],state.gates[2]],[0,0]);
console.log('pair worklet: load, prepare, follower keys, morph, optional motion, independent LFO2, snapshot and restore passed');
