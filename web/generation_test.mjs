// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 bonszaimusic
// Real 1.x INFO/capsule fixtures, modeled Classic wire, no USB/device claim.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import vm from 'node:vm';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {generationInfo,detectGeneration,updateEntry,requireClassicReturnBackup,V1_HEADER,CLASSIC_HEADER} from './generation.js';
import {bkManifest,bkU32,bkR32,bkCrc,bkPack,bkUnpack,captureBackup,restoreBackup,readBackup,readBackupFile,classicEditorBackup,BackupConnection} from './fm1backup.js';
// Exact published d85ecb7 web/fm1backup.js, copied as a read-only client fixture;
// no Classic firmware/site build is performed by this test.
import {readBackup as classicReadBackup} from '../tests/fixtures/classic-fm1backup.mjs';
assert.equal(createHash('sha256').update(fs.readFileSync(new URL('../tests/fixtures/classic-fm1backup.mjs',import.meta.url))).digest('hex'),
 '385a07117c743adab9a20f14b05fcd2e8c76abb60f08c7faa3faf86fec407ac9','published Classic parser fixture must remain byte-identical');
const fixture=JSON.parse(fs.readFileSync(new URL('../tests/fixtures/generation-info.json',import.meta.url)));
const cap=new Uint8Array(fs.readFileSync(new URL('../tests/format2_project_capsule.bin',import.meta.url)));
const bank=new Uint8Array(3080),bv=new DataView(bank.buffer);
bv.setUint32(0,0x32425055,true);bv.setUint16(4,192,true);bv.setUint16(6,16,true);
const objects=new Map([[0,cap],[1,new Uint8Array([1,2,3])],[2,cap],[3,new Uint8Array()],[4,new Uint8Array()],
                       [5,new Uint8Array()],[6,bank],[7,new Uint8Array()]]);
const catalog=(gen,items=objects)=>{
 const ids=gen===2?[0,1,2,3,4,5,6,7]:[0,1,2,3,4,5,6,7,8,9,32,33,34];
 return [gen,0,ids.length,...ids.flatMap(id=>{const b=items.get(id)||new Uint8Array();return [id,...bkU32(b.length),...bkU32(bkCrc(b))];})];
};
for(const gen of [1,2]) {
 const calls=[];
 const info=await detectGeneration(async(h,c,a)=>{
  calls.push({h,c,a});
  if(gen===1&&h===V1_HEADER)throw new Error('timeout');
  return c===1?fixture[gen===2?'modern':'classic']:catalog(gen);
 });
 assert.equal(info.generation,gen);
 assert(calls.every(x=>[1,65].includes(x.c)&&x.a.length===0),'discovery only reads INFO/LIST');
 assert.deepEqual(info.header,gen===2?V1_HEADER:CLASSIC_HEADER);
}
assert.equal(generationInfo(fixture.modern).protocol,18);
const renamed=[...new TextEncoder().encode('UNRELATED NAME'),0,...fixture.modern.slice(fixture.modern.indexOf(0)+1)];
assert.equal(generationInfo(renamed).generation,2,'display text does not identify generation');
await assert.rejects(()=>detectGeneration(async(h,c)=>c===1?fixture.modern:catalog(1)),/mismatch|Classic/);
assert.throws(()=>generationInfo([0,0x60,2,18]));
const wrongRevision=fixture.modern.slice();wrongRevision[wrongRevision.indexOf(0x60)+2]=17;
assert.throws(()=>generationInfo(wrongRevision),/revision/);
const development17=JSON.parse(fs.readFileSync(new URL('../tests/fixtures/format2-protocol17-info.json',import.meta.url))).info;
assert.throws(()=>generationInfo(development17),/revision 17/,'ordinary clients remain strict');
const developmentRequests=[];
const developmentRequest=async(h,c,a)=>{
 developmentRequests.push({h,c,a});
 if(h===V1_HEADER)throw new Error('timeout');
 return c===1?development17:catalog(2);
};
await assert.rejects(()=>detectGeneration(developmentRequest),/revision 17/);
const development=await detectGeneration(developmentRequest,{allowLegacyFormat2Update:true});
assert.equal(development.protocol,17);assert.equal(development.generation,2);assert.equal(development.updateOnly,true);
assert.deepEqual(development.header,CLASSIC_HEADER);
assert(developmentRequests.every(x=>[1,65].includes(x.c)&&x.a.length===0));
for(const revision of [16,19]) {
 const bad=development17.slice();bad[bad.indexOf(0x60)+2]=revision;
 assert.throws(()=>generationInfo(bad,{allowLegacyFormat2Update:true}),/revision/);
}
const wrongLayout=development17.slice();wrongLayout[wrongLayout.indexOf(0)+2]=113;
assert.throws(()=>generationInfo(wrongLayout,{allowLegacyFormat2Update:true}),/revision/);
await assert.rejects(()=>detectGeneration(async(h,c,a)=>c===65?catalog(1):developmentRequest(h,c,a),
 {allowLegacyFormat2Update:true}),/mismatch/);
