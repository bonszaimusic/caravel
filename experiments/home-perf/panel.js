// SPDX-License-Identifier: GPL-3.0-only
const homeEl=id=>document.getElementById('home-'+id);
let homeState={row:0,count:1,track:0},homePending=0;
const homeTemplate=['EMPTY','ENGINE','PERFORM'];
const homeSlot=()=>+homeEl('slot').value;
function homeReady(){homeEl('controls').disabled=false;homeSlotLabels();}
function homeSlotLabels(){const s=String.fromCharCode(65+homeSlot());homeEl('save').textContent=`Projekt ${s} speichern`;homeEl('load').textContent=`Projekt ${s} laden …`;homeEl('export').textContent=`Gespeichertes ${s} herunterladen`;}
function homeMessage(m){
 if(m.type==='home-state'){
  homeState=m;homeEl('row').textContent=`${m.row+1} / ${m.count}`;homeEl('track').value=String(m.track);
  homeEl('prev').disabled=m.row===0;homeEl('next').disabled=m.row+1>=m.count;
  homeEl('default-status').textContent=`Startvorlage: ${homeTemplate[m.defaultSet]}`;
  const pending=m.pending||0;homeEl('pending').textContent=pending?`Noch im RAM: ${[0,1,2,3].filter(s=>pending&(1<<s)).map(s=>String.fromCharCode(65+s)).join(', ')}. STOP speichert dauerhaft.`:'';
  if(homePending&&!pending)homeEl('result').textContent='Nach STOP: Projekte dauerhaft im Emulator abgelegt. Wichtige Stände zusätzlich herunterladen.';
  homePending=pending;return;
 }
 if(m.type==='home-project'){const blob=new Blob([m.bytes],{type:'application/octet-stream'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=`caravel-home-${String.fromCharCode(65+m.slot)}.hpm`;a.click();setTimeout(()=>URL.revokeObjectURL(url),5000);homeEl('result').textContent=`Download des gespeicherten Projekts ${String.fromCharCode(65+m.slot)} angefordert.`;return;}
 if(m.type==='home-result')homeEl('result').textContent=m.message;
}
const homeSend=(op,args=[])=>send({type:'home',op,args});
document.querySelectorAll('[data-home-page]').forEach(b=>b.onclick=()=>homeSend('page',[+b.dataset.homePage]));
homeEl('add').onclick=()=>homeSend('add');
homeEl('track').onchange=()=>homeSend('select',[+homeEl('track').value]);
homeEl('prev').onclick=()=>homeSend('row',[Math.max(0,homeState.row-1)]);
homeEl('next').onclick=()=>homeSend('row',[Math.min(homeState.count-1,homeState.row+1)]);
homeEl('default').onclick=()=>homeSend('default',[+homeEl('template').value]);
homeEl('apply').onclick=()=>{if(confirm(`HOME-Belegung von T${homeState.track+1} durch ${homeTemplate[+homeEl('template').value]} ersetzen? Sounds und Macros bleiben erhalten. HOME EDIT → UNDO macht die Änderung rückgängig.`))homeSend('apply',[homeState.track,+homeEl('template').value]);};
homeEl('slot').onchange=homeSlotLabels;
homeEl('save').onclick=()=>homeSend('save',[homeSlot()]);
homeEl('load').onclick=()=>{if(confirm(`Projekt ${String.fromCharCode(65+homeSlot())} für ALLE Tracks laden und Transport stoppen? Ungespeicherte Änderungen werden ersetzt.`))homeSend('load',[homeSlot()]);};
homeEl('export').onclick=()=>homeSend('export',[homeSlot()]);
homeEl('import').onchange=async e=>{const file=e.target.files[0];if(!file)return;try{if(file.size!==10000){homeEl('result').textContent='Keine passende .hpm-Projektdatei (10000 Byte).';return;}if(!confirm('Projekt für ALLE Tracks importieren und Transport stoppen? Ungespeicherte Änderungen werden ersetzt.'))return;const bytes=new Uint8Array(await file.arrayBuffer());send({type:'home',op:'import',bytes});}catch{homeEl('result').textContent='Datei konnte nicht gelesen werden.';}finally{e.target.value='';}};
