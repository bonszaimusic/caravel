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
| FX | INSERT | FX SENDS · INSERT TYPE / A B C |

The active group is framed in the theme colour and marked **EDIT K1-4**; KNOB 1..4 edit it. The other group stays readable and is marked
VIEW. A page's graph (the LFO scope, the ADSR graph, the effect pictures) is not drawn on these pages; instead a small pictogram under the
WAVE value shows the selected LFO wave (SIN, TRI, SAW, SQR, S&H), and a small ADSR shape beside the ENV title follows ATK, DEC, SUS and REL.
Both work in normal and LARGE text sizes (the WAVE value stays in the medium face so the pictogram fits).

**EDIT 1 / 2 keep their normal layout** for the engines whose page is a picture of the sound: SAMPLE (the waveform), WHEEL (the drawbars)
and FM6 (the algorithm chart); on every other engine they use the two-group layout and lose only the oscilloscope.

**The footer** shows the view and its number in its family: **LFO TOP 1/4**, **LFO BOTTOM 1/4**, **LFO DEST 2/4**, **LFO2 TOP 3/4**,
**MOD 4/4**. The two groups of a pair count as one view; a family with one view shows no number (ENV TOP, ARP TOP). The FX family order is
FX, INSERT, INSERT 2, SLICER, DLY, REVERB, CHORUS, so FX and INSERT are neighbours.

## Loading sounds and the track's FX

A track's own FX are four sends (DST, CHO, DLY, REV, on the FX page) and its INSERT (TYPE and A B C, MIX). The delay, reverb and
chorus themselves (DLY, REVERB, CHORUS pages) are global: one set for the whole project.

What a sound load does (a factory preset on PRESETS or EDIT KNOB 2, a user preset, an engine change, a MIDI program change, the editor):

| | FX HOLD OFF (default) | FX HOLD ON |
| --- | --- | --- |
| Sends DST CHO DLY REV | the preset's, or the usual 0 / 24 / 28 / 36 if it has none | stay as they are |
| INSERT (TYPE, A B C, MIX) | OFF and its defaults | stays as it is |
| SLICER, ARP, SCL, LEN / DIV / SWG / GATE, LEVEL / PAN / MUTE | stay | stay |
| Global DLY / REVERB / CHORUS, tempo, tuning | never touched | never touched |
| Everything else of the sound (engine values, ENV, LFOs, matrix, ...) | the preset's or default | the preset's or default |

**MENU > VOICES > FX HOLD** (HOME held, ALGORITHM to VOICES, the last row) switches it; it is a device setting kept with the other
settings, not part of a project. A **user preset** still stores the track's FX when you save it, but under FX HOLD ON loading it does not
apply them. **SOUND INIT**, **CLEAN INIT**, a new project (INIT) and power-on start from scratch whatever FX HOLD says: SOUND INIT gives
the engine's first preset with its sends, CLEAN INIT no sends and INSERT OFF. Loading a **project** (or LIVE LOAD FULL) restores the FX it
saved. SAVE held (undo) after a load still gives back the sound as it was. Automation and locks on the sends are not touched by FX HOLD.

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

## EDIT and CLEAR STEP on the STEP page

**EDIT opens the EDIT pages** from the STEP page, like on every other page. With a note key held (the step being entered), EDIT clears that
step's locks. On the DETAIL page EDIT still resets the step's fields.

**CLEAR STEP:** hold **SEQ** and press **F5**. It clears the cursor step and its locks and moves the cursor on, so repeated presses sweep
through the pattern; an empty step is passed with "STEP IS EMPTY". One undo (SAVE held). The SEQ TOOLS map shows a **CLR STEP** cell on melodic
tracks and a small **CLR** cell at the end of the lane row on DRUM tracks. F3 still clears the whole sequence.
