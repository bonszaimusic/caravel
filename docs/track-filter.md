<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# FILTER: a low-pass / high-pass for every track

**Scope:** per track, on every engine. Hardware acceptance on a real FM-1 is pending; the behaviour below is covered by host tests
(`tests/trackfilter_test.c` and the tests that changed with it), which are not hardware measurements.

The track's FILTER is **one knob**: turn it left for a **low-pass** (LP), right for a **high-pass** (HP), and leave it in the middle for **OFF**.
It is the **FILT** card of the **INSERT** view in the FX family (the FX button, to INSERT; the bottom group, KNOB 4), next to INS A, INS B and
INS C. It works like the FILTER knob of the FX layer, but it belongs to one track and stays on.

| FILT | what you hear | FILT | what you hear |
| --- | --- | --- | --- |
| **OFF** (0) | the track as it is (nothing runs) | | |
| LP 1 % (-1) | a trace of dulling, -3 dB at 14 kHz | HP 1 % (+1) | a trace of bass taken away, -3 dB at 32 Hz |
| LP 25 % (-16) | -3 dB at 4.6 kHz | HP 25 % (+16) | -3 dB at 103 Hz |
| LP 50 % (-32) | -3 dB at 1.4 kHz | HP 50 % (+32) | -3 dB at 353 Hz |
| LP 75 % (-48) | -3 dB at 400 Hz | HP 75 % (+48) | -3 dB at 1.2 kHz |
| LP 100 % (-64) | -3 dB at 120 Hz | HP 100 % (+63) | -3 dB at 3.8 kHz |

The card shows **OFF**, or the share (1 to 100) with **LP** or **HP** under it. The cutoffs lie an octave apart every seven steps of the
range (30 Hz x 2^(row / 7), as the layer's), so equal turns are equal musical intervals. The slope is 12 dB per octave and there is **no resonance**:
the filter cannot ring or howl, at any setting.

## Where it sits

The FILTER is the **last stage of the track's own chain**:

voices of the engine -> **DIST** -> **INSERT** -> **SLICER** -> **FILTER** -> LEVEL, PAN and the sends (DLY, REV, CHO)

So the **sends take the filtered sound** (a high-passed track sends a high-passed signal into the delay and the reverb), a SLICER's gate is
filtered after it chops, and an INSERT's harmonics (a CRUSH, a FUZZ) are filtered away as you would expect. The engine does not matter: FM6,
GRAIN, PHYS, WHEEL, SLICE and the others have no filter of their own and get one. With **SPREAD** the left, right and side signals are all
filtered alike. The one exception is **DRUM-X**: its per-lane stereo IMAGE and per-lane SEND parts are added after the strip (as they already pass DIST,
the INSERT and the SLICER by); the rest of its sound is filtered.

## Smooth, and nothing when it is off

- Turning the knob moves the cutoff at most the whole range in about **48 ms** (a step a 32-sample block), with no zipper noise; a knob jump,
  an automation step or a CC sweep all glide.
- From OFF the filter fades in over 2.9 ms with the cutoff open; **back to OFF** it first opens, then fades out over 2.9 ms and clears itself. Turning from
  LP to HP (or the other way) does the same through the middle. About 51 ms from the lowest LP to OFF.
- At **OFF with the filter faded out nothing runs at all** and the track is **bit for bit** what it was before the filter existed.
- The cost is a small fixed amount per track, whatever the settings: about 3.5 % of an 8-voice ANALOG part on the Mac (host instructions per sample;
  not a measurement of the FM-1's CPU).

## What else it is

- **Automation, locks and MIDI:** FILT is a parameter like the sends: record it as automation (a filter sweep over a bar), hold a lock on a step,
  or learn a CC to it (MIDI LEARN: CC 0 is full LP, 127 full HP, the middle OFF).
- **FX HOLD:** the filter belongs to the track's FX. A factory preset load sets it OFF, **unless FX HOLD is ON**, when it stays (MENU > VOICES,
  or the INSERT group's third card, KNOB 3). A user preset stores the filter and loads its own. SOUND INIT, CLEAN INIT, project INIT and power-on start OFF.
  See [Loading sounds and the track's FX](menus-and-pages.md#loading-sounds-and-the-tracks-fx).
- **Not a modulation destination:** the MOD matrix, ENV DEST and LFO DEST do not list it in this version.
- **Projects, presets and the editor:** see below.

## Saved data: what older firmware does

FILT is the 114th track parameter, stored just before the engine values (parameter id **105**; the engine values moved from 105..112 to 106..113, P_COUNT
113 -> 114). Every store maps by count, so **anything saved by 0.2.0 or 0.1.0 loads with the filter OFF** and everything else in place: projects (FUN10,
also their automation), user presets, backups, and the web editor's library files.

The other way round, with **0.2.0 / 0.1.0** (checked with their own sources; host only):

| What 0.3.0 wrote | What 0.2.0 / 0.1.0 do with it |
| --- | --- |
| a **project** (FUN10; byte 66 = 114) | refuse it (more parameters than their own): the slot shows **EMPTY**; the flash copy stays until something is saved over it, a backup PUT of one answers rc 1. Every project saved by 0.3.0 does this, whether or not it uses the filter |
| a **user preset** | loads by count: the engine values in place, the filter dropped |
| the settings (MIDI LEARN of FILT) | the entry is ignored there |
| the web editor | an editor of 0.2.0 does not know the layout (114 / 106); the editor served with 0.3.0 does |

This is why the version is **0.3.0**, not 0.2.1: a project saved by it cannot be opened by the version before. FUN10 itself did not change (3840 bytes);
the data now ends four bytes before the patches, MIDI IN TRACK's three bytes take the end of them, and **one spare byte is left**. 114 parameters is also
the most the automation format has ids for (ids 114 .. 127 are the FX layer's), so a further common track parameter needs a new project format.
