#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the full C emulator plus isolated Pair Morph controls. Never builds a firmware/release."""
import os
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
os.chdir(ROOT)
env = dict(os.environ, EMU_CFLAGS="-DFELUCCA_PAIR_MORPH=1 -DFELUCCA_CPU2=0 "+os.environ.get("EXPERIMENT_CFLAGS", ""), EMU_VERSION=os.environ.get("EXPERIMENT_VERSION","ENGINE-PAIR-PROTOTYPE-V6"))
subprocess.run(["sh", "web/emu/build.sh"], env=env, check=True)
out = ROOT / "build" / "engine-pair-morph"
out.mkdir(parents=True, exist_ok=True)
shutil.copytree(ROOT / "build/emu", out, dirs_exist_ok=True)
page = (out / "index.html").read_text()
start = page.index('function bindKnob(k) {')
end = page.index('const tall = matchMedia(', start)
page = page[:start] + (HERE / 'knob_drag.js').read_text() + '\n' + page[end:]
page = page.replace('</main>', '</main>\n' + (HERE / 'panel.html').read_text())
page = page.replace('"felucca-emu-flash"', '"caravel-engine-pair-experiment-flash-v1"')
page = page.replace('.start { position: absolute;', '.start { z-index: 100; position: absolute;')
page = page.replace('  try {\n    const ac = new AudioContext', '''  try {
    say("");
    if (typeof AudioContext === "undefined" || typeof AudioWorkletNode === "undefined")
      throw new Error("AudioWorklet ist hier nicht verfügbar. Bitte die Datei in Chrome selbst öffnen; falls nötig die Serverversion über localhost oder HTTPS verwenden.");
    const ac = new AudioContext''')
page = page.replace('    const [wasm] = await Promise.all([', '''    if (!ac.audioWorklet) throw new Error("Dieser Dateikontext erlaubt keinen AudioWorklet. Die Serverversion benötigt localhost oder HTTPS.");
    // Resume during the user's tap, before fetching/compiling the module.
    const audioReady = ac.resume();
    const [wasm] = await Promise.all([''')
page = page.replace('    await ac.resume();', '    await audioReady;')
page = page.replace('e.target.closest?.(".hit.knob")', 'e.target.closest?.(".hit.knob") || e.target.matches?.("input,select,textarea")')
page = page.replace('if (m.type === "ready") res();', 'if (m.type === "ready") { pairReady(); res(); }\n        else if (m.type.startsWith("pair-")) pairMessage(m);')
page = page.replace('</script>', (HERE / 'panel.js').read_text() + '\n</script>')
(out / 'index.html').write_text(page)
worklet = (out / 'worklet.js').read_text()
worklet = worklet.replace('      else if (m.type === "visible")', '      else if (m.type === "pair") this.pairMessage(m);\n      else if (m.type === "visible")')
worklet = worklet.replace('    publish() {', (HERE / 'worklet-pair.js').read_text() + '\n    publish() {')
worklet = worklet.replace('      const msg = { type: "frame" };', '''      const sel=ex.web_pair_get(4,0);
      const reasonPtr=ex.web_pair_reason(), pairMem=new Uint8Array(ex.memory.buffer);
      let reason=""; for(let j=reasonPtr;pairMem[j] && j<reasonPtr+64;j++) reason+=String.fromCharCode(pairMem[j]);
      this.port.postMessage({type:"pair-state",on:ex.web_pair_get(0,0),base:ex.web_pair_get(1,0),effective:ex.web_pair_get(2,0),
        budget:ex.web_pair_get(3,0),cells:ex.web_pair_get(14,0),sel,a:ex.web_pair_get(5,0),b:ex.web_pair_get(6,0),motion:ex.web_pair_get(9,0),
        engines:[0,1,2,3].map(t=>ex.web_pair_get(11,t)), gains:[0,1,2,3].map(t=>ex.web_pair_get(12,t)), partners:[0,1,2,3].map(t=>ex.web_pair_get(13,t)), reason,
        gates:[0,1,2,3].map(t=>ex.web_pair_get(7,t)),controls:[0,1,2,3,4,5].map(c=>ex.web_pair_control_get(sel,c)),
        rate2:ex.web_pair_lfo2_rate_get(sel),depths:[0,1].map(k=>ex.web_pair_depth_get(sel,k))});
      const msg = { type: "frame" };''')
