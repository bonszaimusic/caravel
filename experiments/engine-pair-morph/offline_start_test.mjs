// SPDX-License-Identifier: GPL-3.0-only
// Startup regression with a minimal DOM: not a substitute for an Android browser test.
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const html=fs.readFileSync('build/engine-pair-morph/caravel-engine-pair.html','utf8');
assert.ok(!html.includes('import { midiKnob, KNOB_MODES } from "./worklet.js"'));
assert.ok(!html.includes('<script type="module">'));
assert.ok(!html.includes('fetch("felucca.wasm")'));
assert.ok(!html.includes('ac.audioWorklet.addModule('));
const scripts=[...html.matchAll(/<script[^>]*>([\s\S]*?)<\/script>/g)].map(m=>m[1]);
assert.equal(scripts.length,1);
const elements=new Map();
function element(){
  return {style:{setProperty(){}},classList:{toggle(){},add(){},remove(){}},listeners:{},
    setAttribute(){},appendChild(){},insertBefore(){},remove(){},querySelectorAll(){return [];},
    querySelector(){return element();},getBBox(){return {x:0,y:0,width:100,height:100};},
    addEventListener(type,fn){this.listeners[type]=fn;},
    getContext(){return {putImageData(){},createImageData(){return {data:new Uint8ClampedArray(240*240*4)};}};}};
}
const document={hidden:false,documentElement:element(),
  getElementById(id){if(!elements.has(id))elements.set(id,element());return elements.get(id);},
  createElement:element,createElementNS:element,addEventListener(){},
  fonts:{add(){},load(){return Promise.resolve();}}};
const inputListeners={};
const context=vm.createContext({document,navigator:{},console,Uint8Array,Uint8ClampedArray,Map,Set,Math,
  FontFace:class{load(){return Promise.resolve();}},
  matchMedia:()=>({matches:true,addEventListener(){}}),
  WebAssembly,queueMicrotask,Float32Array,
  localStorage:{getItem(){return null;},setItem(){}},addEventListener(type,fn){inputListeners[type]=fn;}});
vm.runInContext(scripts[0],context);
const button=document.getElementById('go');
assert.equal(typeof button.listeners.click,'function','inline bootstrap binds Switch on');
await button.listeners.click();
assert.equal(button.disabled,false,'failed audio startup permits retry');
assert.match(document.getElementById('status').textContent,/AudioWorklet/,'unsupported context shows a visible error');
const order=[];
let audioCallback;
context.AudioContext=class {
  sampleRate=44100;destination={};
  // Deliberately no AudioWorklet: reproduces a local Android document context.
  createScriptProcessor(size,inputs,outputs){
    assert.equal(size,1024);assert.equal(inputs,0);assert.equal(outputs,2);order.push('callback');
    return {connect(){},disconnect(){},set onaudioprocess(fn){audioCallback=fn;}};
  }
  resume(){order.push('resume');return Promise.resolve();}
};
await button.listeners.click();
assert.deepEqual(order,['resume','callback'],'audio unlock happens before callback setup');
assert.equal(document.getElementById('start').hidden,true);
assert.equal(document.getElementById('pair-controls').disabled,false);
assert.equal(button.textContent,'On');
let peak=0;
for(let i=0;i<60;i++){
  const channels=[new Float32Array(1024),new Float32Array(1024)];
  audioCallback({outputBuffer:{getChannelData:c=>channels[c]}});
  await new Promise(resolve=>setImmediate(resolve));
}
for(const [id,value] of [['a','0'],['b','2'],['ea','14'],['eb','16']])document.getElementById('pair-'+id).value=value;
await document.getElementById('pair-prepare').onclick();
await new Promise(resolve=>setImmediate(resolve));
document.getElementById('pair-motion').checked=true;
document.getElementById('pair-motion').onchange({target:{checked:true}});
await new Promise(resolve=>setImmediate(resolve));
await document.getElementById('pair-demo').onclick();
inputListeners.keydown({code:'KeyZ',target:{closest(){return false;},matches(){return false;}},preventDefault(){}});
await new Promise(resolve=>setImmediate(resolve));
for(let i=0;i<12;i++){
  const channels=[new Float32Array(1024),new Float32Array(1024)];
  audioCallback({outputBuffer:{getChannelData:c=>channels[c]}});
  for(const channel of channels)for(const x of channel)peak=Math.max(peak,Math.abs(x));
  await new Promise(resolve=>setImmediate(resolve));
}
assert.match(document.getElementById('pair-result').textContent,/Pair ready/);
assert.match(document.getElementById('pair-state').textContent,/T1 \+ T3/);
assert.ok(peak>0.001,'actual standalone callback renders audible WASM notes');
assert.equal(document.getElementById('pair-motion').checked,true,'motion checkbox reaches the actual WASM state');
document.getElementById('pair-page').onclick();
await new Promise(resolve=>setImmediate(resolve));
for(let i=0;i<5;i++){
  const channels=[new Float32Array(1024),new Float32Array(1024)];
  audioCallback({outputBuffer:{getChannelData:c=>channels[c]}});
  await new Promise(resolve=>setImmediate(resolve));
}
assert.equal(document.getElementById('pair-morph').disabled,false,'paired member edits the common morph');
console.log('offline start: real WASM ready and audible without AudioWorklet; retry, pair setup, keyboard and optional motion switch passed');
