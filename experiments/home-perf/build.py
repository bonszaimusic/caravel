#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Full firmware C/WASM mobile experiment. No hardware image or deployment."""
from pathlib import Path
import os,subprocess,shutil,json,base64,zipfile,hashlib
R=Path(__file__).resolve().parents[2];H=Path(__file__).resolve().parent
# Let opt-in experimental variants reuse the original full HOME / PAIR C/WASM
# mobile build without affecting regular release or HOME-only builds.
extra=os.environ.get('ACTION_EMU_CFLAGS','')
env=dict(os.environ,EXPERIMENT_CFLAGS='-DFELUCCA_HOME_PERF=1 -DFELUCCA_PERF_HW=1 '+extra,
         EXPERIMENT_VERSION=os.environ.get('ACTION_EMU_VERSION','HOME-PERFORMANCE-V10'))
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
if 'CARAVEL_ACTION_BANKS=1' in extra:
    guide='''<section class="home-panel" aria-label="ACTION banks v0.6.0">
<h2>CARAVEL 0.6.0 · HOME ACTION BANKS · ANDROID PREVIEW</h2>
<p><strong>STASIS GRAIN DIREKT:</strong> FX &gt; REVERB &gt; TYPE STASIS,
dann die gemeinsame STASIS-Ansicht (GRAIN oben / LAYER unten). Auf dem echten FM-1-Display stehen
<strong>K1 GRAIN · K2 SCAN · K3 WINDOW · K4 REVERSE</strong>.
Kein ACTION-Modus oder BANK-Langdruck erforderlich.
Im ACTION-Modus führt STASIS &gt; BANK lang ebenfalls DIREKT zur GRAIN-Seite;
SELECT wechselt zwischen GRAIN und FX.</p>
<p><strong>FX-Reihenfolge:</strong> FX SENDS/DELAY → CHORUS/REVERB → STASIS (GRAIN/LAYER) → INSERT → SLICER. Beide STASIS-Zeilen bleiben sichtbar; die markierte Zeile gehört K1–K4. Auf LAYER startet oder stoppt OCT+ die gewählte Aktion.</p>
<p><strong>RANDOM MORPH Schutz:</strong> LEVEL/Gain wird nie zufällig verändert.
F_CUTOFF-Ziele bleiben zwischen 150 und 6000 Hz.
Sobald ein gemorphter Parameter manuell gedreht wird, übernimmt der Encoder
den Wert für den Rest des laufenden Morphs (auch bei FREEZE / RESUME / RETURN).
Weitere Morphs können mit neuem Ausgangszustand gestartet werden.</p>
<p><strong>Am virtuellen FM-1:</strong> HOME zweimal kurz tippen, um ACTION ON/OFF zu schalten.
Die fünf höchsten Keyboard-Tasten: <strong>erste = BANK</strong>, folgende vier = ACTION 1–4.
<strong>BANK kurz (loslassen):</strong> Bank wechseln. <strong>BANK mindestens 0,5 Sekunden:</strong>
SETUP ein-/ausblenden (kein gleichzeitiges Halten beim Drehen nötig).
In SETUP bearbeiten K1–K4 bankbezogene Werte; die vier ACTION-Tasten bleiben spielbar.
In PERFORM bearbeiten K1–K4 weiter die normale HOME-Zeile.
SELECT wechselt weiterhin HOME-Zeilen, ALGORITHM den Track.
Die dritte kleine HOME-Zeile zeigt die Bank und vier Aktionen. OCT+/− bleiben frei.</p>
<p><strong>SETUP:</strong> RANDOM = Dauer (Bars), Depth, Scope, Shape;
STASIS = STR/MOTN (REVERB), AMT/TIME (LAYER), nur bei aktivem TYPE STASIS;
PAIR = Morph und vorhandene LFO1/LFO2-MOD-Amounts (keine MOD-Slots werden überschrieben);
LIVE = Track-Level T1–T4. Keine doppelten Projektparameter.
RANDOM Scope-Wechsel setzt nur die aktuelle Morph-Bewegung zurück, nicht gespeicherte Snapshot-Slots; Depth wirkt für das nächste Zufallsziel.</p>
<p><strong>NEU: Vier Snapshot-Slots in RANDOM:</strong> A4 kurz tippen und loslassen = S1→S2→S3→S4→S1 auswählen.
A4 mindestens 0,5 Sekunden halten = aktuelle hörbare HOME-Parameter im gewählten Slot speichern;
A3 = vom aktuellen Zustand in den gewählten Snapshot morphen.
A2 = Freeze/Resume auch bei Snapshot-Morphs. A1 = weiterhin Random.
Die Anzeige <strong>S1*</strong> bedeutet belegt, <strong>S1-</strong> bedeutet leer.
Wenn kein Snapshot belegt ist, bleibt A3 als bisheriges Return A nutzbar, solange ein Random-Morph aktiv ist.
Ein Snapshot kann ROW/TRACK/ALL mit jeweils den morphfähigen HOME-Parametern enthalten.
Snapshots sind <strong>nur im RAM</strong>; Neustart löscht S1–S4. Sie gehören nicht zu Projekt-/Preset-Saves.
Ein A4-Speichern während eines laufenden Morphs friert die aktuellen hörbaren Werte als neuen Slot
ein und beendet die bisherige Morph-Trajektorie, ohne die Klangparameter zurückzustellen.</p>
<p>Bänke: RANDOM (Random, Freeze/Resume, Recall/Return A, Snap wählen/speichern), STASIS
(Hold, Add, Blend, Release), PAIR (A, 50 %, B, Motion), LIVE (Mute T1–T4).
Die 22 übrigen physischen Tasten und externe MIDI-Noten spielen unverändert.
Zum Testen von STASIS: FX &gt; REVERB &gt; TYPE STASIS wählen.
<strong>Nur Emulator-Prototyp:</strong> ACTION-Bänke sind vorläufig feste Vorlagen, Snapshot-Speicher sind RAM-only,
nicht projektgespeichert. Keine Firmware zum Flashen.</p></section>'''
    if 'CARAVEL_STASIS_EXPAND=1' in extra:
        guide += '''<section class="home-panel" aria-label="STASIS GRAIN experiment">
<h2>STASIS GRAIN · LONG CAPTURE · ACTION SETUP</h2>
<p><strong>Direktzugriff:</strong> FX &gt; REVERB &gt; TYPE auf STASIS setzen,
anschließend die gemeinsame <strong>STASIS-Seite</strong> öffnen: GRAIN oben, LAYER unten. SELECT wechselt die aktive Reglerzeile.
Alternativ: HOME zweimal drücken, ACTION-Bank <strong>STASIS</strong> wählen (BANK kurz)
und BANK ca. 0,5 s halten: GRAIN öffnet sich sofort, ohne SELECT.
<strong>SELECT drehen</strong> wechselt im ACTION SETUP zwischen
<strong>GRAIN 2/2</strong> und <strong>FX 1/2</strong>
(bestehende STR, MOTN, ADD/BLEND AMT, TIME).
K1 <strong>GRAIN</strong> 93 / 186 / 372 ms;
K2 <strong>SCAN</strong> 0–100 % des sicher erfassten Fensters;
K3 <strong>WINDOW</strong> HQ (44,1 kHz / ca. 0,74 s),
LONG (22,05 kHz / ca. 1,49 s) oder DRONE (11,025 kHz / ca. 2,97 s);
K4 <strong>REVERSE</strong> vorwärts/rückwärts pro Grain.
Für WINDOW-Wechsel erst HOLD lösen und das Ausklingen beenden;
der neue Modus beginnt ein frisches LIVE-Recording.
Grain-Länge während HOLD nur bei genügend aufgezeichnetem Material erhöhen.</p>
<p>Die vier ACTION-Tasten sind weiter HOLD, ADD, BLEND, RELEASE.
SCAN ist jetzt außerdem ein eigenes MOD-Ziel: LFO oder S&amp;H &gt; SCAN,
z. B. mit Amount +20. Bereits vorhandene MOD-Ziele CAPT/STR/MOTN funktionieren unverändert.
MOTN beeinflusst weiterhin Filter und Mikro-Tap-Bewegung; SCAN durchsucht unabhängig
den länger aufgenommenen Klang. Keine zusätzlichen Audiopuffer: bestehende
zwei 32.768-Sample-Capture-Bänke werden je nach WINDOW mit unterschiedlicher
Abtastrate gespeichert, bei Wiedergabe interpoliert.
<strong>Opt-in, RAM-only:</strong> neue GRAIN/WINDOW/SCAN/REVERSE-Werte werden noch
nicht im Projekt gespeichert. PITCH shifting ist in diesem Test nicht enthalten.
Audioqualität und CPU-Reserve auf echter FM-1-Hardware stehen noch aus.</p></section>'''
    page=page.replace('<div class="boxes">',guide+'\n<div class="boxes">')
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
if 'CARAVEL_STASIS_EXPAND=1' in extra:
    # Chrome file:// can sometimes instantiate Blob-based AudioWorklet modules;
    # content:// and restrictive browsers cannot. Try the genuine worklet first,
    # then retain the prior offline ScriptProcessor (no network or external files).
    inline_worklet = w.replace('export ', '')
    standalone_worklet = r"""
const STASIS_WORKLET_SOURCE = @SOURCE@;
async function createStasisAudioNode(ac) {
  if(ac.audioWorklet && typeof AudioWorkletNode === "function"){
    const url=URL.createObjectURL(new Blob([STASIS_WORKLET_SOURCE],{type:"text/javascript"}));
    try {
      await ac.audioWorklet.addModule(url);
      const result=new AudioWorkletNode(ac,"felucca",
        {numberOfInputs:0,numberOfOutputs:1,outputChannelCount:[2]});
      result._caravelMode="AudioWorklet (separater Audiothread)";
      return result;
    }catch(err){
      console.warn("Caravel standalone: AudioWorklet unavailable; using legacy audio",err);
    }finally { URL.revokeObjectURL(url); }
  }
  const fallback=createOfflineAudioNode(ac);
  fallback._caravelMode="Fallback: Audiothread geteilt (bei hoher Browserlast Knarzen möglich)";
  return fallback;
}
""".replace('@SOURCE@',json.dumps(inline_worklet))
    single=single.replace('node = createOfflineAudioNode(ac);', 'node = await createStasisAudioNode(ac);', 1)
    single=single.replace('const SANS =',standalone_worklet+'\nconst SANS =',1)
    single=single.replace('node = await createStasisAudioNode(ac);',
      'node = await createStasisAudioNode(ac);\n    homeEl("audio").textContent="Audio: "+node._caravelMode;',1)
