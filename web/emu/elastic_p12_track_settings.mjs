// SPDX-License-Identifier: GPL-3.0-only
// P1.2 per-track session SETUP: real WASM UI controls, no sound patch writes.
import fs from "node:fs";
const bytes=fs.readFileSync(process.argv[2]||"build/emu/felucca.wasm");
const {instance}=await WebAssembly.instantiate(bytes,{});
const ex=instance.exports;
for(const name of ["web_action_state","web_action_setup_toggle",
                   "web_action_setup_knob","web_p12_test_select_track",
                   "web_p12_scope_track","web_p12_pull_track_ms",
                   "web_p12_return_track_ms","web_p12_range_track",
                   "web_p12_armed_mask","web_p12_demo_track"]) {
 if(typeof ex[name]!=="function")throw Error("Missing WASM export "+name);
}
ex._initialize();ex.web_nor_erase();ex.web_boot();
const render=(ms)=>{
 const n=Math.round(ms*44.1);
 for(let k=0;k<n;k+=128)ex.web_render(Math.min(128,n-k));
};
const btn=k=>{ex.web_buttons(1<<k);render(60);ex.web_buttons(0);render(60);};
const key=k=>{ex.web_keys(1<<k);render(50);ex.web_keys(0);render(50);};
const select=t=>{ex.web_p12_test_select_track(t);render(30);};
const edit=(k,n)=>{ex.web_action_setup_knob(k,n);render(30);};
const get=t=>[ex.web_p12_scope_track(t),ex.web_p12_pull_track_ms(t),
              ex.web_p12_return_track_ms(t),ex.web_p12_range_track(t)];
const ok=(v,msg)=>{if(!v)throw Error(msg);};
render(1100);
btn(6);btn(6);
ok(ex.web_action_state()&1,"ACTION did not open");
for(let k=0;k<7 && (ex.web_action_state()>>8&255)!==6;k++)key(22);
ok((ex.web_action_state()>>8&255)===6,"ELASTIC bank not reachable");
for(let t=0;t<4;t++)ex.web_p12_demo_track(t);
render(100);
const defaults=get(0);
for(let t=1;t<4;t++)ok(get(t).every((v,i)=>v===defaults[i]),"nonidentical boot defaults");
select(0);
ex.web_action_setup_toggle();render(30);
edit(0,1);edit(1,15);edit(2,20);edit(3,10);
const t0=get(0);
ok(t0[0]===1 && t0[3]>defaults[3],"T1 ROW/range setup failed");
ok(t0[1]!==defaults[1] && t0[2]===45000,"T1 independent PULL/RETURN failed");
select(1);
ok(get(1).every((v,i)=>v===defaults[i]),"T2 did not keep own initial settings");
edit(1,70);edit(2,30);edit(3,20);
const t1=get(1);
ok(t1[0]===0 && t1[1]===45000 && t1[2]===300000,
   "T2 CELL/45s PULL/5m RETURN setup failed");
for(const t of [0,2,3]){
 const wanted=t===0?t0:defaults;
 ok(get(t).every((v,i)=>v===wanted[i]),"settings leaked to track "+(t+1));
}
ex.web_action_setup_toggle();render(30);
select(0);key(21); // T1 ROW ARM
ok(ex.web_p12_armed_mask()===1,"T1 ROW arm failed");
select(1);key(21); // T2 CELL ARM pick
ok(ex.web_p12_armed_mask()===3,"T2 CELL arm suppressed T1 ROW");
ex.web_action_setup_toggle();render(30);
edit(0,1); // Change T2 to ROW; should disarm ONLY T2.
ok(ex.web_p12_armed_mask()===1,"T2 SCOPE change disarmed other track");
ok(ex.web_p12_scope_track(0)===1 && ex.web_p12_scope_track(1)===1,
   "per-track scope mismatch after T2 change");
ok(ex.web_p12_return_track_ms(1)===300000 &&
   ex.web_p12_return_track_ms(0)===45000,"time settings lost on SCOPE change");
ex.web_action_setup_toggle();render(30);
select(0);key(21); // OFF T1
ok(ex.web_p12_armed_mask()===0,"T1 OFF failed");
ok(get(0).every((v,i)=>v===t0[i]),"T1 session setup not retained after OFF");
ok(get(1)[2]===300000,"T2 session setup not retained");
console.log("elastic-p12-track-settings-wasm: PASS (four independent setups, mixed ROW/CELL, ARMED isolation, session retention)");
