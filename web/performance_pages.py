#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Preserve versioned 0.6.0 performance content on a verified 0.6.0/0.7.0/0.7.1 site.

This never builds, replaces or relabels firmware, emulator or release files.
The Pages workflow first builds from the published tag, then applies this
independently versioned website content from the selected main commit.
"""
import argparse
import hashlib
import html
import io
import json
from pathlib import Path
import re
import shutil
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "docs/performance/stasis-060"
PROJECT = "Caravel_060_STASIS_HOME.hpm"
PACKAGE = "Caravel_060_STASIS_Performance.zip"
MIDI = "Caravel_060_V4b_Notes_90BPM.mid"
START = "<!-- CARAVEL-PERFORMANCE-START -->"
END = "<!-- CARAVEL-PERFORMANCE-END -->"


def load_assets(source=ASSETS):
    manifest = json.loads((source / "manifest.json").read_text())
    if manifest["version"] != "0.6.0" or manifest["project"] != "STASIS 060":
        raise ValueError("Unexpected performance version/project")
    data = {}
    for name, digest in manifest["files"].items():
        if Path(name).name != name or (source / name).is_symlink():
            raise ValueError("Unsafe performance asset")
        raw = (source / name).read_bytes()
        if hashlib.sha256(raw).hexdigest() != digest:
            raise ValueError("Performance checksum mismatch: " + name)
        data[name] = raw
    if len(data[PROJECT]) != 10000 or not data[PROJECT].startswith(b"HPM1"):
        raise ValueError("Invalid HPM project")
    if not data[MIDI].startswith(b"MThd"):
        raise ValueError("Invalid MIDI file")
    with zipfile.ZipFile(io.BytesIO(data[PACKAGE])) as archive:
        if archive.testzip():
            raise ValueError("Corrupt performance ZIP")
        for name in (PROJECT, MIDI, "PLAY_GUIDE_EN.md", "Spielplan_DE.md",
                     "caravel_backup.py", "fm1_install.py", "LICENSE.txt"):
            if archive.read(name) != data[name]:
                raise ValueError("ZIP/standalone asset mismatch: " + name)
    return manifest, data


def marked(body):
    return START + body + END


def clean(text):
    return re.sub(re.escape(START) + r".*?" + re.escape(END), "", text, flags=re.S)


def performance_body():
    rows = [("1 · Play", "Pair MORPH", "T3 LEVEL", "T2 SEM CUT", "Pair LEVEL"),
            ("2 · Pulse", "T1 SLICER", "PATTERN", "RATE", "DEPTH"),
            ("3 · Colour", "T1 SEM CUT", "T2 REV", "T1 REV", "T3 DIST")]
    table = "".join("<tr>" + "".join("<td>" + html.escape(cell) + "</td>" for cell in row) + "</tr>" for row in rows)
    return f'''<p class="lead">Load the starting project from the feature Short: two SEM tracks in a Pair, a gently driven Drone and twelve HOME controls. Then follow the moves, one control at a time.</p>
<div class="actions"><a href="{PACKAGE}" download>Download the full performance</a><a href="{PROJECT}" download>Project only · .hpm</a></div>
<p class="note">Free download · no account · requires Caravel 0.6.0</p>
<section class="chapter"><div class="chapter-grid"><figure><img src="home.png" width="240" height="240" alt="Actual Caravel HOME display with Pair Morph and controls for Tracks 1, 2 and 3"><figcaption>Actual firmware display</figcaption></figure><div><h2>STASIS 060</h2><p>Track 1: SEM + slow GATE SLICER.<br>Track 2: deep SEM, paired with Track 1.<br>Track 3: a stable Drone with 22% native distortion.</p><p>The complete project contains sounds, HOME assignments and Pair state for all four tracks. It is a <strong>starting setup</strong>; the knob movements and captured audio are not stored in the project.</p></div></div></section>
<section class="chapter"><h2>Your HOME bank</h2><div class="table-wrap"><table><thead><tr><th>Row</th><th>K1</th><th>K2</th><th>K3</th><th>K4</th></tr></thead><tbody>{table}</tbody></table></div><p class="note">SELECT changes rows. Keep Track 1 selected. STASIS HOLD and BLEND use the actual REVERB and LAYER pages. Row names above describe their purpose; they are not custom device labels.</p></section>
<section class="chapter" id="load"><h2>Load it on the FM-1</h2><ol><li>Run Caravel 0.6.0, connect USB, stop playback and close other MIDI editors.</li><li>Unzip the full performance download. Install Python 3 and the MIDI dependencies:<pre><code>python3 -m pip install mido python-rtmidi</code></pre></li><li>In the unzipped folder, check the ports and import into a saved slot you can replace, for example D:<pre><code>python3 caravel_backup.py ports
python3 caravel_backup.py import D {PROJECT}</code></pre>The tool asks before writing. The chosen slot is replaced; save an existing project separately if you want to keep it.</li><li>On the FM-1, open <strong>SAVE → PROJECT → D</strong> and load it.</li></ol><p>This imports a project; it does not install firmware or automatically load the working project.</p><p class="note">The exact HPM passed the firmware's simulated USB import, flash readback and project-load checks. A physical FM-1 import was not performed here.</p></section>
<section class="chapter"><h2>Play the notes. Make the moves.</h2><p>The MIDI file plays notes on channel 1 for the SEM Pair and channel 3 for the Drone, at 90 BPM. It sends no CC or SysEx. Perform MORPH, SLICER DEPTH, STR, HOLD, BLEND and STOP yourself using the guide.</p><p>Move times start at the beginning of the 30-second performance; in the 42-second Short, this is video time 0:06.</p><div class="actions"><a href="PLAY_GUIDE_EN.md" download>Play guide · English</a><a href="Spielplan_DE.md" download>Spielplan · Deutsch</a><a href="{MIDI}" download>MIDI notes only</a></div></section>
<section class="sound"><h2>Hear the 30-second performance</h2><audio controls preload="none" aria-label="STASIS 060 thirty-second performance"><source src="preview.mp3" type="audio/mpeg">Your browser does not support audio playback.</audio><p class="note">Original Caravel 0.6.0 firmware DSP, natively rendered. This is not an FM-1 hardware recording.</p></section>'''


def build(site, source=ASSETS):
    import make_site
    import stasis_pages
    site = Path(site)
    # Verify the destination profile before writing any website content.
    installer = site / "webapp/installer/index.html"
    landing = site / "index.html"
    version = re.search(r'<span class="pill">v(0\.6\.0|0\.7\.[01])</span>', landing.read_text())
    if '"release_verified": true' not in installer.read_text() or not version:
        raise ValueError("Performance content requires a verified Caravel 0.6.0/0.7.0/0.7.1 site")
    manifest, data = load_assets(source)
    look = make_site.look_css(installer.read_text())
    historical = version[1] in ('0.7.0','0.7.1')
    source_url = 'https://github.com/bonszaimusic/caravel/releases/download/v0.6.0/caravel-0.6.0-source.tar.gz'
    if historical:
        # Keep established tutorial/audio URLs, explicitly labelled 0.6.0.
        # Generate separately so the selected release's installer/editor/emulator
        # and strict firmware inventory remain byte-for-byte unchanged.
        with tempfile.TemporaryDirectory(prefix='caravel-historical-content-') as temp:
            stage = Path(temp)
            stasis_pages.build(stage, look, make_site.SHIP_PATH, ROOT)
            for name in ('tutorials', 'sounds', 'changelog'):
                path = stage / name / 'index.html'
                path.write_text(path.read_text().replace('../firmware/caravel-0.6.0-source.tar.gz', source_url))
                shutil.copytree(stage / name, site / name, dirs_exist_ok=True)
    target = site / "performance"
    target.mkdir(exist_ok=True)
    for name, raw in data.items():
        (target / name).write_bytes(raw)
    (target / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    page = stasis_pages.page("Load the performance. Make it yours.", performance_body(),
                             look, make_site.SHIP_PATH)
    if historical:
        page = page.replace('../firmware/caravel-0.6.0-source.tar.gz', source_url)
    page = page.replace("</style>", ".table-wrap{overflow-x:auto}table{width:100%;border-collapse:collapse;margin:16px 0}th,td{text-align:left;padding:10px 12px;border-bottom:1px solid var(--raise);white-space:nowrap}pre{overflow-x:auto;padding:12px;background:var(--bg);border:1px solid var(--raise);border-radius:4px;font-size:13px}li{margin:12px 0}</style>")
    page = page.replace("</nav>", '<a href="./" aria-current="page">PERFORMANCE</a></nav>', 1)
    (target / "index.html").write_text(page)
    text = clean(landing.read_text())
    card = '<section class="box sec" id="performance"><span class="tag">PERFORMANCE DOWNLOAD</span><p><a href="performance/">Load the STASIS HOME setup from the feature Short.</a></p><p>Full project, MIDI notes and a step-by-step play guide.</p><p><a href="performance/' + PACKAGE + '" download>Full download · ZIP</a> · <a href="performance/' + PROJECT + '" download>Project · HPM</a></p></section>'
    if historical:
        card += '<section class="box sec"><span class="tag">0.6.0 STASIS RESOURCES</span><p><a href="tutorials/">Tutorials and PDF guide</a> · <a href="sounds/">Sound examples</a> · <a href="changelog/">0.6.0 changelog</a></p></section>'
    if "</section>" not in text or "</nav>" not in text:
        raise ValueError("Unexpected Caravel landing layout")
    text = text.replace("</section>", "</section>" + marked(card), 1)
    text = text.replace("</nav>", marked('<a href="performance/">PERFORMANCE</a>') + "</nav>", 1)
    landing.write_text(text)
    for relative in ("tutorials/index.html", "sounds/index.html", "changelog/index.html"):
        path = site / relative
        if not path.is_file():
            raise ValueError("Missing release page: " + relative)
        text = clean(path.read_text())
        text = text.replace("</nav>", marked('<a href="../performance/">PERFORMANCE</a>') + "</nav>", 1)
        if relative != "changelog/index.html":
            callout = '<section class="chapter"><h2>Replay the feature Short.</h2><p>Load STASIS 060: the complete HOME project, MIDI notes and the individual moves.</p><div class="actions"><a href="../performance/">Download the performance setup</a></div></section>'
            text = text.replace("<footer>", marked(callout) + "<footer>", 1)
        path.write_text(text)
    # All release installers, firmware and emulator files remain byte-for-byte intact.
    stasis_pages.check_site(site)
    print("Performance site PASS: local downloads, HOME setup and play guides")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-source", action="store_true")
    parser.add_argument("--site", type=Path)
    args = parser.parse_args()
    load_assets()
    print("Performance assets PASS: SHA256, HPM, MIDI and ZIP correspondence")
    if args.site:
        build(args.site)
