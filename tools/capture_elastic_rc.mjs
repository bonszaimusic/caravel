// SPDX-License-Identifier: GPL-3.0-only
// Real compiled firmware framebuffers, never illustrations or device photos.
import fs from 'node:fs';
import crypto from 'node:crypto';
const wasmPath=process.argv[2]||'build/emu/felucca.wasm';
const out=process.argv[3]||'build/elastic-rc-captures';
const bytes=fs.readFileSync(wasmPath);
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
fs.mkdirSync(out,{recursive:true});
const captures=[];
async function scene(name,configure){
  const {instance}=await WebAssembly.instantiate(bytes,{}),x=instance.exports;
  const render=ms=>{const n=Math.round(ms*44.1);for(let i=0;i<n;i+=128)x.web_render(Math.min(128,n-i));};
  const button=b=>{x.web_buttons(1<<b);render(60);x.web_buttons(0);render(60);};
  const key=k=>{x.web_keys(1<<k);render(55);x.web_keys(0);render(55);};
  const setup=()=>{x.web_action_setup_toggle();render(40);};
  const knob=(k,n)=>{x.web_action_setup_knob(k,n);render(40);};
  const select=t=>{x.web_p12_test_select_track(t);render(40);};
  x._initialize();x.web_nor_erase();x.web_boot();render(1100);
  button(6);button(6);
  for(let i=0;i<7&&(x.web_action_state()>>8&255)!==6;i++)key(22);
  if((x.web_action_state()>>8&255)!==6)throw Error('ELASTIC missing');
  for(let t=0;t<4;t++)x.web_p12_demo_track(t);
  select(0);render(100);
  configure({x,render,key,setup,knob,select});
  render(100);
  const fb=new Uint16Array(x.memory.buffer,x.web_screen(),240*240),rgb=Buffer.alloc(240*240*3);
  for(let i=0;i<fb.length;i++){
    const c=((fb[i]>>8)|(fb[i]<<8))&65535; // panel big-endian RGB565
    rgb[i*3]=((c>>11)&31)*255/31|0;
    rgb[i*3+1]=((c>>5)&63)*255/63|0;
    rgb[i*3+2]=(c&31)*255/31|0;
  }
  const ppm=Buffer.concat([Buffer.from('P6\n240 240\n255\n'),rgb]);
  fs.writeFileSync(`${out}/${name}.ppm`,ppm);
  captures.push({name,ppm_sha256:hash(ppm)});
}
await scene('elastic-bank-idle',()=>{});
await scene('elastic-times-cell',a=>a.setup());
await scene('elastic-physics-off',a=>{a.setup();a.key(26);});
await scene('elastic-physics-seed42',a=>{a.setup();a.key(26);a.knob(2,50);a.knob(3,42);});
await scene('elastic-return-long',a=>{a.setup();a.knob(2,120);});
await scene('elastic-cell-armed',a=>{a.key(21);a.x.web_enc(6,1);a.render(80);});
await scene('elastic-row-four-armed',a=>{a.setup();a.knob(0,1);a.setup();a.key(21);});
await scene('elastic-row-coupled',a=>{
  a.setup();a.knob(0,1);a.key(26);a.knob(2,50);a.knob(3,42);a.setup();
  a.key(21);a.x.web_enc(6,5);a.render(100);
});
await scene('elastic-four-track-hud',a=>{
  for(let t=0;t<4;t++){a.select(t);a.setup();a.knob(0,1);a.setup();a.key(21);}
  a.select(0);
});
await scene('elastic-setup-t2',a=>{
  a.select(1);a.setup();a.key(26);a.knob(0,-20);a.knob(2,17);a.knob(3,9);
});
const sources={};
for(const file of ['firmware/src/felucca.c','web/emu/felucca_web.c',
  'experiments/home-perf/home_elastic_p1.h','experiments/home-perf/home_elastic_trajectory.h'])
  sources[file]=hash(fs.readFileSync(file));
fs.writeFileSync(`${out}/manifest.json`,JSON.stringify({version:'0.13.0-rc1',
  kind:'actual WASM firmware renderer, not FM-1 hardware',wasm_sha256:hash(bytes),sources,captures},null,2)+'\n');
console.log(`Captured ${captures.length} real 240x240 RC firmware frames; WASM ${hash(bytes)}`);
