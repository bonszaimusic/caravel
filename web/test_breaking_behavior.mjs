// SPDX-License-Identifier: GPL-3.0-only
// Simulated DOM + MIDI installer guard. No browser, USB or firmware writes.
// Run: node web/test_breaking_behavior.mjs
import assert from "node:assert/strict";
import fs from "node:fs";
import vm from "node:vm";
import { classicEditorBackup, bkCrc } from "./fm1backup.js";

const html = fs.readFileSync(new URL("./index_pkg.html", import.meta.url), "utf8");
let script = html.match(/<script type="module">([\s\S]*?)<\/script>/)?.[1];
assert(script?.includes("/*LIB*/") && script.includes("/*META*/"));
if (process.env.INSTALLER_RACE_NEGATIVE) {
  const a=script.indexOf('$("stock-file").addEventListener'),b=script.indexOf('function saveArchive',a);
  script=script.slice(0,a)+script.slice(a,b).replaceAll('lockInstaller(writing)','lockInstaller(false)')+script.slice(b);
}
if (process.env.CLASSIC_RETURN_NEGATIVE) {
  script=script.replace('    saveArchive(classicEditorBackup(selectedBackup),"Caravel-Classic-restore");','');
}
async function setup({loader=false,supported=true,failBackup=false,answers=[],stockWait=null,installWait=null} = {}) {
  const stats = {events:[],writes:0,backups:0,downloads:[],confirmations:[]};
  const objectURLs = new Map();
  const elements = new Map();
  function el(id) {
    if (!elements.has(id)) elements.set(id,{
      id,disabled:true,checked:false,value:"",textContent:"",listeners:{},
      addEventListener(type,fn){this.listeners[type]=fn;}
    });
    return elements.get(id);
  }
  const document = {
    documentElement:{lang:"en"},getElementById:el,querySelectorAll:()=>[],
    createElement:()=>({click(){
      stats.events.push("download");
      stats.downloads.push({name:this.download,blob:objectURLs.get(this.href)});
    },remove(){}}),
    body:{append(){}}
  };
  class Updater {
    async find(predicate) {
      const model=loader?"ota-FM-1":"FM-1";
      if (!predicate({model})) return null;
      return {id:{model},link:{input:{},output:{},close(){}}};
    }
    async install(){stats.writes++;stats.events.push("install");await installWait;}
    async resume(){stats.writes++;stats.events.push("resume");return true;}
  }
  class BackupConnection {
    async identify(){return {generation:supported?2:0,version:"CARAVEL v1.0.0"};}
    close(){}
  }
  const ctx = vm.createContext({
    document, window:{addEventListener(){}},
    navigator:{language:"en",requestMIDIAccess:async()=>({})},
    fetch:async()=>({ok:true,arrayBuffer:async()=>new Uint8Array([42]).buffer}),
    validateCaravelPackage:async()=>({image:{}}),
    Updater,BackupConnection,
    captureBackup:async()=>{
      stats.backups++;stats.events.push("backup");
      if(failBackup)throw Error("simulated backup failure");
      return {format:"CaravelBackup2",objects:[]};
    },
    validateStockPackage:async()=>{await stockWait;return {};},STOCK_V15_SIZE:1,
    // File transport and capsule validity have separate real-ZIP tests. This
    // DOM stand-in checks download/confirmation/write ordering with the real
    // Classic JSON serializer.
    readBackupFile:async()=>({format:"felucca-backup",version:1,generation:1,
      objects:[0,1,2,3,4,5,6,7,8,9,32,33,34].map(id=>{
        const b=id<2?new Uint8Array([1]):new Uint8Array();
        return {id,size:b.length,crc:bkCrc(b),data:Buffer.from(b).toString("base64")};
      })}),
    requireClassicReturnBackup:a=>a,classicEditorBackup,
    Blob,URL:{createObjectURL:blob=>{const key="#"+objectURLs.size;objectURLs.set(key,blob);return key;},revokeObjectURL(){}},
    setTimeout(){},confirm(message){stats.confirmations.push(message);return answers.shift()??false;},console
  });
  vm.runInContext(script.replace("/*LIB*/","").replace("/*META*/",JSON.stringify({
    breaking:true,version:"1.0.0",product:"FM-1_900",pkg:"test.fwsc"
  })),ctx,{timeout:5000});
  await new Promise(resolve=>setImmediate(resolve));
  return {
    stats,el,
    click:()=>el("go").listeners.click(),
    unlock(){
      el("brk-understand").checked=true;el("brk-understand").listeners.input();
      el("brk-backup").checked=true;el("brk-backup").listeners.input();
      el("brk-type").value="UPDATE";el("brk-type").listeners.input();
      assert.equal(el("go").disabled,false);
    }
  };
}
let passed=0;
function pass(what){console.log("breaking behavior: "+what+" ok");passed++;}
const locked=await setup({answers:[true,true]});
assert.equal(locked.el("go").disabled,true);
await locked.click();
assert.equal(locked.stats.writes,0);
locked.el("brk-understand").checked=true;locked.el("brk-understand").listeners.input();
locked.el("brk-backup").checked=true;locked.el("brk-backup").listeners.input();
assert.equal(locked.el("go").disabled,true);
locked.el("brk-type").value="WRONG";locked.el("brk-type").listeners.input();
assert.equal(locked.el("go").disabled,true);
locked.el("brk-type").value="UPDATE";locked.el("brk-type").listeners.input();
assert.equal(locked.el("go").disabled,false);
pass("both checkboxes and typed UPDATE are required");
const complete=await setup({answers:[true]});complete.unlock();await complete.click();
assert.deepEqual(complete.stats.events,["backup","download","install"]);
pass("backup and download before write");
const denyLast=await setup({answers:[false]});denyLast.unlock();await denyLast.click();
assert.equal(denyLast.stats.backups,1);assert.equal(denyLast.stats.writes,0);
pass("final cancel forbids write");
const denyBackup=await setup({supported:false,answers:[false]});denyBackup.unlock();await denyBackup.click();
assert.equal(denyBackup.stats.backups,0);assert.equal(denyBackup.stats.writes,0);
pass("unsupported backup prompt cancel forbids write");
const denySecond=await setup({supported:false,answers:[true,false]});denySecond.unlock();await denySecond.click();
assert.equal(denySecond.stats.writes,0);
pass("second prompt cancel forbids write");
const failed=await setup({failBackup:true});failed.unlock();await failed.click();
assert.equal(failed.stats.writes,0);
pass("backup exception fails closed");
const loaderCancel=await setup({loader:true,answers:[false]});loaderCancel.unlock();await loaderCancel.click();
assert.equal(loaderCancel.stats.backups,0);assert.equal(loaderCancel.stats.writes,0);
assert.match(loaderCancel.el("status").textContent,/backup|loader|recovery/i);
assert.equal(loaderCancel.el("go").disabled,false,"cancelled recovery must unlock the installer");
pass("loader warns and cancel prevents resume");
const loaderContinue=await setup({loader:true,answers:[true]});loaderContinue.unlock();await loaderContinue.click();
assert.equal(loaderContinue.stats.backups,0);
assert.deepEqual(loaderContinue.stats.events,["resume"]);
pass("confirmed loader resume makes no backup claim");
let finishStock,finishInstall;
const stockWait=new Promise(r=>{finishStock=r;}),installWait=new Promise(r=>{finishInstall=r;});
const racing=await setup({answers:[true],stockWait,installWait});racing.unlock();
racing.el("stock-file").files=[{size:1,arrayBuffer:async()=>new Uint8Array([42]).buffer}];
const checking=racing.el("stock-file").listeners.change();
const installing=racing.click();await new Promise(r=>setImmediate(r));
assert.equal(racing.stats.writes,1);assert.equal(racing.el("go").disabled,true);
finishStock();await checking;
for(const id of ["go","stock-go","classic-go"])assert.equal(racing.el(id).disabled,true,"late stock validation must retain the active update lock: "+id);
finishInstall();await installing;
assert.equal(racing.el("go").disabled,false);
pass("late stock-file validation cannot unlock a running update");
async function prepareClassic(answer) {
  const scene=await setup({answers:[answer]});
  scene.el("classic-backup").files=[{size:1,arrayBuffer:async()=>new Uint8Array([0x50,0x4b]).buffer}];
  await scene.el("classic-backup").listeners.change();
  scene.el("classic-package").files=[{size:610074,arrayBuffer:async()=>new Uint8Array([42]).buffer}];
  await scene.el("classic-package").listeners.change();
  scene.el("classic-confirm").value="CLASSIC";scene.el("classic-confirm").listeners.input();
  assert.equal(scene.el("classic-go").disabled,false);
  return scene;
}
const classicReturn=await prepareClassic(true);await classicReturn.el("classic-go").listeners.click();
assert.deepEqual(classicReturn.stats.events,["backup","download","download","install"]);
assert.match(classicReturn.stats.confirmations[0],/BOTH.*1\.x.*Classic-restore/s);
assert.match(classicReturn.stats.downloads[1].name,/^Caravel-Classic-restore-.*\.json$/);
const derivative=JSON.parse(await classicReturn.stats.downloads[1].blob.text());
assert.equal(derivative.format,"felucca-backup");assert.equal(derivative.version,1);
assert(derivative.objects.every(o=>!('bytes' in o)&&!('file' in o)));
pass("Classic return downloads compatible JSON and confirms both backups before install");
const classicCancel=await prepareClassic(false);await classicCancel.el("classic-go").listeners.click();
assert.deepEqual(classicCancel.stats.events,["backup","download","download"]);
assert.equal(classicCancel.stats.writes,0);assert.equal(classicCancel.el("classic-go").disabled,false);
pass("Classic return cancel after both downloads sends no update and unlocks controls");
console.log("breaking behavior: "+passed+" simulated scenarios passed");