offline_adapter=(R/'experiments/engine-pair-morph/offline_audio.js').read_text()
if 'CARAVEL_STASIS_EXPAND=1' in extra:
    # A slightly larger fallback buffer gives Android's main-thread audio
    # more scheduling slack when it must repaint the high-res FM-1 UI.
    # The preferred AudioWorklet keeps native 128-frame render quanta.
    offline_adapter=offline_adapter.replace('ac.createScriptProcessor(1024,0,2)',
                                           'ac.createScriptProcessor(2048,0,2)')
adapter=offline_adapter.replace('/* @PROCESSOR_SOURCE@ */',w.replace('export ',''))
helpers=w.split('if (typeof registerProcessor === \"function\") {',1)[0].replace('export ','')
single=single.replace('import { midiKnob, KNOB_MODES } from \"./worklet.js\";',helpers+'\n'+adapter)
single=single.replace('<script type="module">','<script>\n(()=>{"use strict";').replace('</script>','\n})();\n</script>')
single=single.replace('@font-face { font-family: "DotGothic16"; src: url("fonts/DotGothic16-subset.woff") format("woff"); font-display: swap; }','')
font=json.dumps(base64.b64encode((out/'fonts/DotGothic16-subset.woff').read_bytes()).decode())
single=single.replace('const SANS =','const hpFont=new FontFace("DotGothic16",Uint8Array.from(atob('+font+'),c=>c.charCodeAt(0)).buffer);document.fonts.add(hpFont);hpFont.load();\nconst SANS =')
assert 'import ' not in single[single.index('<script>'):]
assert 'fetch("felucca.wasm")' not in single
if os.environ.get('ACTION_EMU_VERSION','').startswith('v0.7.0'):
    single=single.replace('0.6.0','0.7.0-rc1')
if 'CARAVEL_ACTION_BANKS=1' in extra:
    (out/'Caravel-ACTION-BANKS-0.6.0-Android.html').write_text(single)
else:
    (out/'Caravel-HOME-Performance-Handy.html').write_text(single)
print('HOME standalone:',len(single),'bytes; WASM SHA256',hashlib.sha256((out/'felucca.wasm').read_bytes()).hexdigest())