assert.throws(()=>updateEntry(2,{protocol:17}));
assert.deepEqual(updateEntry(2,{protocol:17,allowLegacyFormat2Update:true}),updateEntry(1));
assert.throws(()=>updateEntry(2,{protocol:16,allowLegacyFormat2Update:true}));
// Exercise the real backup connection, including its write-command refusal.
const input={state:'connected',onmidimessage:null},sent=[];
const output={state:'connected',send(frame){
 const h=Array.from(frame.slice(1,4)),cmd=frame[4],a=frame.slice(5,-1);sent.push(Array.from(frame));
 if(h.join()===V1_HEADER.join())return; // actual protocol17 ignores modern framing
 const data=cmd===1?development17:cmd===65?catalog(2):(()=>{
  const b=objects.get(a[0]),off=bkR32(a,1),n=a[6]|a[7]<<7;
  return [a[0],0,...bkU32(off),n&127,n>>7,...bkPack(b.subarray(off,off+n))];
 })();
 queueMicrotask(()=>input.onmidimessage?.({data:Uint8Array.from([240,...h,cmd,...data,247])}));
}};
const oldConnection=new BackupConnection(input,output);
await oldConnection.identify({allowLegacyFormat2Update:true});
const saved17=await captureBackup((r,o)=>oldConnection.request(r,o),development.version);
assert.equal(readBackup(saved17).generation,2);
const sentBefore=sent.length;
await assert.rejects(()=>oldConnection.request([67,[0,0]]),/read-only/);
await assert.rejects(()=>oldConnection.request([3,[0,0]]),/read-only/);
assert.equal(sent.length,sentBefore,'no destructive packet leaves the read-only bridge');
assert(sent.every(f=>[1,65,66].includes(f[4])));oldConnection.close();

assert.notDeepEqual(updateEntry(2),updateEntry(1));
assert.throws(()=>updateEntry(0));

function device(gen, initial=objects) {
 const data=new Map([...initial].map(([k,v])=>[k,v.slice()])),calls=[];let stage;
 return {data,calls,request:async([cmd,a])=>{
  calls.push([cmd,a]);
  if(cmd===65)return catalog(gen,data);
  if(cmd===66){const b=data.get(a[0]),off=bkR32(a,1),n=a[6]|a[7]<<7;
   return [a[0],0,...bkU32(off),n&127,n>>7,...bkPack(b.subarray(off,off+n))];}
  assert.equal(cmd,67);const[op,id]=a;
  if(op===0)stage={id,size:bkR32(a,2),crc:bkR32(a,7),bytes:[]};
  if(op===1)stage.bytes.push(...bkUnpack(a.slice(7),Math.min(256,stage.size-bkR32(a,2))));
  if(op===2){const b=Uint8Array.from(stage.bytes);assert.equal(bkCrc(b),stage.crc);data.set(id,b);}
  return [op,id,0];
 }};
}
const source=device(2),archive=await captureBackup(source.request,'fixture');
assert.equal(archive.format,'CaravelBackup2');assert.equal(archive.objects.length,8);
assert.deepEqual(readBackup(JSON.stringify(archive)).objects[0].bytes,cap);
const target=device(2);await restoreBackup(target.request,archive);
assert(target.calls[0][0]===65,'LIST before PUT');assert.deepEqual(target.data.get(0),cap);
const old=device(1);await assert.rejects(()=>restoreBackup(old.request,archive),/generations differ/);
assert(old.calls.every(([cmd])=>cmd===65),'F2 archive to Classic: no destructive request');
const classic={format:'felucca-backup',version:1,objects:[0,1,2,3,4,5,6,7,8,9,32,33,34].map(id=>{
 const bytes=id<2?new Uint8Array([1]):new Uint8Array();return {id,size:bytes.length,crc:bkCrc(bytes),data:Buffer.from(bytes).toString('base64')};})};
