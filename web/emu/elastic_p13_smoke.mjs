// SPDX-License-Identifier: GPL-3.0-only
// Actual compiled WASM callbacks, HPM1 v5 project SAVE/LOAD and UI gesture.
import fs from "node:fs";
const bytes=fs.readFileSync(process.argv[2]||"build/emu/felucca.wasm");
const {instance}=await WebAssembly.instantiate(bytes,{});
const x=instance.exports;
for(const fn of ["web_action_state","web_action_setup_knob",
   "web_action_setup_toggle","web_p12_test_select_track","web_p12_return_track_ms",
   "web_p13_bounce_track","web_p13_effective_bounce_track",
   "web_p13_reflect_track","web_p13_couple_track","web_p13_seed_track",
   "web_p13_setup_page","web_p13_test_reset_settings",
   "web_p13_test_project_save","web_p13_test_project_load"]){
 if(typeof x[fn]!=="function")throw Error("Missing WASM export: "+fn);
}
const ok=(v,msg)=>{if(!v)throw Error(msg);};
const render=(ms)=>{
 const n=Math.round(ms*44.1);
 for(let k=0;k<n;k+=128)x.web_render(Math.min(128,n-k));
};
const btn=k=>{x.web_buttons(1<<k);render(60);x.web_buttons(0);render(60);};
const key=k=>{x.web_keys(1<<k);render(50);x.web_keys(0);render(50);};
const select=t=>{x.web_p12_test_select_track(t);render(40);};
const setup=(k,n)=>{x.web_action_setup_knob(k,n);render(40);};
x._initialize();x.web_nor_erase();x.web_boot();render(1100);
btn(6);btn(6);
ok(x.web_action_state()&1,"ACTION not enabled");
for(let k=0;k<7 && (x.web_action_state()>>8&255)!==6;k++)key(22);
ok((x.web_action_state()>>8&255)===6,"ELASTIC bank missing");
select(0);
x.web_action_setup_toggle();render(30);
ok(x.web_p13_setup_page()===0,"default must open TIMES");
setup(2,30); /* RETURN 5 minutes */
key(26); /* physical ACTION A4 switches SETUP page */
ok(x.web_p13_setup_page()===1,"A4 did not open PHYSICS");
setup(0,250); /* 1..1000 with accelerated encoder */
setup(1,-20); /* REFLECT 50 */
setup(2,65); /* COUPLE 65 */
setup(3,72); /* SEED 72 */
/* P1.4: BOUNCE is a tagged full-cycle musical duration, not a legacy count. */
ok((x.web_p13_bounce_track(0)&0x8000)!==0 &&
   (x.web_p13_bounce_track(0)&15)>=1,"T1 tempo BOUNCE setting");
ok(x.web_p13_reflect_track(0)===50 &&
   x.web_p13_couple_track(0)===65 &&
   x.web_p13_seed_track(0)===72,"T1 physics settings");
key(26);
ok(x.web_p13_setup_page()===0,"A4 did not return to TIMES");
select(1);setup(2,-70); /* short return for T2 */
key(26);
setup(0,-2);setup(2,17);setup(3,9);
ok(x.web_p13_bounce_track(1)!==x.web_p13_bounce_track(0),
   "T2 BOUNCE unexpectedly inherits T1");
ok(x.web_p13_seed_track(1)===9 && x.web_p13_couple_track(1)===17,
   "T2 seed/network settings");
ok(x.web_p13_couple_track(2)===0 && x.web_p13_couple_track(3)===0,
   "T3/T4 coupling must default OFF");
const saved=[0,1,2,3].map(t=>[
 x.web_p13_bounce_track(t),x.web_p13_reflect_track(t),
 x.web_p13_couple_track(t),x.web_p13_seed_track(t),
 x.web_p12_return_track_ms(t)]);
ok(x.web_p13_test_project_save()===0,"v5 HPM1 project SAVE failed");
x.web_p13_test_reset_settings();render(30);
ok((x.web_p13_bounce_track(0)&0x8000)!==0 && x.web_p13_couple_track(0)===0,
   "test reset failed");
ok(x.web_p13_test_project_load()===1,"v5 HPM1 project LOAD failed");
for(let t=0;t<4;t++){
 const got=[
   x.web_p13_bounce_track(t),x.web_p13_reflect_track(t),
   x.web_p13_couple_track(t),x.web_p13_seed_track(t),
   x.web_p12_return_track_ms(t)];
 ok(got.every((v,i)=>v===saved[t][i]),"v5 project roundtrip T"+(t+1)+": "+got);
}
ok(x.web_p13_setup_page()===0,"LOAD must reset to page one");
console.log("elastic-p13-smoke: PASS (two-page physical A4, four track physics, exact HPM1 v5 SAVE/LOAD)");
