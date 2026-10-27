#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Versioned tutorials, original DSP examples and changelog for the 0.6.0 site."""
import argparse,hashlib,html,json,re,shutil,struct
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote,urlsplit

ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load_sources(root=ROOT):
    guide=json.loads((root/"docs/tutorials/stasis-0.6.0.json").read_text())
    audio=json.loads((root/"docs/audio/stasis-0.6.0.json").read_text())
    if guide["version"]!="0.6.0" or audio["version"]!="0.6.0" or len(guide["chapters"])!=4 or len(audio["clips"])!=3:
        raise ValueError("Incorrect 0.6.0 tutorial/audio inventory")
    pdf=root/"docs/tutorials"/guide["pdf"]
    if sha(pdf)!=guide["pdf_sha256"] or not pdf.read_bytes().startswith(b"%PDF-"):
        raise ValueError("Tutorial PDF checksum/format mismatch")
    for chapter in guide["chapters"]:
        image=root/"docs/tutorials/img"/chapter["image"]
        raw=image.read_bytes()
        if sha(image)!=chapter["image_sha256"] or raw[:8]!=b"\x89PNG\r\n\x1a\n" or struct.unpack(">II",raw[16:24])!=(240,240):
            raise ValueError("Incorrect tutorial screenshot: "+image.name)
    for clip in audio["clips"]:
        p=root/"docs/audio"/clip["file"]
        if sha(p)!=clip["sha256"] or clip["decoded_peak"]>=1 or clip["held_rms"]<=.001 or clip["seconds"] not in (20,24):
            raise ValueError("Incorrect demo audio: "+p.name)
    return guide,audio

def e(s):return html.escape(str(s),quote=True)
def page(title,body,look,ship):
    return f'''<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>{e(title)} - Caravel 0.6.0</title>
<meta name="description" content="Caravel 0.6.0 STASIS: illustrated tutorials, original DSP sound examples, ADD, BLEND and upgrade notes.">
<style>{look}
body{{margin:0;background:var(--bg);color:var(--text);font:16px/1.65 -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}}
main{{max-width:1000px;margin:auto;padding:34px 20px 60px}}h1{{font-size:clamp(28px,5vw,44px);line-height:1.15;margin:12px 0 16px;max-width:24ch}}
h2{{font-size:23px;line-height:1.25;margin:0 0 18px}}h3{{font-size:17px;margin:20px 0 6px}}p{{margin:0 0 14px}}
a{{color:var(--amb)}}.lead{{max-width:76ch;color:var(--dim)}}.eyebrow{{color:var(--amb);font:700 12px/1.5 var(--mono);letter-spacing:.13em}}
.actions{{display:flex;flex-wrap:wrap;gap:12px;margin:22px 0 34px}}.actions a{{padding:9px 14px;border:1px solid var(--ctl);border-radius:5px;text-decoration:none}}
.chapter,.sound{{padding:26px;margin:22px 0;background:var(--surf);border:1px solid var(--raise);border-radius:10px;scroll-margin-top:20px}}
.chapter-grid{{display:grid;grid-template-columns:240px minmax(0,1fr);gap:30px}}figure{{margin:0}}figure img{{width:240px;max-width:100%;image-rendering:pixelated;border-radius:4px}}
figcaption,.note{{color:var(--dim);font-size:13px}}.tip{{border-left:3px solid var(--amb);padding:12px 16px;background:var(--bg);margin-top:20px}}
audio{{width:100%;max-width:640px;margin:8px 0 12px}}.cues{{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:5px 24px;list-style:none;padding:0;margin:16px 0}}
.cues time{{color:var(--amb);font-family:var(--mono);margin-right:9px}}.scope{{padding:20px;border:1px solid var(--ctl);border-radius:7px;margin-top:28px;max-width:82ch}}
.changes{{max-width:78ch}}.changes h2{{margin-top:30px}}footer{{border-top:1px solid var(--raise);margin-top:32px;padding-top:18px;color:var(--dim);font-size:12px}}
@media(max-width:700px){{main{{padding:26px 14px 40px}}.chapter,.sound{{padding:20px 16px}}.chapter-grid{{grid-template-columns:1fr;gap:24px}}figure{{text-align:center}}.cues{{grid-template-columns:1fr}}}}
</style></head><body><header class="site"><a class="logo" href="../"><svg class="ship" width="42" height="26" viewBox="0 0 28 17" aria-hidden="true"><path d="{ship}"/></svg>CARAVEL</a>
<span class="pill">v0.6.0</span><nav aria-label="Caravel"><a href="../webapp/installer/">INSTALL</a><a href="../emulator/">EMULATOR</a><a href="../tutorials/">TUTORIALS</a><a href="../sounds/">SOUNDS</a><a href="../changelog/">CHANGELOG</a></nav></header>
<main><div class="eyebrow">CARAVEL 0.6.0 / STASIS</div><h1>{e(title)}</h1>{body}
<footer>Caravel by bonszaimusic, based on Felucca 1.6.1. Independent of M-VAVE and Hugelton Instruments.
<a href="../firmware/LICENSING.md">Licensing</a> · <a href="../firmware/NOTICE">Notices</a> · <a href="../firmware/caravel-0.6.0-source.tar.gz">Matching source</a></footer></main></body></html>'''

