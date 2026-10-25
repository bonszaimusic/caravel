#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Full firmware C/WASM mobile experiment. No hardware image or deployment."""
from pathlib import Path
import os,subprocess,shutil,json,base64,zipfile,hashlib
R=Path(__file__).resolve().parents[2];H=Path(__file__).resolve().parent
env=dict(os.environ,EXPERIMENT_CFLAGS='-DFELUCCA_HOME_PERF=1 -DFELUCCA_PERF_HW=1',EXPERIMENT_VERSION='HOME-PERFORMANCE-V10')
subprocess.run(['python3','experiments/engine-pair-morph/build.py'],cwd=R,env=env,check=True)
out=R/'build/home-perf';shutil.copytree(R/'build/engine-pair-morph',out,dirs_exist_ok=True)
page=(out/'index.html').read_text().replace('ENGINE-PAIR-PROTOTYPE-V6','HOME-PERFORMANCE-V10')
page=page.replace('caravel-engine-pair-experiment-flash-v1','caravel-home-performance-hw-v4')
import re
old_panel=(R/'experiments/engine-pair-morph/panel.html').read_text()
pair_panel=(H/'pair-panel.html').read_text()
for side in ('A','B'):
    options=re.search(r'<select id="pair-e'+side.lower()+r'">(.*?)</select>',old_panel,re.S).group(1)
    pair_panel=pair_panel.replace('@ENGINE_OPTIONS_'+side+'@',options)
assert old_panel in page
page=page.replace(old_panel,'')
page=page.replace('<div class="boxes">',(H/'panel.html').read_text()+pair_panel+'\n<div class="boxes">')
old_js=(R/'experiments/engine-pair-morph/panel.js').read_text()
assert old_js in page
page=page.replace(old_js,(H/'pair-panel.js').read_text())
# A downloaded emulator has no adjacent installer/editor. Keep its workflow local.
page=re.sub(r'<a href="\.\./webapp/(?:installer|editor)/">.*?</a>','',page)
page=re.sub(r'(<header\b[^>]*>.*?<nav\b[^>]*>).*?(</nav>)',r'\1<a href="#device">GERÄT</a><a href="#home-controls">HOME</a><a href="#pair-panel">PAIR</a>\2',page,count=1,flags=re.S)
page=page.replace('href="../"','href="#device"')
page=page.replace('<a href="../firmware/LICENSING.md">Licences</a>','Lizenzen im Testpaket')
page=page.replace('<a href="fonts/OFL.txt">SIL OFL 1.1</a>','SIL OFL 1.1 (Lizenz im Testpaket)')
page=page.replace('<span class="tag">Screen</span>','<span class="tag">Gerät · Regler ziehen, Tasten tippen oder halten</span>')
page=re.sub(r'<p>V6:.*?</p>','<p>PAIR über die ENV-Taste öffnen. K1 Partner, K2 ON/OFF, K3 MORPH, K4 MOTION. HOME bleibt frei belegbar: Morph auf PAIR drehen, dann GLO+HOME. Der Partner übernimmt den Voice-Modus von A.</p>',page)
page=page.replace('Download pair snapshot','Legacy EPM2 herunterladen (ohne HOME)').replace('Restore snapshot','Legacy EPM2 importieren')
page=page.replace('Snapshots use their own EPM2 format (EPM1 loads too).','Für komplette Projekte die HOME-Sicherung (.hpm) oben verwenden. Legacy EPM2 enthält keine HOME-Belegung.')