(out / 'worklet.js').write_text(worklet)
# Offline entry point: actual compiled firmware and the same AudioWorklet, no server or deployment.
wasm_js = 'new Uint8Array(' + json.dumps(list((out / 'felucca.wasm').read_bytes()),separators=(',',':')) + ').buffer'
single = page.replace('fetch("felucca.wasm").then((r) => { if (!r.ok) throw new Error(`felucca.wasm: ${r.status}`); return r.arrayBuffer(); })',
                      'Promise.resolve(' + wasm_js + ')')
single = single.replace('typeof AudioContext === "undefined" || typeof AudioWorkletNode === "undefined"',
                        'typeof AudioContext === "undefined"')
single = single.replace('    if (!ac.audioWorklet) throw new Error("Dieser Dateikontext erlaubt keinen AudioWorklet. Die Serverversion benötigt localhost oder HTTPS.");', '')
single = single.replace('''    const [wasm] = await Promise.all([
      Promise.resolve(''' + wasm_js + '''),
      ac.audioWorklet.addModule("worklet.js")]);''', '    const wasm = ' + wasm_js + ';')
single = single.replace('new AudioWorkletNode(ac, "felucca", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] })',
                        'createOfflineAudioNode(ac)')
adapter = (HERE / 'offline_audio.js').read_text().replace('/* @PROCESSOR_SOURCE@ */', worklet.replace('export ', ''))
single = single.replace('import { midiKnob, KNOB_MODES } from "./worklet.js";',
                        adapter + '\nimport { midiKnob, KNOB_MODES } from "./worklet.js";')
single = single.replace('    go.textContent = "On";', '    go.textContent = "On";\n    document.getElementById("pair-result").textContent = "Offline-Audio aktiv; höhere Latenz möglich.";')
# A single offline HTML must not retain the page's relative module import.
# Use a classic inline script, so local/content URLs do not require a module fetch.
helpers = worklet.split('if (typeof registerProcessor === "function") {', 1)[0].replace('export ', '')
single = single.replace('import { midiKnob, KNOB_MODES } from "./worklet.js";', helpers)
single = single.replace('<script type="module">', '<script>\n(() => { "use strict";')
single = single.replace('</script>', '\n})();\n</script>')
font_js = json.dumps(list((out / 'fonts/DotGothic16-subset.woff').read_bytes()),separators=(',',':'))
single = single.replace('@font-face { font-family: "DotGothic16"; src: url("fonts/DotGothic16-subset.woff") format("woff"); font-display: swap; }','')
single = single.replace('const SANS =', 'const pairFont = new FontFace("DotGothic16", new Uint8Array(' + font_js + ').buffer); document.fonts.add(pairFont); pairFont.load();\nconst SANS =')
(out / 'caravel-engine-pair.html').write_text(single)
with zipfile.ZipFile(out / 'caravel-engine-pair-android.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    archive.write(out / 'caravel-engine-pair.html', 'caravel-engine-pair-v6-android.html')
    for name in ('LICENSE','LICENSING.md','NOTICE','CREDITS.md'):
        archive.write(ROOT / name, name)
    for notice in (ROOT / 'LICENSES').glob('*'):
        archive.write(notice, 'LICENSES/' + notice.name)
    archive.writestr('SOURCE.txt', 'Corresponding source: bonszaimusic/caravel.\nSee docs/tutorials/README.md.\nOffline audio uses a main-thread Web Audio callback with the same firmware WASM; higher latency is possible.\n')
print(f"Pair emulator: {out}")
