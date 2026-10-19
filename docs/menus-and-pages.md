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
| LFO DEST | MOD | LFO AMOUNTS · MOD MATRIX (SLOT, SRC, DST, AMT; four pips in the header show which slots are in use) |
| ENV | ENV DEST | ENV · ENV DEST / SYNC |
| ARP | ARP 2 | ARP MODE / RATE · ARP SWING / ORDER |
| VOICE | VOICE 2 | VOICE MODE / GLIDE · VOICE ALLOC / PAN |
| EDIT 1 | EDIT 2 | EDIT 1 · EDIT 2 |
| LANES (DRUM only) | LANES 2 | LANES 1-4 · LANES 5-8 |
| FX | DLY | FX SENDS · DELAY |
| REVERB | CHORUS | REVERB · CHORUS |
| INSERT | INSERT 2 | INSERT TYPE / MIX (with the type's curve) · INSERT A / B / C |
| SLICER | (its graph) | SLICER · the step pattern and what it does to a tone |
| SCL | (its keys) | SCALE · the 12 keys: the scale lit, the root in the accent colour |
| CHORD | (its keys) | CHORD KEYS · the chord each scale note plays (DIA3, DIA7) with the last one marked, and its notes on the keys |

The active group is framed in the theme colour and marked **EDIT K1-4**; KNOB 1..4 edit it. The other group stays readable and is marked
VIEW. A page's graph (the LFO scope, the ADSR graph, the effect pictures) is not drawn on these pages; instead a small pictogram under the
WAVE value shows the selected LFO wave (SIN, TRI, SAW, SQR, S&H), and a small ADSR shape beside the ENV title follows ATK, DEC, SUS and REL.
Both work in normal and LARGE text sizes (the WAVE value stays in the medium face so the pictogram fits).

**EDIT 1 / 2 keep their normal layout** for the engines whose page is a picture of the sound: SAMPLE (the waveform), WHEEL (the drawbars)
and FM6 (the algorithm chart); on every other engine they use the two-group layout and lose only the oscilloscope.

**The footer** shows the view and its number in its family: **LFO TOP 1/3**, **LFO BOTTOM 1/3**, **LFO2 TOP 2/3**, **LFO DEST 3/3**,
**MOD 3/3**. The two groups of a pair count as one view; a family with one view shows no number (ENV TOP, ARP TOP). The FX family order is
FX, DLY, REVERB, CHORUS, INSERT, INSERT 2, SLICER, so each pair is one view: FX SENDS 1/4, REVERB 2/4, INSERT 3/4, SLICER 4/4.

**Who a group is for:** every group's header carries a small tag: **ALL** (accent colour) for the global pages (DELAY, REVERB, CHORUS), **T1** to
**T4** for a page of the selected track (the FX sends, INSERT, SLICER, the LFOs, ENV, EDIT, ...). Changing the track changes the T groups;
the ALL groups stay as they are.

**SCL, CHORD and SLICER** are one group above and a picture of the settings below (the scale on the keys, the last chord played, the step
pattern).

**Every other page with a card row** (HOME, MIXER, CLOCK, SEQ with the piano roll, the DRUM grid, DETAIL and AUTOMATION, the sound and project pages, the engine pages
with a picture) keeps its small cards and its full-height panel; the card row is framed in the theme colour, as the active group of a two-group
page is, to say that KNOB 1-4 edit it. The LINE style (MENU > STYLE) has no frame: its rules do that job. The layers' maps and the name entry have no card row
and no frame.

**INSERT** shows TYPE and MIX in the upper group, with the curve of the chosen type beside them (a drive's transfer curve, the folder, the
bit staircase, the swept effects' sweep); the type's own A, B and C are the lower group. **SLICER** keeps its four controls in the upper
group; the lower block is a picture of the settings, not audio: the 16 steps of the pattern in groups of four with the playing step marked,
and below them a tone as the SLICER shapes it (GATE: a closed step as loud as DEPTH leaves it; STUT: the repeated piece at DEPTH).

## Loading sounds and the track's FX

A track's own FX are four sends (DST, CHO, DLY, REV, on the FX page) and its INSERT (TYPE and A B C, MIX). The sends say how much of the
track goes to the one shared delay, reverb and chorus (DLY, REVERB, CHORUS pages, global for the whole project); DST is the track's own
distortion. The INSERT is the track's own too.

What a sound load does:

| | factory preset (PRESETS, EDIT KNOB 2, engine change, MIDI program change, editor) | user preset |
| --- | --- | --- |
| Sends DST CHO DLY REV | FX HOLD OFF (default): the preset's, or the usual 0 / 24 / 28 / 36 if it has none. FX HOLD ON: stay as they are | the saved ones |
| INSERT (TYPE, A B C, MIX) | stays (no factory preset has one) | the saved one |
| SLICER, ARP, SCL, LEN / DIV / SWG / GATE, LEVEL / PAN / MUTE | stay | stay |
| Global DLY / REVERB / CHORUS, tempo, tuning | never touched | never touched |
| Everything else (engine values, ENV, LFOs, matrix, ...) | the preset's or default | the saved |

**FX HOLD** is the fourth card (KNOB 4) of the INSERT A / B / C group in the FX family, and the last row of MENU > VOICES (HOME held, ALGORITHM to VOICES):
both switch the same setting. It is a device setting kept with the other settings, not part of a project and not of the track the group belongs to,
although the group's tag says T1 to T4. **SOUND INIT**, a new project (INIT) and power-on start from scratch whatever FX HOLD says: the engine's
first preset with its sends, INSERT OFF. **CLEAN INIT** clears the sends and the INSERT. Loading a **project** (or LIVE LOAD FULL) restores
the FX it saved. SAVE held (undo) after a load gives back the sound as it was. Automation and locks on the sends are not touched.
Before the FX HOLD change a factory preset load switched the INSERT OFF.

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

## LATCH on the CHORD page

The CHORD page (SCL family, after SCL) has a third parameter, **LATCH** (OFF, NEW, STACK), KNOB 3. It keeps the notes of the keys and of MIDI
IN sounding after they are released; see [LATCH](latch.md). **OCT- and OCT+ pressed together** still reset the octave to 0, and now also let
go of everything the selected track has latched ("LATCH RELEASED").
