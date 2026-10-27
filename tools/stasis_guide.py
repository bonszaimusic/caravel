#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Create the four-page 0.6.0 STASIS guide with actual firmware screenshots."""
from pathlib import Path
import json,hashlib
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import Paragraph
from reportlab.lib.enums import TA_LEFT

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"docs/tutorials/Caravel-0.6.0-STASIS-Guide.pdf"
PAGES=[
    ("01","Capture a sound. Keep it moving.","stasis-hold.png","LIVE / HOLD",
     [("1. Feed the texture","Set a track's REV send to about 85-90%. On FX > REVERB, select TYPE STASIS. This page now reads TYPE / CAPT / STR / MOTN."),
      ("2. Capture","Leave CAPT on LIVE and play a chord for at least 0.6 seconds. Turn CAPT to HOLD: the recently played sound continues after you release the notes."),
      ("3. Shape the held sound","Start with STR around 70-85% and MOTN around 30-45%. STR brings the texture forward; MOTN moves reflections, filtering and HALL damping while keeping grain playback at its original rate.")],
     "Try it: SEM > WARM PAD, a low minor chord, CAPT HOLD, then raise MOTN. STOP releases the texture into the HALL tail.",
     "STASIS captures the internal REV send. It does not record microphone or USB audio. Ordinary tempo DELAY is suspended while STASIS is active."),
    ("02","Add another sound to the texture.","add-ready.png","ADD",
     [("1. Keep HOLD","Leave CAPT on HOLD and open FX > LAYER. The controls are MODE / AMT / TIME / action. Choose MODE ADD; AMT 25% is a good starting point."),
      ("2. Play the addition","Play the new sound with its REV send enabled for about 0.6 seconds. Every track with REV send contributes, so lower the other sends if you want just one source."),
      ("3. Press OCT+","OCT+ starts the overdub. MIX fades in the new recording over 200 ms; STORE finishes the mixture in about 0.75 s. READY means you can add again. CAPT remains HOLD.")],
     "Try it: hold a SEM pad, add a low DRONE note, then a higher HADES note. After capture you can mute each live source; its contribution remains in the texture.",
     "AMT is the new sound's share: 25% new + 75% old. Gains are complementary to keep repeated additions bounded. The layers merge and cannot be removed individually."),
    ("03","Blend into a new held texture.","blend-mix.png","BLEND",
     [("1. Choose the transition","With CAPT HOLD, open LAYER and choose MODE BLEND. Set TIME from 1 to 30 seconds; start with 6 s. AMT is not used in BLEND."),
      ("2. Play, then start","Play the destination sound through REV send for about 0.6 seconds, then press OCT+. A snapshot of the new sound replaces the held texture over TIME."),
      ("3. Hold the destination","When MIX and STORE finish, the destination stays in HOLD. This is one transition per press, not continuous automatic capture. Press OCT+ during WAIT or MIX to return smoothly to the original.")],
     "Try it: hold DRONE > TIDE, then blend into a brighter SEM > OB STRING chord. Release or mute the live source after capture to hear the transition alone.",
     "WAIT keeps the original while there is no sufficiently recent input. During STORE the commit finishes before another operation. STOP and HOLD > LIVE release the audible mixture normally."),
    ("04","Keep projects safe when upgrading.","stasis-compat.png","PROJECTS / PROFILE",
     [("1. Know the 0.6.0 profile","The release contains HALL and STASIS. ROOM, SPRING and the SLICE engine are omitted to fit the target. The separate SLICER track effect remains available."),
      ("2. Review the warning","Projects using omitted features show the actual substitutions: SLICE > ANALOG and ROOM/SPRING > HALL. OCT- CANCEL leaves the current session untouched; OCT+ TEST loads a converted working copy."),
      ("3. Save a separate copy","The source slot stays protected for the session. Save the audition into another slot. For RESTORE LAST, occupied slots are protected because the original source slot is unknown; use an empty slot.")],
     "Before updating: export a complete backup with matching Caravel tools. Keep user banks with the performance. The public installer provides the matching 0.6.0 package.",
     "Project saves keep MODE, AMT and TIME, but not the captured audio. Textures live in RAM. Automated tests pass; actual FM-1 worst-case load, physical USB/OTA and long-term stability are not certified by host tests."),
]
W,H=A4
def text(c,content,x,y,width,size=10.5,color="#20252a",leading=None):
    p=Paragraph(content,ParagraphStyle("body",fontName="Helvetica",fontSize=size,leading=leading or size*1.43,
         textColor=HexColor(color),alignment=TA_LEFT))
    _,height=p.wrap(width,H);p.drawOn(c,x,y-height);return y-height

def main():
    OUT.parent.mkdir(parents=True,exist_ok=True)
    c=canvas.Canvas(str(OUT),pagesize=A4,pageCompression=1,invariant=1)
    c.setTitle("Caravel 0.6.0 - STASIS guide");c.setAuthor("bonszaimusic")
    for index,(number,title,image,tag,steps,tip,boundary) in enumerate(PAGES,1):
        c.setFillColor(HexColor("#faf8f2"));c.rect(0,0,W,H,fill=1,stroke=0)
        c.setFillColor(HexColor("#1b1a17"));c.rect(0,H-116,W,116,fill=1,stroke=0)
        c.setFillColor(HexColor("#ffaa29"));c.setFont("Helvetica-Bold",12);c.drawString(42,H-34,"CARAVEL  /  0.6.0")
        c.setFillColor(HexColor("#ffffff"));c.setFont("Helvetica-Bold",22);c.drawString(42,H-72,title)
        c.setFont("Helvetica",10);c.setFillColor(HexColor("#d2c6ad"));c.drawString(42,H-94,tag)
        x,y=42,H-145
        c.drawImage(str(ROOT/"docs/tutorials/img"/image),x,y-220,width=220,height=220)
        text(c,"Actual firmware display",x,y-232,220,8.5,"#62635d")
        right=x+243;ry=y-1
        for heading,body in steps:
            ry=text(c,heading,right,ry,W-right-42,11.3,"#222420",14.5)-7
            ry=text(c,body,right,ry,W-right-42,10.2,leading=14.4)-17
        band=min(y-277,ry-10)
        c.setFillColor(HexColor("#fff0ca"));c.roundRect(42,band-100,W-84,100,8,fill=1,stroke=0)
        text(c,"PLAY IT",56,band-14,W-112,9.5,"#83510a")
        text(c,tip,56,band-36,W-112,11.2,leading=16)
        text(c,boundary,42,band-123,W-84,10,"#55574f",14)
        c.setStrokeColor(HexColor("#d3d0c7"));c.line(42,57,W-42,57)
        c.setFillColor(HexColor("#5b5e55"));c.setFont("Helvetica",8.5)
        c.drawString(42,40,"Tutorials and original DSP sound examples: bonszaimusic.github.io/caravel/")
        c.drawRightString(W-42,40,f"STASIS  /  {number}")
        c.linkURL("https://bonszaimusic.github.io/caravel/tutorials/",(42,33,W-42,50),relative=0)
        c.showPage()
    c.save()
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    model={"version":"0.6.0","pdf":OUT.name,"pdf_sha256":sha(OUT),
           "chapters":[{"number":n,"title":t,"image":im,"image_sha256":sha(ROOT/"docs/tutorials/img"/im),
                        "tag":tag,"steps":[{"title":a,"text":b} for a,b in steps],"tip":tip,"note":note}
                       for n,t,im,tag,steps,tip,note in PAGES]}
    (OUT.parent/"stasis-0.6.0.json").write_text(json.dumps(model,indent=2)+"\n")
    print(OUT)

if __name__=="__main__":main()
