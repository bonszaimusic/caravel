// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 bonszaimusic
// Run the real 1.x storage functions against a shared origin with Classic data.
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
let editor=fs.readFileSync(new URL('./editor.html',import.meta.url),'utf8');
let emu=fs.readFileSync(new URL('./emu/index.html',import.meta.url),'utf8');
if(process.env.STORAGE_NEGATIVE) {
 editor=editor.replaceAll('caravel-v1-editor','felucca-editor');
 emu=emu.replaceAll('caravel-v1-emu','felucca-emu');
}
const classicFlash=JSON.stringify({'4096':Buffer.from([1,2,3]).toString('base64')});
const values=new Map([['felucca-editor-tab','tracks'],['felucca-emu-flash',classicFlash],
 ['felucca-emu-knobs','1'],['felucca-emu-knobmode','rel']]);
const before=JSON.stringify([...values]);
const localStorage={getItem:k=>values.get(k)??null,setItem:(k,v)=>values.set(k,String(v))};
const slice=(text,start,end)=>{const a=text.indexOf(start),b=text.indexOf(end,a);assert(a>=0&&b>a,'storage source markers');return text.slice(a,b);};
const tabCode=slice(editor,'const TABS =','const hasTracks =');
const tab=vm.runInNewContext(tabCode+';({tab,TAB_KEY})',{location:{hash:''},localStorage});
assert.equal(tab.tab,'sound','1.x must not adopt the Classic tab');
assert.notEqual(tab.TAB_KEY,'felucca-editor-tab');
const keys=emu.match(/const STORE = [^;]+;/)?.[0];assert(keys);
const flashFns=slice(emu,'function loadFlash()','// ------------------------------------------------------------------ Web MIDI');
const ctx={localStorage,atob:s=>Buffer.from(s,'base64').toString('binary'),btoa:s=>Buffer.from(s,'binary').toString('base64')};
const flash=vm.runInNewContext(keys+flashFns+';({loadFlash,saveFlash,STORE,KNOBS_ON,KNOB_MODE})',ctx);
assert.equal(Object.keys(flash.loadFlash()).length,0,'1.x must not open Classic NOR');
assert(flash.saveFlash({'4096':new Uint8Array([7,8,9])}));
assert.deepEqual(Array.from(flash.loadFlash()['4096']),[7,8,9]);
for(const k of ['STORE','KNOBS_ON','KNOB_MODE'])assert(flash[k].startsWith('caravel-v1-'));
assert.equal(values.get('felucca-emu-flash'),classicFlash,'Classic NOR remains byte-identical');
assert.equal(JSON.stringify([...values].filter(([k])=>!k.startsWith('caravel-v1-'))),before);
const opened=[];
const indexedDB={open(name){opened.push(name);const r={};queueMicrotask(()=>{
 const rows=name==='felucca-editor'?[{id:'classic-only',engine:'FM6'}]:[];
 r.result={transaction(){const tx={objectStore(){return {getAll(){const q={};queueMicrotask(()=>{q.result=rows;q.onsuccess?.();tx.oncomplete?.();});return q;}};}};return tx;}};
 r.onsuccess?.();
 });return r;}};
const storeCode=slice(editor,'const store = (() => {','\nlet lib =');
const store=vm.runInNewContext(storeCode+';store',{indexedDB,queueMicrotask});
assert.equal((await store.all()).length,0,'1.x must not open the Classic patch library');
assert.deepEqual(opened,['caravel-v1-editor']);
console.log('generation storage: actual editor tab, IndexedDB library, emulator NOR and controls isolated; Classic data untouched');
