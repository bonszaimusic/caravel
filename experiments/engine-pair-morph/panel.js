// SPDX-License-Identifier: GPL-3.0-only
// Appended inside the existing emulator page's module: send() stays the single worklet transport.
const pairUI = (id) => document.getElementById(`pair-${id}`);
let pairState = null;
function pairReady() { pairUI("controls").disabled = false; }
function pairMessage(m) {
  if (m.type === "pair-state") {
    pairState = m;
    const member=m.on && (m.sel===m.a || m.sel===m.b);
    pairUI("morph").disabled=pairUI("motion").disabled=!member;
    pairUI("morph").setAttribute("aria-valuetext",`Basis ${Math.round(m.base/32767*100)} Prozent, live ${Math.round(m.effective/32767*100)} Prozent`);
    pairUI("state").textContent = m.on
      ? `T${m.a+1} + T${m.b+1} · editing T${m.sel+1} · base ${(m.base/32767*100).toFixed(1)}% · effective ${(m.effective/32767*100).toFixed(1)}% · voices ${m.gates[m.a]} / ${m.gates[m.b]} · ${m.budget} logical note${m.budget===1?"":"s"}`
      : `Pair disabled: ${m.reason ?? "PAIR OFF"}. Voice-Modus wird übernommen. AUTO / SPREAD OFF; DRUM und SLICE brauchen eine eigene Notenregelung.`;
    if (document.activeElement !== pairUI("morph")) pairUI("morph").value = m.base;
    pairUI("motion").checked = !!m.motion;
    if (m.controls) {
      for (const [id, index] of [["level",0],["pan",1],["latch",3],["rate1",5]])
        if (document.activeElement !== pairUI(id)) pairUI(id).value = m.controls[index];
      pairUI("mute").checked = !!m.controls[2];
      if (document.activeElement !== pairUI("rate2")) pairUI("rate2").value = m.rate2;
      for (const [id,index] of [["depth1",0],["depth2",1]])
        if (document.activeElement !== pairUI(id)) pairUI(id).value = m.depths[index];
    }
  } else if (m.type === "pair-result") pairUI("result").textContent = m.message;
  else if (m.type === "pair-snapshot") {
    const url = URL.createObjectURL(new Blob([m.bytes],{type:"application/octet-stream"}));
    const a = document.createElement("a"); a.href=url; a.download="caravel-engine-pair.epm"; a.click();
    URL.revokeObjectURL(url);
  }
}
const pairSend = (op,args=[]) => send({type:"pair",op,args});
pairUI("enable").onclick = () => pairSend("enable",[+pairUI("a").value,+pairUI("b").value]);
pairUI("page").onclick = () => pairSend("page");
pairUI("prepare").onclick = () => pairSend("prepare",[+pairUI("a").value,+pairUI("b").value,+pairUI("ea").value,+pairUI("eb").value]);
pairUI("select-a").onclick = () => pairSend("select",[pairState?.a ?? +pairUI("a").value]);
pairUI("select-b").onclick = () => pairSend("select",[pairState?.b ?? +pairUI("b").value]);
pairUI("panic").onclick = () => pairSend("panic");
pairUI("disable").onclick = () => pairSend("off");
pairUI("demo").onclick = () => pairSend("sequence");
pairUI("save").onclick = () => pairSend("save");
pairUI("morph").oninput = (e) => pairSend("morph",[pairState?.sel ?? 0,+e.target.value]);
pairUI("motion").onchange = (e) => pairSend("motion",[+e.target.checked]);
for (const [id, control] of [["level",0],["pan",1],["mute",2],["latch",3],["rate1",5]])
  pairUI(id).oninput = (e) => pairSend("control",[pairState?.sel ?? 0,control,id==="mute" ? +e.target.checked : +e.target.value]);
pairUI("rate2").oninput = (e) => pairSend("lfo2_rate",[pairState?.sel ?? 0,+e.target.value]);
for (const [id,slot,source] of [["depth1",0,1],["depth2",1,11]])
  pairUI(id).oninput = (e) => pairSend("lfo",[pairState?.sel ?? 0,slot,source,+e.target.value]);
pairUI("load").onchange = async (e) => {
  const file=e.target.files[0]; if (!file) return;
  if (file.size !== 4032) { pairUI("result").textContent="Invalid EPM snapshot size."; return; }
  send({type:"pair",op:"restore",bytes:new Uint8Array(await file.arrayBuffer())});
  e.target.value="";
};