def changelog(root):
    text=(root/"CHANGELOG.md").read_text().split("## [0.6.0]",1)[1].split("\n## [",1)[0]
    blocks=[];paragraph=[];items=[]
    def flush():
        if paragraph:blocks.append("<p>"+e(" ".join(paragraph))+"</p>");paragraph.clear()
        if items:blocks.append("<ul>"+"".join("<li>"+e(x)+"</li>" for x in items)+"</ul>");items.clear()
    for line in text.splitlines()[1:]:
        if line.startswith("### "):flush();blocks.append("<h2>"+e(line[4:])+"</h2>")
        elif line.startswith("- "):items.append(line[2:])
        elif not line.strip():flush()
        elif items:items[-1]+=" "+line.strip()
        else:paragraph.append(line.strip())
    flush();return "".join(blocks)

def build(stage,look,ship,root=ROOT):
    guide,audio=load_sources(root)
    tutorials=stage/"tutorials";sounds=stage/"sounds";changes=stage/"changelog"
    for p in [tutorials/"img",sounds,changes]:p.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(root/"docs/tutorials"/guide["pdf"],tutorials/guide["pdf"])
    shutil.copyfile(root/"docs/tutorials/stasis-0.6.0.json",tutorials/"stasis-0.6.0.json")
    intro='<p class="lead">Capture a moment, build on a held texture, or move into a new sound. These four short lessons use the actual firmware display.</p>'
    intro+=f'<div class="actions"><a href="{e(guide["pdf"])}" download>Download the four-page PDF</a><a href="../sounds/">Hear the examples</a><a href="../emulator/">Try the emulator</a></div>'
    content=[]
    for chapter in guide["chapters"]:
        image=chapter["image"];shutil.copyfile(root/"docs/tutorials/img"/image,tutorials/"img"/image)
        steps="".join(f'<h3>{e(s["title"])}</h3><p>{e(s["text"])}</p>' for s in chapter["steps"])
        content.append(f'<section class="chapter" id="lesson-{chapter["number"]}"><div class="eyebrow">{e(chapter["number"])} / {e(chapter["tag"])}</div><h2>{e(chapter["title"])}</h2><div class="chapter-grid"><figure><img src="img/{e(image)}" width="240" height="240" alt="{e(chapter["tag"])} on the actual Caravel firmware display" loading="lazy"><figcaption>Actual firmware display</figcaption></figure><div>{steps}</div></div><p class="tip">{e(chapter["tip"])}</p><p class="note">{e(chapter["note"])}</p></section>')
    intro+="".join(content)+'<p><a href="https://github.com/bonszaimusic/caravel/blob/v0.6.0/docs/tutorials/README.md#performance-workflow">HOME, Pair Morph, macros and performance storage tutorials</a></p>'
    (tutorials/"index.html").write_text(page("Make a sound last. Then take it further.",intro,look,ship))
    body='<p class="lead">Three original examples from the actual Caravel firmware DSP. Listen for what stays after the live source is muted, and how the texture changes.</p><p class="note">Native DSP renders, not recordings of an FM-1. Constant peak normalization and a short end fade; no external sound effects. Playback starts only when you press play.</p>'
    for clip in audio["clips"]:
        shutil.copyfile(root/"docs/audio"/clip["file"],sounds/clip["file"])
        cues="".join(f'<li><time>{int(t)//60}:{int(t)%60:02d}</time>{e(label)}</li>' for t,label in clip["cues"])
        body+=f'<section class="sound" id="{e(clip["id"])}"><div class="eyebrow">{clip["seconds"]} SECONDS / {e(clip["id"]).upper()}</div><h2>{e(clip["title"])}</h2><p>{e(clip["description"])}</p><audio controls preload="none" aria-label="{e(clip["title"])}"><source src="{e(clip["file"])}" type="audio/mpeg">Your browser does not support audio playback.</audio><p class="note"><a href="{e(clip["file"])}" download>Download MP3</a></p><ul class="cues">{cues}</ul></section>'
    body+='<div class="actions"><a href="../tutorials/">Follow the tutorial</a><a href="../emulator/">Play the emulator</a></div>'
    (sounds/"index.html").write_text(page("Hear HOLD, ADD and BLEND.",body,look,ship))
    shutil.copyfile(root/"docs/audio/stasis-0.6.0.json",sounds/"stasis-0.6.0.json")
    (changes/"index.html").write_text(page("What changed in 0.6.0.",'<div class="changes">'+changelog(root)+'</div><div class="actions"><a href="../tutorials/">Read the tutorials</a><a href="https://github.com/bonszaimusic/caravel/blob/v0.6.0/CHANGELOG.md">Full project changelog</a></div>',look,ship))
    for relative,prefix in [("webapp/installer/index.html","../../"),("webapp/editor/index.html","../../"),("emulator/index.html","../")]:
        p=stage/relative
        if p.exists():
            s=p.read_text();links="".join(f'<a href="{prefix}{target}/">{title}</a>' for target,title in [("tutorials","TUTORIALS"),("sounds","SOUNDS"),("changelog","CHANGELOG")])
            s=re.sub(r'(<a [^>]*href="https://github.com/bonszaimusic/caravel"[^>]*>DOCS</a>)',links+r'\1',s,count=1)
            p.write_text(s)