const newer=device(2);await assert.rejects(()=>restoreBackup(newer.request,classic),/generations differ/);
assert(newer.calls.every(([cmd])=>cmd===65),'Classic archive to F2: no destructive request');
const damaged=structuredClone(archive);damaged.objects[0].data=Buffer.from(new Uint8Array(6144)).toString('base64');
damaged.objects[0].crc=bkCrc(new Uint8Array(6144));
assert.throws(()=>readBackup(damaged),/capsule/);
assert.throws(()=>bkManifest([2,...catalog(1).slice(1)]));
assert.throws(()=>requireClassicReturnBackup(readBackup(archive)));

const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'caravel-generation-'));
try {
 const root=new URL('..',import.meta.url).pathname;
 execFileSync('python3',['-c',String.raw`
import pathlib,sys,struct
sys.path.insert(0,str(pathlib.Path(sys.argv[1])/'tools'))
import caravel_backup as cb
cap=(pathlib.Path(sys.argv[1])/'tests/format2_project_capsule.bin').read_bytes()
bank=struct.pack('<IHH',0x32425055,192,16)+bytes(3072)
objects={0:cap,1:bytes([1,2,3]),2:cap,3:b'',4:b'',5:b'',6:bank,7:b''}
cb.save_archive(pathlib.Path(sys.argv[2])/'format2.zip',objects,2)
payload=bytearray([0,0]);left=5593
while left:
 n=min(130,left)
 if 0<left-n<3:n=left-3
 payload+=bytes([128+n-3,1,0]);left-=n
classic=bytearray(7200);struct.pack_into('<6I',classic,0,0x314d5048,5,7200,1,0,0)
struct.pack_into('<II',classic,24,len(payload),5594);classic[3872:3872+len(payload)]=payload
struct.pack_into('<I',classic,16,cb.fnv(classic[32:]));struct.pack_into('<I',classic,20,cb.fnv(classic[:20]))
old={k:(bytes(classic) if k in (0,2) else bytes([1,2,3]) if k==1 else b'') for k in cb.IDS}
cb.save_archive(pathlib.Path(sys.argv[2])/'classic.zip',old,1)
`,root,tmp]);
 const zipped=await readBackupFile(fs.readFileSync(path.join(tmp,'format2.zip')));
 assert.equal(zipped.generation,2);assert.deepEqual(zipped.objects[0].bytes,cap);
 const classicZip=await readBackupFile(fs.readFileSync(path.join(tmp,'classic.zip')));
 requireClassicReturnBackup(classicZip);
 const classicJSON=classicEditorBackup(classicZip);
 assert.equal(classicJSON.format,'felucca-backup');assert.equal(classicJSON.version,1);
 assert(classicJSON.objects.every(o=>!('bytes' in o)&&!('file' in o)));
 const frozenRead=classicReadBackup(JSON.stringify(classicJSON));
 for(const [i,o] of frozenRead.objects.entries())assert.deepEqual(o.bytes,classicZip.objects[i].bytes);
 assert.throws(()=>classicReadBackup(fs.readFileSync(path.join(tmp,'classic.zip')).toString('utf8')));
 assert.throws(()=>classicEditorBackup(archive));
 const corrupt=new Uint8Array(fs.readFileSync(path.join(tmp,'format2.zip')));corrupt[40]^=1;
 await assert.rejects(()=>readBackupFile(corrupt));
} finally {fs.rmSync(tmp,{recursive:true,force:true});}
// Execute the production editor parser on its real INFO, including backup caps.
const html=fs.readFileSync(new URL('./editor.html',import.meta.url),'utf8');
const proto=html.slice(html.indexOf('/*PROTO-BEGIN*/'),html.indexOf('/*PROTO-END*/'));
const editor=vm.runInNewContext(proto+';({parse,CMD,frame,unframe})',{setTimeout,clearTimeout});
const parsed=editor.parse[1](fixture.modern);assert.equal(parsed.backupCaps,3);assert.equal(parsed.chainRows,0);
assert.deepEqual(Array.from(editor.frame(3,[0,1,0,64]).slice(1,4)),V1_HEADER);
assert.equal(editor.unframe([0xf0,...CLASSIC_HEADER,1,...fixture.classic,0xf7]),null);
console.log('generation: protocol 18/Classic discovery, real INFO/capsule, LIST1/2, JSON2, CLI ZIP1/2, bidirectional no-write rejection, editor framing PASS');