page=page.replace('pairReady(); res();','pairReady(); homeReady(); res();')
page=page.replace('            warned = true;','            warned = true; homeEl("storage").textContent="Dieser Browser speichert Projekte nur bis zum Schließen der Seite. Wichtige Stände jetzt als .hpm herunterladen.";')
page=page.replace('    if (ac.sampleRate !== 44100) console.warn(', '    if (ac.sampleRate !== 44100) homeEl("audio").textContent="Browser-Audio läuft nicht mit 44,1 kHz; Tonhöhe kann abweichen.";\n    if (ac.sampleRate !== 44100) console.warn(')
page=page.replace('else if (m.type.startsWith("pair-")) pairMessage(m);','else if (m.type.startsWith("pair-")) pairMessage(m);\n        else if (m.type.startsWith("home-")) homeMessage(m);')
page=page.replace('</script>',(H/'panel.js').read_text()+'\n</script>')
(out/'index.html').write_text(page)
w=(out/'worklet.js').read_text().replace('[0xE5000, 0xE8000]','[0xE5000, 0xFC000]')
w=w.replace('else if (m.type === "pair") this.pairMessage(m);','else if (m.type === "home") this.homeMessage(m);\n      else if (m.type === "pair") this.pairMessage(m);')
w=w.replace('    publish() {',(H/'worklet-home.js').read_text()+'\n    publish() {')
w=w.replace('Pair ready. Existing sounds kept; performance settings copied from A.','Pair aktiv. Sounds bleiben erhalten; Performance-Einstellungen werden von A übernommen.')
w=w.replace('Pair refused. Stop transport, release notes and check the PAIR page.','Pair abgelehnt. Transport stoppen, Noten loslassen und PAIR-Seite prüfen.')
w=w.replace('Pair ready. Play either member; CC119 controls shared morph.','Testsession erstellt. Beide Mitglieder spielen das Pair; Morph auf der PAIR-Seite.')
w=w.replace('      const msg = { type: "frame" };','''      const track=ex.web_pair_get(4,0);
      this.port.postMessage({type:'home-state',track,count:ex.web_home_get(0,track,0,0),row:ex.web_home_get(1,track,0,0),defaultSet:ex.web_home_get(2,track,0,0),pending:ex.web_home_get(10,track,0,0)});
      const msg = { type: "frame" };''')
(out/'worklet.js').write_text(w)
# Convert actual processor and actual WASM to a completely self-contained classic-script HTML.
wasm='Uint8Array.from(atob('+json.dumps(base64.b64encode((out/'felucca.wasm').read_bytes()).decode())+'),c=>c.charCodeAt(0)).buffer'
start=page.index('    const [wasm] = await Promise.all([');end=page.index(']);',start)+3
single=page[:start]+'    const wasm = '+wasm+';'+page[end:]
single=single.replace('typeof AudioContext === "undefined" || typeof AudioWorkletNode === "undefined"','typeof AudioContext === "undefined"')
single=single.replace('    if (!ac.audioWorklet) throw new Error("Dieser Dateikontext erlaubt keinen AudioWorklet. Die Serverversion benötigt localhost oder HTTPS.");','')
single=single.replace('new AudioWorkletNode(ac, "felucca", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] })','createOfflineAudioNode(ac)')
adapter=(R/'experiments/engine-pair-morph/offline_audio.js').read_text().replace('/* @PROCESSOR_SOURCE@ */',w.replace('export ',''))
helpers=w.split('if (typeof registerProcessor === \"function\") {',1)[0].replace('export ','')
single=single.replace('import { midiKnob, KNOB_MODES } from \"./worklet.js\";',helpers+'\n'+adapter)
single=single.replace('<script type="module">','<script>\n(()=>{"use strict";').replace('</script>','\n})();\n</script>')
single=single.replace('@font-face { font-family: "DotGothic16"; src: url("fonts/DotGothic16-subset.woff") format("woff"); font-display: swap; }','')
font=json.dumps(base64.b64encode((out/'fonts/DotGothic16-subset.woff').read_bytes()).decode())
single=single.replace('const SANS =','const hpFont=new FontFace("DotGothic16",Uint8Array.from(atob('+font+'),c=>c.charCodeAt(0)).buffer);document.fonts.add(hpFont);hpFont.load();\nconst SANS =')
assert 'import ' not in single[single.index('<script>'):]
assert 'fetch("felucca.wasm")' not in single
(out/'Caravel-HOME-Performance-Handy.html').write_text(single)
print('HOME standalone:',len(single),'bytes; WASM SHA256',hashlib.sha256((out/'felucca.wasm').read_bytes()).hexdigest())
