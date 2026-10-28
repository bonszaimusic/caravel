// SPDX-License-Identifier: GPL-3.0-only
// Physical FM-1 A3/A4 gesture integration against the compiled C/WASM engine,
// renderer and the real shared track parameter store.
import fs from 'node:fs';
import assert from 'node:assert/strict';
const code=fs.readFileSync('build/home-perf/felucca.wasm');
const {instance}=await WebAssembly.instantiate(code,{});
const e=instance.exports;
e._initialize();e.web_nor_erase();e.web_boot();
const render=ms=>{for(let n=Math.ceil(ms*44.1);n>0;){let x=Math.min(128,n);e.web_render(x);n-=x;}};
const tap=b=>{e.web_buttons(1<<b);render(65);e.web_buttons(0);render(90);};
const key=(k,duration=90)=>{e.web_keys(1<<k);render(duration);e.web_keys(0);render(130);};
const hold=k=>key(k,680);
const slot=()=>e.web_action_snapshot_slot();
const count=s=>e.web_action_snapshot_count(s);
const level=()=>e.web_pair_control_get(0,0);
const setLevel=n=>assert.equal(e.web_pair_control(0,0,n),1);
const frame=()=>new Uint16Array(e.memory.buffer,e.web_screen(),240*240).slice();
render(1300);tap(6);tap(6);
assert.equal(e.web_action_state()&1,1,'ACTION enabled via physical HOME double-tap');
e.web_home_apply(0,2); // track 1 second HOME row contains LEVEL / FILT / DLY / REV
assert.equal(e.web_home_get(0,0,0,0),2);
assert.equal(e.web_home_get(3,0,1,0),1,'HOME row 2 controls an actual track parameter');
assert.equal(slot(),0,'selected snapshot S1 initially');
assert.equal(count(0),0,'S1 is empty initially');
assert.equal(count(1),0,'S2 is empty initially');
const emptyScreen=frame();
// Set fast return duration using BANK long → SETUP → physical K1 encoder
hold(22);
assert.equal((e.web_action_state()>>1)&1,1,'BANK HOLD enters SETUP');
e.web_enc(3,-5);render(80);
assert.equal(e.web_action_setup_value(0),1,'K1 changes duration to one bar');
hold(22);
assert.equal((e.web_action_state()>>1)&1,0,'BANK HOLD exits SETUP');
// A4 long saves S1 at current 90 without moving it or changing slot.
setLevel(90);render(120);
hold(26);
assert.equal(slot(),0,'A4 LONG does not advance slot');
assert.ok(count(0)>0,'S1 saved nonempty HOME snapshot');
assert.equal(level(),90,'A4 capture does not alter live sound');
assert.equal(e.web_action_random_state(),0,'A4 capture itself does not start Random');
const screenSaved=frame();
let changed=0;
for(let y=24+171;y<24+200;y++)
 for(let x=0;x<240;x++)changed+=emptyScreen[y*240+x]!==screenSaved[y*240+x];
assert.ok(changed>40,'real FM-1 ACTION strip changes for filled S1');
// A4 short changes slot ONLY; storing S2 does not replace S1.
key(26);assert.equal(slot(),1,'A4 tap cycles to S2');
assert.equal(count(1),0,'A4 short does not save a snapshot');
setLevel(40);render(120);
hold(26);
assert.ok(count(1)>0,'S2 captured independently');
assert.equal(level(),40);
const n1=count(0),n2=count(1);
assert.ok(n1>0&&n2>0);
key(26);assert.equal(slot(),2,'S3 selected');
assert.equal(count(2),0);
key(25);assert.equal(e.web_action_random_state(),0,'recall of empty slot does not start morph');
key(26);assert.equal(slot(),3,'S4 selected');
key(26);assert.equal(slot(),0,'A4 wraps to S1');
assert.equal(count(0),n1,'S1 intact');
// Recall S1 from arbitrary live level. The audio DSP continues rendering.
setLevel(10);render(100);
key(25);
assert.equal(e.web_action_random_state(),5,'A3 recalls saved snapshot with a real morph');
render(450);
assert.ok(level()>10&&level()<90,'morph interpolates, not jumps');
key(24);assert.equal(e.web_action_random_state(),2,'A2 freezes snapshot recall');
const frozen=level();render(550);
assert.equal(level(),frozen,'frozen snapshot stays fixed');
key(24);assert.equal(e.web_action_random_state(),5,'A2 resumes snapshot recall');
render(4300);
assert.equal(level(),90,'S1 recall reaches exact saved value');
assert.equal(e.web_action_random_state(),0,'completed recall goes idle');
// Save S2, reuse same four bytes of shared engine parameter, not shadow state.
key(26);assert.equal(slot(),1);
key(25);assert.equal(e.web_action_random_state(),5,'S2 recall starts from current S1');
render(4300);
assert.equal(level(),40,'S2 value stays independent of S1');
assert.equal(count(1),n2);
// Overwrite S2 from current live 75; S1 stays immutable.
setLevel(75);render(120);
hold(26);
assert.equal(count(1),n2,'rewriting slot preserves original ref count');
setLevel(15);render(120);
key(25);render(4300);
assert.equal(level(),75,'overwritten S2 value is recalled');
key(26);key(26);key(26);assert.equal(slot(),0);
key(25);render(4300);
assert.equal(level(),90,'S1 still holds original 90 after S2 rewrite');
// Existing RANDOM/RETURN workflow still works without an explicit REC destination.
key(23);
assert.equal(e.web_action_random_state(),1,'A1 still starts real RANDOM morph');
key(24);assert.equal(e.web_action_random_state(),2);
key(24);assert.equal(e.web_action_random_state(),1);
// A4 long during live RANDOM stores the audible intermediate and cancels
// only the morph trajectory; it never resets any sound parameter.
render(250);
const intermediate=level();
hold(26);
assert.equal(e.web_action_random_state(),0,'A4 capture stops moving morph');
assert.equal(level(),intermediate,'A4 capture retains live level at capture time');
// More than one snapshot still exists; no new UI pages, no extra notes.
assert.equal(count(0),n1);
assert.ok(count(1)>0);
assert.equal(e.web_action_key_note(26),255,'A4 is consumed, not sounded');
tap(6);tap(6);
assert.equal(e.web_action_state()&1,0,'ACTION OFF restores keyboard');
assert.notEqual((()=>{e.web_keys(1<<26);render(90);let n=e.web_action_key_note(26);e.web_keys(0);render(100);return n;})(),255,'ordinary high note returned');
// A genuine new device session has empty snapshots: no project schema change.
const fresh=(await WebAssembly.instantiate(code,{})).instance.exports;
fresh._initialize();fresh.web_nor_erase();fresh.web_boot();
assert.equal(fresh.web_action_snapshot_count(0),0,'snapshots are intentionally volatile');
assert.equal(fresh.web_action_snapshot_count(1),0);
console.log('SNAPSHOTS C/WASM PASS: four slots, A4 tap/hold, real LCD, deep HOME capture, exact bidirectional 1-bar morph, freeze/resume, empty-slot safe, overwrite isolated, Random retained, A4 audio-safe, session volatile.');
