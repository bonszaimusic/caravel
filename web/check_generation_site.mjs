// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 bonszaimusic
// Execute the actual assembled site's transport; keywords cannot pass this gate.
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
const root=path.resolve(process.argv[2]);
const read=p=>fs.readFileSync(path.join(root,p),'utf8');
const installer=read('webapp/installer/index.html'),editor=read('webapp/editor/index.html');
const script=installer.match(/<script type="module">([\s\S]*)<\/script>/)?.[1];
assert(script,'installer module');
const end=script.indexOf('const $ =');assert(end>0,'assembled installer boundary');
const library=script.slice(0,end);
const strip=s=>s.replace(/^export\s+/gm,'').replace(/^import .*?;\n/gm,'');
// The editor's actual imported libraries must equal the ones exercised inside
// the installer, avoiding a passing receipt for unrelated or stale JS files.
for(const name of ['generation.js','fm1backup.js'])
 assert(library.includes(strip(read('webapp/editor/'+name))),`matching deployed ${name}`);
const env={TextEncoder,TextDecoder,Uint8Array,DataView,Blob,DecompressionStream,setTimeout,clearTimeout,btoa,atob};
const api=vm.runInNewContext(library+';({detectGeneration,updateEntry,restoreBackup,readBackup})',{...env});
const fixture=JSON.parse(fs.readFileSync(new URL('../tests/fixtures/generation-info.json',import.meta.url)));
const ids=g=>g===2?[0,1,2,3,4,5,6,7]:[0,1,2,3,4,5,6,7,8,9,32,33,34];
const list=g=>[g,0,ids(g).length,...ids(g).flatMap(id=>[id,0,0,0,0,0,0,0,0,0,0])];
for(const generation of [1,2]) {
 const calls=[];
 const result=await api.detectGeneration(async(header,cmd,args)=>{
  calls.push([header,cmd,args]);
  if(generation===1&&header[1]===0x43)throw new Error('modeled Classic ignores new framing');
  return cmd===1?fixture[generation===2?'modern':'classic']:list(generation);
 });
 assert.equal(result.generation,generation);
 assert.deepEqual(Array.from(result.header),generation===2?[0x7d,0x43,0x41]:[0x7d,0x46,0x4c]);
 assert(calls.every(([,cmd,args])=>[1,65].includes(cmd)&&!args.length),'read-only discovery');
}
await assert.rejects(()=>api.detectGeneration(async(h,cmd)=>cmd===1?fixture.modern:list(1)));
assert.notDeepEqual(Array.from(api.updateEntry(1)),Array.from(api.updateEntry(2)));
const start=editor.indexOf('/*PROTO-BEGIN*/'),stop=editor.indexOf('/*PROTO-END*/');assert(start>=0&&stop>start);
// Contextifying an object is persistent. Use distinct global scopes because the
// installer and editor both define Link, as they do in separate browser pages.
const proto=vm.runInNewContext(editor.slice(start,stop)+';({frame,unframe,parse})',{...env});
assert.deepEqual(Array.from(proto.frame(3,[0,1,0,64]).slice(1,4)),[0x7d,0x43,0x41]);
assert.equal(proto.unframe([0xf0,0x7d,0x46,0x4c,1,...fixture.classic,0xf7]),null);
assert.equal(proto.parse[1](fixture.modern).backupCaps,3);
const emu=read('emulator/index.html');
// Exercise real storage functions on one modeled origin seeded with Classic
// data. A key-name comment alone cannot pass these checks.
const slice=(text,from,to)=>{
 const a=text.indexOf(from),b=text.indexOf(to,a);assert(a>=0&&b>a,'storage source boundaries');
 return text.slice(a,b);
};
const values=new Map([['felucca-editor-tab','tracks'],['felucca-emu-flash',JSON.stringify({'4096':btoa('classic')})],
 ['felucca-emu-knobs','1'],['felucca-emu-knobmode','rel']]);
const before=JSON.stringify([...values]);
const localStorage={getItem:k=>values.get(k)??null,setItem:(k,v)=>values.set(k,String(v))};
const tab=vm.runInNewContext(slice(editor,'const TABS =','const hasTracks =')+';({tab,TAB_KEY})',
 {location:{hash:''},localStorage});
assert.equal(tab.tab,'sound','1.x must ignore the Classic tab');
assert(tab.TAB_KEY.startsWith('caravel-v1-'));
const keys=emu.match(/const STORE = [^;]+;/)?.[0];assert(keys,'emulator storage constants');
const flash=vm.runInNewContext(keys+slice(emu,'function loadFlash()','// ------------------------------------------------------------------ Web MIDI')+
 ';({loadFlash,saveFlash,STORE,KNOBS_ON,KNOB_MODE})',{localStorage,btoa,atob});
assert.equal(Object.keys(flash.loadFlash()).length,0,'1.x must ignore Classic NOR');
assert(flash.saveFlash({'4096':new Uint8Array([7,8,9])}));
assert.deepEqual(Array.from(flash.loadFlash()['4096']),[7,8,9]);
for(const key of ['STORE','KNOBS_ON','KNOB_MODE'])assert(flash[key].startsWith('caravel-v1-'));
assert.equal(JSON.stringify([...values].filter(([key])=>!key.startsWith('caravel-v1-'))),before,'Classic data untouched');
const opened=[];
const indexedDB={open(name){opened.push(name);const request={};queueMicrotask(()=>{
 const rows=name==='felucca-editor'?[{id:'classic-only',engine:'FM6'}]:[];
 request.result={transaction(){const tx={objectStore(){return {getAll(){const q={};queueMicrotask(()=>{
  q.result=rows;q.onsuccess?.();tx.oncomplete?.();});return q;}};}};return tx;}};
 request.onsuccess?.();});return request;}};
const store=vm.runInNewContext(slice(editor,'const store = (() => {','\nlet lib =')+';store',{indexedDB,queueMicrotask});
assert.equal((await store.all()).length,0,'1.x must ignore the Classic patch library');
assert.deepEqual(opened,['caravel-v1-editor']);
const files=Object.fromEntries(['webapp/installer/index.html','webapp/editor/index.html',
 'webapp/editor/generation.js','webapp/editor/fm1backup.js','emulator/index.html']
 .map(p=>[p,createHash('sha256').update(fs.readFileSync(path.join(root,p))).digest('hex')]));
console.log(JSON.stringify({result:'PASS',tests:['actual-site-generation-discovery','INFO-LIST-mismatch-rejection',
 'separate-update-entry','editor-rejects-Classic-framing','FORMAT2-backup-capability','shared-origin-storage-isolation'],
 limitations:'Modeled Classic replies and native 1.x fixture, no browser/USB/FM-1 test',files}));
