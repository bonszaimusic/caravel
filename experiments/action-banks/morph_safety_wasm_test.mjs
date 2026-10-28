// SPDX-License-Identifier: GPL-3.0-only
// Hardware/C/WASM regression: LEVEL never random, F_CUTOFF 150..6000 Hz,
// a real K2 turn releases just that control through FREEZE/RETURN,
// and the new native FX > GRAIN accesses the SAME STASIS engine values.
import fs from 'node:fs';
import assert from 'node:assert/strict';
const {instance}=await WebAssembly.instantiate(fs.readFileSync('build/home-perf/felucca.wasm'),{});
const e=instance.exports;
e._initialize();e.web_nor_erase();e.web_boot();
const render=ms=>{for(let n=Math.ceil(ms*44.1);n>0;){let x=Math.min(128,n);e.web_render(x);n-=x;}};
const tap=b=>{e.web_buttons(1<<b);render(65);e.web_buttons(0);render(100);};
const key=(k,ms=90)=>{e.web_keys(1<<k);render(ms);e.web_keys(0);render(120);};
const hold=k=>key(k,680);
const cutoffHz=i=>e.web_action_cutoff_hz(i);
const cells=()=>Array.from({length:e.web_action_random_count()},(_,i)=>({
 kind:e.web_action_random_cell(i,0),id:e.web_action_random_cell(i,1),
 track:e.web_action_random_cell(i,2),aux:e.web_action_random_cell(i,6),
 target:e.web_action_random_cell(i,4),released:e.web_action_random_cell(i,5),i
}));
render(1350);
let cut=e.web_action_find_cutoff(0);
if(cut<0){
 assert.equal(e.web_pair_prepare(0,1,0,0),1,'enable known ANALOG tracks for cutoff test');
 e.web_pair_off();cut=e.web_action_find_cutoff(0);
}
assert.ok(cut>=0,'must find a F_CUTOFF engine control in a real synth');
let initial=64;
for(let i=1;i<127;i++)if(cutoffHz(i)>=950&&cutoffHz(i)<=1250){initial=i;break;}
assert.ok(cutoffHz(initial)>=150&&cutoffHz(initial)<=6000,'test begins in permitted cutoff range');
assert.equal(e.web_pair_param(0,cut,initial),1);
assert.equal(e.web_pair_control(0,0,90),1,'explicit track LEVEL');
e.web_home_apply(0,2);
assert.equal(e.web_home_assign(0,0,0,1,0,0),1,'LEVEL is a real HOME cell');
assert.equal(e.web_home_assign(0,0,1,1,0,cut),1,'F_CUTOFF is a real HOME cell');
e.web_home_row(0);
tap(6);tap(6);assert.equal(e.web_action_state()&1,1,'physical ACTION ON');
key(23);assert.equal(e.web_action_random_state(),1,'real Random starts');
let targets=cells();
assert.ok(targets.length>0,'captured real HOME controls');
assert.ok(!targets.some(c=>c.kind==1 && c.aux==0 && c.id==0),
 'P_LEVEL must not be a Random destination');
const cutoffCell=()=>cells().find(c=>c.kind==1 && c.track==0 &&
 c.aux==1 && c.id==cut-e.web_action_engine_base());
assert.ok(cutoffCell(),'engine cutoff eligible for Random');
for(let i=0;i<24;i++){
 key(23);
 let c=cutoffCell();
 assert.ok(c && !c.released,'cutoff retains Random eligibility within allowed band');
 const hz=cutoffHz(c.target);
 assert.ok(hz>=150&&hz<=6000,'Random cutoff target '+hz+'Hz always 150..6000');
}
render(1100);
assert.equal(e.web_pair_control_get(0,0),90,'track LEVEL remains unchanged after Random');
e.web_enc(4,6);render(130); // physical K2, the assigned HOME cutoff
let c=cutoffCell();assert.equal(c.released,1,'manual K2 turn takes ownership of this cutoff');
const manual=e.web_pair_param_get(0,cut);
render(3500);
assert.equal(e.web_pair_param_get(0,cut),manual,'morph does not rewrite touched cutoff');
key(24);assert.equal(e.web_action_random_state(),2,'FREEZE still works');
key(24);assert.equal(e.web_action_random_state(),1,'RESUME still works');
render(550);
assert.equal(e.web_pair_param_get(0,cut),manual,'RESUME does not reclaim manual cutoff');
key(25);assert.equal(e.web_action_random_state(),3,'RETURN still works');
render(1800);
assert.equal(e.web_pair_param_get(0,cut),manual,'RETURN does not reclaim manual cutoff');
key(23);assert.equal(e.web_action_random_state(),1,'new explicit RANDOM restarts trajectory');
assert.equal(cutoffCell().released,0,'new RANDOM can reacquire manual cutoff');
// STASIS GRAIN must be discoverable in the actual native FX page table.
assert.ok(e.web_stasis_grain_page()<1000,'native FX > GRAIN page is registered');
assert.equal(e.web_stasis_grain_visible(),0,'GRAIN hidden while TYPE is HALL');
e.web_stasis_test_type(3);render(120);
assert.equal(e.web_stasis_grain_visible(),1,'GRAIN visible when TYPE=STASIS');
e.web_stasis_grain_open();render(110);
e.web_enc(3,1);render(140);
assert.equal(e.web_stasis_granular_get(0),2,'native FX > GRAIN K1 writes real grain length');
e.web_enc(4,11);render(140);
assert.equal(e.web_stasis_granular_get(1),11,'native FX > GRAIN K2 writes real scan');
e.web_home_open(0);render(100);
key(22);hold(22);
assert.equal((e.web_action_state()>>8)&255,1,'STASIS ACTION bank');
assert.equal((e.web_action_state()>>1)&1,1,'BANK HOLD opens setup');
assert.equal(e.web_stasis_granular_get(14),1,'BANK HOLD opens GRAIN directly, no SELECT');
assert.equal(e.web_action_setup_value(0),2,'ACTION reads same grain size as native FX page');
assert.equal(e.web_action_setup_value(1),11,'ACTION reads same scan value as native FX page');
console.log('MORPH SAFETY C/WASM PASS: LEVEL untouched, cutoff 150..6000Hz for 24 starts, physical K2 has priority through FREEZE/RETURN, next RANDOM reclaims, native FX GRAIN and direct ACTION use canonical state.');
