// SPDX-License-Identifier: GPL-3.0-only
const pairUI=id=>document.getElementById(`pair-${id}`);
let pairState=null;
function pairReady(){pairUI('controls').disabled=false;}
function pairMessage(m){
 if(m.type==='pair-state'){
  pairState=m;const member=!!m.on&&(m.sel===m.a||m.sel===m.b);
  for(const id of ['morph','motion','level','pan','mute','latch','rate1','rate2','depth1','depth2','demo'])pairUI(id).disabled=!member;
  pairUI('disable').disabled=!m.on;pairUI('enable').disabled=!!m.on;
  pairUI('select-a').disabled=pairUI('select-b').disabled=!m.on;
  pairUI('select-a').textContent=m.on?`T${m.a+1} · A auswählen`:'Track A auswählen';pairUI('select-b').textContent=m.on?`T${m.b+1} · B auswählen`:'Track B auswählen';
  pairUI('a').disabled=pairUI('b').disabled=!!m.on;
  if(m.on){pairUI('a').value=String(m.a);pairUI('b').value=String(m.b);}
  const status=m.on?`Pair T${m.a+1} ↔ T${m.b+1} · ausgewählt T${m.sel+1} · bis zu ${m.budget} ${m.budget===1?'Note':'Noten'}${m.cells===3?' · zwei CELL-Netzwerke':m.cells?' · CELL-Netzwerk + Poly-Stimmen':''} · Morph Basis ${Math.round(m.base/32767*100)} %, live ${Math.round(m.effective/32767*100)} %${member?'':` · Zum Bedienen T${m.a+1} oder T${m.b+1} auswählen.`}`:`Kein aktives Pair. Track A und B wählen, dann paaren. ${m.reason||''}`;
  if(pairUI('state').textContent!==status)pairUI('state').textContent=status;
  pairUI('morph').setAttribute('aria-valuetext',`Basis ${Math.round(m.base/32767*100)} Prozent, live ${Math.round(m.effective/32767*100)} Prozent`);
  if(document.activeElement!==pairUI('morph'))pairUI('morph').value=m.base;pairUI('motion').checked=!!m.motion;
  if(m.controls){for(const [id,index] of [['level',0],['pan',1],['latch',3],['rate1',5]])if(document.activeElement!==pairUI(id))pairUI(id).value=m.controls[index];pairUI('mute').checked=!!m.controls[2];if(document.activeElement!==pairUI('rate2'))pairUI('rate2').value=m.rate2;for(const [id,index]of [['depth1',0],['depth2',1]])if(document.activeElement!==pairUI(id))pairUI(id).value=m.depths[index];}
 }else if(m.type==='pair-result')pairUI('result').textContent=m.message;
 else if(m.type==='pair-snapshot'){const url=URL.createObjectURL(new Blob([m.bytes],{type:'application/octet-stream'})),a=document.createElement('a');a.href=url;a.download='caravel-pair-ohne-home.epm';a.click();setTimeout(()=>URL.revokeObjectURL(url),5000);}
}
const pairSend=(op,args=[])=>send({type:'pair',op,args});
pairUI('enable').onclick=()=>pairSend('enable',[+pairUI('a').value,+pairUI('b').value]);
pairUI('page').onclick=()=>homeSend('page',[4]);
pairUI('prepare').onclick=()=>{if(confirm('Neue Testsession starten? ALLE ungespeicherten Sounds, Sequenzen, HOME-Belegungen und Macros werden ersetzt.'))pairSend('prepare',[+pairUI('a').value,+pairUI('b').value,+pairUI('ea').value,+pairUI('eb').value]);};
pairUI('select-a').onclick=()=>{if(pairState?.on)pairSend('select',[pairState.a]);};
pairUI('select-b').onclick=()=>{if(pairState?.on)pairSend('select',[pairState.b]);};
pairUI('panic').onclick=()=>pairSend('panic');pairUI('disable').onclick=()=>pairSend('off');
pairUI('demo').onclick=()=>{if(confirm('Sequenz von Track A durch die Demo ersetzen?'))pairSend('sequence');};
pairUI('save').onclick=()=>pairSend('save');
pairUI('morph').oninput=e=>pairSend('morph',[pairState?.sel??0,+e.target.value]);
pairUI('motion').onchange=e=>pairSend('motion',[+e.target.checked]);
for(const[id,control]of [['level',0],['pan',1],['mute',2],['latch',3],['rate1',5]])pairUI(id).oninput=e=>pairSend('control',[pairState?.sel??0,control,id==='mute'?+e.target.checked:+e.target.value]);
pairUI('rate2').oninput=e=>pairSend('lfo2_rate',[pairState?.sel??0,+e.target.value]);
for(const[id,slot,source]of [['depth1',0,1],['depth2',1,11]])pairUI(id).oninput=e=>pairSend('lfo',[pairState?.sel??0,slot,source,+e.target.value]);
pairUI('load').onchange=async e=>{const file=e.target.files[0];if(!file)return;try{if(file.size!==4032){pairUI('result').textContent='Keine passende .epm-Datei.';return;}if(!confirm('Alte Pair-Datei importieren? Der aktuelle Arbeitsstand wird ersetzt; HOME und Macros sind in diesem Format nicht enthalten.'))return;send({type:'pair',op:'restore',bytes:new Uint8Array(await file.arrayBuffer())});}catch{pairUI('result').textContent='Datei konnte nicht gelesen werden.';}finally{e.target.value='';}};
