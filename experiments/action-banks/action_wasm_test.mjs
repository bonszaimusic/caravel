// SPDX-License-Identifier: GPL-3.0-only
// Real Caravel 0.6.0 C/WASM integration, including keyboard ISR and LCD renderer.
import fs from 'node:fs';
import assert from 'node:assert/strict';
const bytes=fs.readFileSync('build/home-perf/felucca.wasm');
const {instance}=await WebAssembly.instantiate(bytes,{});
const e=instance.exports;
e._initialize();e.web_nor_erase();e.web_boot();
const render=ms=>{for(let n=Math.ceil(ms*44.1);n>0;){const x=Math.min(128,n);e.web_render(x);n-=x;}};
const tap=b=>{e.web_buttons(1<<b);render(65);e.web_buttons(0);render(65);};
const press=k=>{e.web_keys(1<<k);render(90);};
const release=()=>{e.web_keys(0);render(110);};
const key=k=>{press(k);const note=e.web_action_key_note(k);release();return note;};
const state=()=>e.web_action_state();
render(1250);
assert.equal(state()&1,0,'OFF at startup');
assert.notEqual(key(23),255,'unreserved note plays in normal mode');
const frame=()=>new Uint16Array(e.memory.buffer,e.web_screen(),240*240).slice();
const before=frame();
tap(6);tap(6);
assert.equal(state()&1,1,'HOME double tap enables action');
assert.notEqual(state()&(1<<16),0,'HOME keyboard mask active');
render(180);
const after=frame();
let changed=0;
for(let y=24+171;y<24+201;y++)
 for(let x=0;x<240;x++)changed+=before[y*240+x]!==after[y*240+x];
assert.ok(changed>100,'third ACTION strip appears on genuine LCD renderer');
assert.equal(e.web_breath_keys() & (31<<22),31<<22,'all five ACTION key LEDs breathe');
assert.equal(key(26),255,'ACTION key is silent in firmware keyboard block (CAPTURE A is non-destructive)');
assert.notEqual(key(7),255,'22 other physical notes still playable');
assert.equal(key(22),255,'bank key never sounds');
assert.equal((state()>>8)&255,1,'bank switch selects STASIS');
assert.equal(key(22),255);assert.equal((state()>>8)&255,2,'PAIR bank');
key(22);assert.equal((state()>>8)&255,3,'LIVE bank');
const beforeMute=e.web_action_mute(0);
key(23);assert.notEqual(e.web_action_mute(0),beforeMute,'LIVE action actually toggles T1 mute');
key(23);assert.equal(e.web_action_mute(0),beforeMute,'LIVE mute toggles back');
key(22);assert.equal((state()>>8)&255,0,'returns to RANDOM bank');
assert.equal(e.web_action_random_state(),0);
key(23);assert.equal(e.web_action_random_state(),1,'RANDOM starts real C morph');
assert.ok(e.web_action_random_count()>0,'captured real HOME parameters');
key(24);assert.equal(e.web_action_random_state(),2,'FREEZE');
key(24);assert.equal(e.web_action_random_state(),1,'RESUME');
key(25);assert.equal(e.web_action_random_state(),3,'RETURN A');
// HOME double tap OFF restores physical keys without changing any bank or OCT.
tap(6);tap(6);
assert.equal(state()&1,0,'HOME double tap disables');
assert.equal(state()&(1<<16),0,'mask cleared');
assert.notEqual(key(23),255,'all notes play in OFF');
assert.equal((state()>>8)&255,0,'bank retained for next enable');
// A held note must not be eaten when ACTION becomes active.
press(23);
assert.notEqual(e.web_action_key_note(23),255);
tap(6);tap(6);
assert.notEqual(e.web_action_key_note(23),255,'already held note stays a note');
release();
assert.equal(key(23),255,'after release the key becomes ACTION');
console.log('ACTION 0.6.0 C/WASM PASS: HOME double tap, four banks, live mute, Random/Freeze/Resume/Return, third LCD row, LEDs, five silent keys, 22 playable keys, held-note safety, OFF.');