class Links(HTMLParser):
    def __init__(self):super().__init__();self.urls=[]
    def handle_starttag(self,tag,attrs):
        a=dict(attrs)
        for name in ("href","src"):
            if name in a:self.urls.append(a[name])

def check_site(stage):
    stage=stage.resolve();broken=[]
    for name in ("tutorials/index.html","sounds/index.html","changelog/index.html"):
        p=stage/name
        if not p.is_file():raise ValueError("Missing release page: "+name)
    for p in stage.rglob("*.html"):
        parser=Links();parser.feed(p.read_text())
        for raw in parser.urls:
            url=urlsplit(raw)
            if url.scheme or url.netloc or not url.path:continue
            target=(p.parent/unquote(url.path)).resolve()
            if stage not in target.parents and target!=stage:broken.append(raw);continue
            if target.is_dir():target=target/"index.html"
            if not target.is_file():broken.append(str(p.relative_to(stage))+": "+raw)
    if broken:raise ValueError("Broken local site links: "+", ".join(broken))
    print("0.6.0 site PASS: tutorials, PDF, audio players, changelog and local links")

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("--check-source",action="store_true");parser.add_argument("--check-site",type=Path);args=parser.parse_args()
    load_sources()
    print("0.6.0 media PASS: four real screenshots, PDF and three unclipped DSP examples")
    if args.check_site:check_site(args.check_site)
