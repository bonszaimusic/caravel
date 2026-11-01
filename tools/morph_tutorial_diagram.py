#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Draw the timeline of 'a RANDOM morph on another track holds the journey' for docs/tutorials/img/journey-hold.png.
Run from the repository root: python3 tools/morph_tutorial_diagram.py   (needs Pillow)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parents[1]
W,H=1280,760
BG=(16,12,8);TEXT=(247,239,222);DIM=(176,160,128);AMB=(255,170,41);CYAN=(88,200,216);GREEN=(120,200,120);RED=(224,96,80);BOX=(41,32,16);LINE=(84,65,31)
im=Image.new("RGB",(W,H),BG);d=ImageDraw.Draw(im)
def font(n,bold=False):
    return ImageFont.load_default(size=n)
F=font(20);FS=font(16);FB=font(26);FT=font(32)
def text(xy,s,fill=TEXT,f=F,anchor="la"):d.text(xy,s,fill=fill,font=f,anchor=anchor)
text((40,28),"One morph engine: a RANDOM morph on another track holds the journey",TEXT,FT)
text((40,72),"The journey belongs to T1. T2 starts its own RANDOM morph while the journey runs. Scope TRACK on both.",DIM,FS)
x0,x1=250,1240;lanes=[("JOURNEY (T1)",150),("RANDOM (T2)",250),("T1 controls",350),("T2 controls",450),("Status line",550)]
for name,y in lanes:
    text((40,y+22),name,DIM,F);d.line((x0,y+70,x1,y+70),fill=LINE,width=1)
# time marks
a,b,c,e,f=x0,470,690,980,x1                       # a: journey starts, b: T2 RANDOM starts, c: T2 morph arrives, e: journey arrives, f: end
def block(x,xe,y,label,fill,fg=BG,outline=None):
    d.rounded_rectangle((x,y+8,xe,y+56),8,fill=fill,outline=outline);text(((x+xe)//2,y+32),label,fg,FS,"mm")
block(a,b,150,"leg > S1 (PLAY)",AMB)
d.rounded_rectangle((b,158,c,206),8,outline=RED,width=3);text(((b+c)//2,182),"HELD",RED,F,"mm")
block(c,e,150,"leg > S1 goes on by itself",AMB)
block(e,f,150,"WAIT, then > S2",BOX,TEXT,LINE)
block(b,c,250,"T2 morph",CYAN)
# controls: T1 rises, stands still during the hold, rises again; T2 flat, moves, flat
def poly(y,pts,col):
    d.line([(px,y+60-py) for px,py in pts],fill=col,width=4)
poly(350,[(a,5),(b,28),(c,28),(e,50),(f,50)],AMB)
poly(450,[(a,20),(b,20),(c,45),(f,45)],CYAN)
for x in (b,c):d.line((x,128,x,640),fill=LINE,width=1)
for x,l1,l2,col in ((a,"JOURNEY T1","PLAY",GREEN),(b+8,"JOURNEY T1","HELD",RED),(c+8,"JOURNEY T1","PLAY",GREEN)):
    text((x,540),l1,col,FS);text((x,564),l2,col,FS)
# notes
text((b,112),"1  T2: A1 RANDOM",CYAN,FS,"ma");text((c,112),"2  T2 morph is done",CYAN,FS,"ma")
text((40,630),"1  The journey stops where T1 stands: it is HELD, not ended. T1's controls do not move; T2's do.",TEXT,FS)
text((40,660),"2  When the T2 morph has arrived the journey goes on by itself: a new leg from T1's live values to the same slot, with the full BARS length.",TEXT,FS)
text((40,690),"A1 while HELD is PAUSE: the journey then waits for A1 again. Saving or deleting a snapshot ends it. A2 FREEZE on T2 keeps it held.",DIM,FS)
text((40,720),"There is only one morph at a time: two morphs never run side by side.",DIM,FS)
out=ROOT/"docs/tutorials/img/journey-hold.png";im.save(out,optimize=True);print("wrote",out)
