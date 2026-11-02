#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Draw the timeline of 'a RANDOM morph holds only its own track's journey' for docs/tutorials/img/journey-hold.png.
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
text((40,28),"Every track has its own engine: a RANDOM morph holds only its own journey",TEXT,FT)
text((40,72),"T1 and T2 both run a journey. T1 starts its own RANDOM morph while its journey runs. Scope TRACK on both.",DIM,FS)
x0,x1=250,1240;lanes=[("JOURNEY (T1)",150),("RANDOM (T1)",250),("JOURNEY (T2)",350),("T1 controls",450),("T2 controls",550)]
for name,y in lanes:
    text((40,y+22),name,DIM,F);d.line((x0,y+70,x1,y+70),fill=LINE,width=1)
a,b,c,e,f=x0,470,690,980,x1                       # a: journeys start, b: T1 RANDOM starts, c: T1 morph arrives, e: T1 journey arrives, f: end
def block(x,xe,y,label,fill,fg=BG,outline=None):
    d.rounded_rectangle((x,y+8,xe,y+56),8,fill=fill,outline=outline);text(((x+xe)//2,y+32),label,fg,FS,"mm")
block(a,b,150,"leg > S1",AMB)
d.rounded_rectangle((b,158,c,206),8,outline=RED,width=3);text(((b+c)//2,182),"HELD",RED,F,"mm")
block(c,e,150,"leg > S1 goes on",AMB)
block(e,f,150,"WAIT, > S2",BOX,TEXT,LINE)
block(b,c,250,"T1 morph",CYAN)
block(a,600,350,"leg > S1",AMB);block(600,860,350,"WAIT",BOX,TEXT,LINE);block(860,f,350,"leg > S2",AMB)
def poly(y,pts,col):
    d.line([(px,y+60-py) for px,py in pts],fill=col,width=4)
poly(450,[(a,5),(b,28)],AMB);poly(450,[(b,28),(b+60,40),(b+130,18),(c,30)],CYAN);poly(450,[(c,30),(e,50),(f,50)],AMB)
poly(550,[(a,5),(600,50),(860,50),(f,12)],AMB)
for x in (b,c):d.line((x,128,x,640),fill=LINE,width=1)
text((b,112),"1  T1: A1 RANDOM",CYAN,FS,"ma");text((c,112),"2  T1 morph is done",CYAN,FS,"ma")
text((40,660),"1  T1's journey stops where it stands: HELD, not ended. T2's journey and morphs are not touched.",TEXT,FS)
text((40,690),"2  When the T1 morph has arrived, T1's journey goes on by itself: a new leg from T1's live values to the same slot.",TEXT,FS)
text((40,720),"A1 while HELD is PAUSE: the journey then waits for A1 again. A control two tracks move belongs to the track that started last.",DIM,FS)
out=ROOT/"docs/tutorials/img/journey-hold.png";im.save(out,optimize=True);print("wrote",out)
