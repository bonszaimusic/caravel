<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Pages, groups and the settings menu

Caravel shows more of a page at once and changes a few gestures. Hardware acceptance on a real FM-1 is pending.

## Two parameter groups on one screen

These pages show two groups of four parameters at once instead of one page and a graph:

| Top group | Bottom group | Header texts |
| --- | --- | --- |
| LFO | LFO SYNC | LFO SHAPE / RATE · LFO SYNC / TRIGGER |
| LFO2 | LFO2 SYNC | LFO2 SHAPE / RATE · LFO2 SYNC / TRIGGER |
| ENV | ENV DEST | ENV · ENV DEST / SYNC |
| ARP | ARP 2 | ARP MODE / RATE · ARP SWING / ORDER |
| VOICE | VOICE 2 | VOICE MODE / GLIDE · VOICE ALLOC / PAN |
| EDIT 1 | EDIT 2 | EDIT 1 · EDIT 2 |
| LANES (DRUM only) | LANES 2 | LANES 1-4 · LANES 5-8 |
| FX | DLY | FX SENDS · DELAY |
| REVERB | CHORUS | REVERB · CHORUS |
| INSERT | INSERT 2 | INSERT TYPE / MIX (with the type's curve) · INSERT A / B / C |
| SLICER | (its graph) | SLICER · the step pattern and what it does to a tone |

The active group is framed in the theme colour and marked **EDIT K1-4**; KNOB 1..4 edit it. The other group stays readable and is marked
VIEW. A page's graph (the LFO scope, the ADSR graph, the effect pictures) is not drawn on these pages; instead a small pictogram under the
WAVE value shows the selected LFO wave (SIN, TRI, SAW, SQR, S&H), and a small ADSR shape beside the ENV title follows ATK, DEC, SUS and REL.
Both work in normal and LARGE text sizes (the WAVE value stays in the medium face so the pictogram fits).

**EDIT 1 / 2 keep their normal layout** for the engines whose page is a picture of the sound: SAMPLE (the waveform), WHEEL (the drawbars)
and FM6 (the algorithm chart); on every other engine they use the two-group layout and lose only the oscilloscope.

**The footer** shows the view and its number in its family: **LFO TOP 1/4**, **LFO BOTTOM 1/4**, **LFO DEST 2/4**, **LFO2 TOP 3/4**,
**MOD 4/4**. The two groups of a pair count as one view; a family with one view shows no number (ENV TOP, ARP TOP). The FX family order is
FX, DLY, REVERB, CHORUS, INSERT, INSERT 2, SLICER, so each pair is one view: FX SENDS 1/4, REVERB 2/4, INSERT 3/4, SLICER 4/4.

**Who a group is for:** every group's header carries a small tag: **ALL** (accent colour) for the global pages (DELAY, REVERB, CHORUS), **T1** to
**T4** for a page of the selected track (the FX sends, INSERT, SLICER, the LFOs, ENV, EDIT, ...). Changing the track changes the T groups;
the ALL groups stay as they are.

**INSERT** shows TYPE and MIX in the upper group, with the curve of the chosen type beside them (a drive's transfer curve, the folder, the
bit staircase, the swept effects' sweep); the type's own A, B and C are the lower group. **SLICER** keeps its four controls in the upper
group; the lower block is a picture of the settings, not audio: the 16 steps of the pattern in groups of four with the playing step marked,
and below them a tone as the SLICER shapes it (GATE: a closed step as loud as DEPTH leaves it; STUT: the repeated piece at DEPTH).

## SELECT

MENU > CONTROL > SELECT chooses what the SELECT knob does: BPM, PAGE, MODW or VEL. On a device without saved settings it starts on
**PAGE**; saved settings, including BPM, are kept. With PAGE, a turn to the right visits the top group, the bottom group, then the next
page; a turn to the left reverses it. The family buttons (for example LFO) still step through the same positions. GLO + SELECT sets the tempo.

**In the settings menu** (HOME held), SELECT on PAGE walks the rows one detent at a time and continues into the next tab (and wraps at the
ends); each tab keeps the row it was left at. Other SELECT modes do nothing in the menu, as before.

## MORE

The EDIT family has one **MORE** page after VOICE 2 with the single parameters that used to have a page each: KNOB 1 **SPRD** (SPREAD, every
engine), KNOB 2 **MORPH** (ANALOG, PHASE, LOFI and the DRUM-X kit), KNOB 3 **PHS FB** (PHASE). A parameter the engine does not have leaves its
card empty and its knob does nothing.

On **ANALOG**, MORE is the lower group of a view with **FILTER** above it (TYPE, CUT, RES, KTR), marked FILTER 3/3 in the footer. On **SEM** the
page is not offered: SEM MODE already has SPREAD and the other two parameters do not exist there.

## EDIT and CLEAR STEP on the STEP page

**EDIT opens the EDIT pages** from the STEP page, like on every other page. With a note key held (the step being entered), EDIT clears that
step's locks. On the DETAIL page EDIT still resets the step's fields.

**CLEAR STEP:** hold **SEQ** and press **F5**. It clears the cursor step and its locks and moves the cursor on, so repeated presses sweep
through the pattern; an empty step is passed with "STEP IS EMPTY". One undo (SAVE held). The SEQ TOOLS map shows a **CLR STEP** cell on melodic
tracks and a small **CLR** cell at the end of the lane row on DRUM tracks. F3 still clears the whole sequence.
