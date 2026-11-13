// SPDX-License-Identifier: GPL-3.0-only
// P1.2 opt-in WASM smoke: real HOME/ACTION navigation, display and note audio.
// Requires a fresh wasm with CARAVEL_ELASTIC_P1=1 and CARAVEL_ELASTIC_PLAYABLE=1.
import fs from "node:fs";
const bytes=fs.readFileSync(process.argv[2]||"build/emu/felucca.wasm");
const {instance}=await WebAssembly.instantiate(bytes,{});
const ex=instance.exports,mem=ex.memory;
if(typeof ex.web_action_state!=="function")throw Error("ACTION exports missing");
if(typeof ex.web_p12_armed_mask!=="function" ||
   typeof ex.web_p12_coil_shift!=="function" ||
   typeof ex.web_p12_demo_track!=="function" ||
   typeof ex.web_p12_return_ms!=="function" ||
   typeof ex.web_p12_pull_ms!=="function")
   throw Error("P1.2 instrumented WASM exports missing");
if(ex.web_p12_pull_ms(100)!==45000 ||
   ex.web_p12_return_ms(90)!==45000 ||
   ex.web_p12_return_ms(94)!==90000 ||
   ex.web_p12_return_ms(100)!==300000)
   throw Error("PULL/RETURN durations are not the requested 45s/5m maps");
ex._initialize();ex.web_nor_erase();ex.web_boot();
const render=(ms)=>{
 const n=Math.round(ms*44.1);let peak=0;
 for(let k=0;k<n;k+=128){
  const m=Math.min(128,n-k);ex.web_render(m);
  const l=new Float32Array(mem.buffer,ex.web_out_l(),m);
  for(const v of l)peak=Math.max(peak,Math.abs(v));
 }
 return peak;
};
const btn=(b)=>{ex.web_buttons(1<<b);render(60);ex.web_buttons(0);render(60);};
const key=(k)=>{ex.web_keys(1<<k);render(55);ex.web_keys(0);render(55);};
const bank=()=>ex.web_action_state()>>8&255;
const fb=()=>new Uint16Array(mem.buffer,ex.web_screen(),240*240).slice();
const changedRegion=(a,b,x,y,w,h)=>{
 let n=0;
 for(let j=y;j<y+h;j++)for(let i=x;i<x+w;i++)
  if(a[j*240+i]!==b[j*240+i])n++;
 return n;
};
render(1100);
btn(6);btn(6); // HOME twice enables ACTION
if(!(ex.web_action_state()&1))throw Error("ACTION did not open");
for(let i=0;i<7 && bank()!==6;i++)key(22); // BANK NEXT
if(bank()!==6)throw Error("ELASTIC bank not reachable, bank="+bank());
/* A change of engine alone leaves old HOME bindings behind. The explicit
 * test-only helper provides the actual four DRONE timbre cells. */
ex.web_p12_demo_track(0);render(100);
ex.web_action_setup_toggle();render(30);
ex.web_action_setup_knob(0,1);render(30); // SETUP K1: CELL -> ROW
ex.web_action_setup_toggle();render(30);
const before=fb();
key(21); // A1 ARM visible row
const armed=fb();
if(ex.web_p12_armed_mask()!==1)throw Error("ELASTIC never armed T1");
if(!changedRegion(before,armed,182,26,12,12))
 throw Error("track-1 ARM badge failed to draw");
if(!changedRegion(before,armed,3,42,56,52))
 throw Error("active HOME cell failed to show ARMED spine/dot");
ex.web_keys(1<<7);render(130); // held live C note
const toneBefore=render(100);
ex.web_enc(3,5);render(100);  // K1 raw touch
ex.web_enc(4,-4);render(100); // K2 independent touch
const active=fb();
const shift1=ex.web_p12_coil_shift(0,0);
const shift2=ex.web_p12_coil_shift(0,1);
if(!shift1 || !shift2)throw Error("both cell springs must visibly move");
if(!changedRegion(armed,active,3,42,56,52) ||
   !changedRegion(armed,active,62,42,56,52))
 throw Error("PULL/RETURN mini-coil is not updating both HOME cells");
const peak=Math.max(toneBefore,render(300));
if(!(peak>0.001))throw Error("no audible waveform from held DRONE");
ex.web_keys(0);render(150);
key(24); // A3 HOLD
key(26); // A4 RESET
key(21); // A1 OFF
if(ex.web_p12_armed_mask()!==0)throw Error("A1 OFF left an arm active");
const rgb=Buffer.alloc(240*240*3);
for(let i=0;i<active.length;i++){
 const c=active[i];rgb[3*i]=((c>>11)&31)*255/31|0;
 rgb[3*i+1]=((c>>5)&63)*255/63|0;
 rgb[3*i+2]=(c&31)*255/31|0;
}
fs.mkdirSync("build/emu",{recursive:true});
fs.writeFileSync("build/emu/elastic-p12-screen.ppm",
 Buffer.concat([Buffer.from("P6\n240 240\n255\n"),rgb]));
console.log("elastic-p12-wasm-smoke: PASS (verified T1 ARM mask, visible badges, 2 moving cell coils, audio, RESET/OFF)");
