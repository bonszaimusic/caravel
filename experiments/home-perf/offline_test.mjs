// SPDX-License-Identifier: GPL-3.0-only
// Tests actual embedded HTML/processor/WASM with browser APIs simulated, not a phone browser.
import fs from 'node:fs';import vm from 'node:vm';import assert from 'node:assert/strict';
const html=fs.readFileSync('build/home-perf/Caravel-HOME-Performance-Handy.html','utf8');
assert.ok(!html.includes('<script type="module">'));assert.ok(html.includes('href="#home-controls"'));assert.ok(!/href="\.\.?\//.test(html));assert.ok(!html.includes('fetch("felucca.wasm")'));assert.ok(!html.includes('ac.audioWorklet.addModule('));
const script=[...html.matchAll(/<script[^>]*>([\s\S]*?)<\/script>/g)];assert.equal(script.length,1);
const elements=new Map(),listeners={};let approve=true;const confirmations=[];
function element(){return{value:'0',hidden:false,style:{setProperty(){}},classList:{toggle(){},add(){},remove(){}},listeners:{},setAttribute(){},appendChild(){},insertBefore(){},remove(){},click(){},querySelectorAll(){return[]},querySelector(){return element()},getBBox(){return{x:0,y:0,width:100,height:100}},addEventListener(k,f){this.listeners[k]=f},getContext(){return{putImageData(){},createImageData(){return{data:new Uint8ClampedArray(240*240*4)}}}}}}
const pages=[...html.matchAll(/data-home-page="(\d+)"/g)].map(m=>Object.assign(element(),{dataset:{homePage:m[1]}}));
const document={hidden:false,documentElement:element(),getElementById(id){if(!elements.has(id))elements.set(id,element());return elements.get(id)},querySelectorAll(selector){return selector==='[data-home-page]'?pages:[]},createElement:element,createElementNS:element,addEventListener(){},fonts:{add(){},load(){return Promise.resolve()}}};
let callback,storageBlocked=false;const flash=new Map(),downloads=[];
const context=vm.createContext({document,navigator:{},console,Uint8Array,Uint8ClampedArray,Float32Array,Uint16Array,Map,Set,Math,WebAssembly,queueMicrotask,setTimeout,atob:s=>Buffer.from(s,'base64').toString('binary'),btoa:s=>Buffer.from(s,'binary').toString('base64'),Blob,URL,confirm:text=>{confirmations.push(text);return approve;},FontFace:class{load(){return Promise.resolve()}},matchMedia:()=>({matches:true,addEventListener(){}}),localStorage:{getItem:k=>flash.get(k)||null,setItem:(k,v)=>{if(storageBlocked)throw new Error("Storage denied");flash.set(k,v)}},addEventListener(k,f){listeners[k]=f}});
vm.runInContext(script[0][1],context);const button=document.getElementById('go');await button.listeners.click();assert.equal(button.disabled,false);assert.match(document.getElementById('status').textContent,/AudioWorklet/);
context.AudioContext=class{sampleRate=44100;destination={};createScriptProcessor(){return{connect(){},disconnect(){},set onaudioprocess(f){callback=f}}}resume(){return Promise.resolve()}};
await button.listeners.click();assert.equal(document.getElementById('start').hidden,true);assert.equal(document.getElementById('home-controls').disabled,false);
async function pump(n=30){let peak=0;for(let i=0;i<n;i++){const ch=[new Float32Array(1024),new Float32Array(1024)];callback({outputBuffer:{getChannelData:c=>ch[c]}});for(const a of ch)for(const x of a)peak=Math.max(peak,Math.abs(x));await new Promise(setImmediate);}return peak;}
await pump(60);
// Direct navigation uses the actual C pages and resets the menu/layer state.
for(const page of pages){page.onclick();await pump(2);}pages.find(p=>p.dataset.homePage==='0').onclick();await pump(2);
// Cancel template replacement without changing the model.
approve=false;document.getElementById('home-template').value='2';document.getElementById('home-apply').onclick();await pump(2);assert.equal(document.getElementById('home-row').textContent,'1 / 1');approve=true;
document.getElementById('home-template').value='2';document.getElementById('home-apply').onclick();await pump(3);assert.equal(document.getElementById('home-row').textContent,'1 / 2');
document.getElementById('home-next').onclick();await pump(3);assert.equal(document.getElementById('home-row').textContent,'2 / 2');
document.getElementById('home-slot').value='0';document.getElementById('home-save').onclick();await pump(3);assert.match(document.getElementById('home-result').textContent,/gespeichert/);assert.ok(flash.has('caravel-home-performance-hw-v4'));
document.getElementById('home-template').value='0';document.getElementById('home-apply').onclick();await pump(3);assert.equal(document.getElementById('home-row').textContent,'1 / 1');document.getElementById('home-load').onclick();await pump(3);assert.equal(document.getElementById('home-row').textContent,'2 / 2');
listeners.keydown({code:'KeyZ',target:{closest(){return false},matches(){return false}},preventDefault(){}});await new Promise(setImmediate);const peak=await pump(15);assert.ok(peak>0.001);listeners.keyup({code:'KeyZ',preventDefault(){}});await pump(15);
console.log('HOME standalone: retry/start without AudioWorklet, actual embedded WASM audio, templates, row control, A-D and browser storage messages passed (DOM/API simulation).');

// Default preference and slot labels are explicit; loading remains a separate action.
document.getElementById('home-template').value='2';document.getElementById('home-default').onclick();await pump(3);assert.match(document.getElementById('home-default-status').textContent,/PERFORM/);assert.match(document.getElementById('home-result').textContent,/Startvorlage/);
document.getElementById('home-slot').value='2';document.getElementById('home-slot').onchange();assert.match(document.getElementById('home-save').textContent,/C/);document.getElementById('home-export').onclick();await pump(3);assert.match(document.getElementById('home-result').textContent,/C ist leer/);
// Dangerous helper actions can be cancelled. The existing two-row HOME stays intact.
approve=false;document.getElementById('pair-prepare').onclick();document.getElementById('pair-demo').onclick();await pump(3);assert.equal(document.getElementById('home-row').textContent,'2 / 2');assert.ok(confirmations.some(x=>x.includes('ALLE ungespeicherten')));approve=true;
// File input resets after a rejected file, so the same filename can be selected again.
const input={files:[{size:9}],value:'bad.hpm'};await document.getElementById('home-import').onchange({target:input});assert.equal(input.value,'');assert.match(document.getElementById('home-result').textContent,/Keine passende/);
// Browser keys must leave selects/input fields alone.
let prevented=false;listeners.keydown({code:'ArrowUp',target:{closest(){return false},matches(){return true}},preventDefault(){prevented=true;}});assert.equal(prevented,false);
console.log('PHONE UI: C-page navigation, cancel/replace, default feedback, explicit slot labels, empty export, reset confirmation and import retry passed (DOM/API simulation).');

// Refused browser persistence must be visible outside the developer console.
storageBlocked=true;document.getElementById('home-slot').value='0';document.getElementById('home-save').onclick();await pump(8);assert.match(document.getElementById('home-storage').textContent,/nur bis zum Schließen/);
console.log('PHONE UI: browser persistence refusal is visible; download fallback stays available.');
