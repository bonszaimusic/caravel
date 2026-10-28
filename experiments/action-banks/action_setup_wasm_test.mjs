// SPDX-License-Identifier: GPL-3.0-only
// Real Caravel C/WASM firmware input, not simulated JavaScript state.
import fs from 'node:fs';
import assert from 'node:assert/strict';
const {instance}=await WebAssembly.instantiate(fs.readFileSync('build/home-perf/felucca.wasm'),{});
const e=instance.exports;
e._initialize();e.web_nor_erase();e.web_boot();
function render(ms){for(let n=Math.ceil(ms*44.1);n>0;){const x=Math.min(n,128);e.web_render(x);n-=x;}}
function tap(b){e.web_buttons(1<<b);render(65);e.web_buttons(0);render(80);}
function key(k){e.web_keys(1<<k);render(80);e.web_keys(0);render(100);}
function bankHold(){e.web_keys(1<<22);render(670);e.web_keys(0);render(110);}
function bank(){return(e.web_action_state()>>8)&255;}
function setup(){return(e.web_action_state()>>1)&1;}
function val(k){return e.web_action_setup_value(k);}
render(1300);
tap(6);tap(6);
assert.equal(e.web_action_state()&1,1,'HOME ACTION enabled');
assert.equal(bank(),0);
assert.equal(setup(),0);
const before=new Uint16Array(e.memory.buffer,e.web_screen(),240*240).slice();
bankHold();
assert.equal(bank(),0,'long BANK never changes bank');
assert.equal(setup(),1,'long BANK opens latched SETUP');
render(180);
const after=new Uint16Array(e.memory.buffer,e.web_screen(),240*240);
let pixels=0;
for(let y=24+52;y<24+199;y++)for(let x=0;x<240;x++)pixels+=before[y*240+x]!==after[y*240+x];
assert.ok(pixels>300,'real LCD redraws HOME K1..K4 as SETUP cards');
assert.equal(val(0),8,'Random bars start at 8');
assert.equal(val(1),35,'Random depth starts at 35');
assert.equal(val(2),1,'Random scope starts TRACK');
assert.equal(val(3),1,'Random shape starts SMOOTH');
// Physical encoder events are handled by the actual firmware UI loop.
e.web_enc(3,1);render(110);
assert.notEqual(val(0),8,'physical K1 updates Random duration');
e.web_action_setup_knob(1,1);
assert.equal(val(1),50,'K2 depth uses enumerated presets');
e.web_action_setup_knob(2,1);
assert.equal(val(2),2,'K3 scope changes and resets capture');
e.web_action_setup_knob(3,1);
assert.equal(val(3),2,'K4 curve selects DRIFT');
key(23);
assert.equal(e.web_action_random_state(),1,'RANDOM remains accessible while SETUP open');
assert.ok(e.web_action_random_count()>0,'real firmware captures HOME cells');
key(24);assert.equal(e.web_action_random_state(),2,'FREEZE still works');
key(24);assert.equal(e.web_action_random_state(),1,'RESUME still works');
key(25);assert.equal(e.web_action_random_state(),3,'RETURN still works');
key(22);assert.equal(bank(),1,'short BANK advances to STASIS upon release');
assert.equal(setup(),1,'SETUP stays enabled across banks');
const strength=val(0);
e.web_action_setup_knob(0,1);
assert.equal(val(0),strength,'STASIS settings refuse writing unless TYPE is STASIS');
key(22);assert.equal(bank(),2,'PAIR bank');
assert.equal(val(0),-1,'PAIR view shows unpaired condition');
key(22);assert.equal(bank(),3,'LIVE bank');
const level=val(0);
e.web_action_setup_knob(0,-1);
assert.notEqual(val(0),level,'LIVE writes real Track 1 P_LEVEL, not a shadow parameter');
const beforeMute=e.web_action_mute(0);key(23);
assert.notEqual(e.web_action_mute(0),beforeMute,'LIVE action still mutes during SETUP');
key(23);assert.equal(e.web_action_mute(0),beforeMute);
bankHold();assert.equal(setup(),0,'second BANK hold closes SETUP');
assert.equal(bank(),3,'closing SETUP preserves bank');
tap(6);tap(6);
assert.equal(e.web_action_state()&1,0,'HOME double tap disables ACTION');
assert.equal(setup(),0,'disabling ACTION clears SETUP');
console.log('ACTION SETUP C/WASM PASS: 500ms BANK hold, short release, LCD cards, firmware encoder, four RANDOM controls, actions during SETUP, safe STASIS/PAIR, shared LIVE LEVEL and mute, exit.');
