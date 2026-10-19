<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# DRUM-X

Synthesized drum kits. **Origin:** the synthesis and the factory A/B sounds are adapted from **JIANT** by juanjiant-bit (GPL-3.0-only, itself a
Felucca fork; JIANT's DRUM-X follows the idea of Sonic Charge's Microtonic). Caravel adds track-local kits, fixed MIDI Learn targets and compatible
storage. Not affiliated with Sonic Charge. Hardware acceptance on a real FM-1 is pending.

## Selecting and sound

Select the DRUM preset **DRUM-X**, or KIT **DRUM-X**. Eight GM / grid lanes each have two patches, A and B, with pitch, pitch modulation, decay,
noise and colour (the oscillator's FM index, or its drive for SINE, and the noise filter's cutoff). **MORPH** blends the two patches while hits ring.
MODE selects the oscillator (SINE, FM, METAL, BELL), the noise filter (LP, BP, HP) and the envelope combinations; PM.TYPE, per-lane drive, layer,
feedback and chorus / delay / reverb sends shape each voice. Global **BP**, **feedback**, stereo **IMAGE** and the tempo-related **X-MOD** add
movement; X-MOD's rate follows BPM and its phase runs free.

## Pages

EDIT adds **X KIT**, **X-MOD**, **X SOUND**, **X SOUND 2**, **X MODE** and **X LAYER**. On X SOUND, KNOB 1 selects the lane and KNOB 2 selects A/B.

## MIDI Learn, locks and automation

Every sound control can be MIDI Learned on the device or assigned in the matching editor. Up to 64 assignments are stored; each stays bound to its
exact track, lane and A/B field. Existing reserved MIDI CCs keep their functions. The main kit parameters and MORPH support step locks and
automation; the additional per-lane and X-MOD controls support MIDI Learn and saved edits, not sequencer locks or automation.

## Saving and compatibility

Projects containing DRUM-X use the FUNB format; DRUM-X user presets use versions 6 and 7 with a tagged engine-specific payload. Other sounds
keep their formats and FM6 tracks keep their own patches. Settings use PER5 and import PER1..PER4, including old MIDI assignments. Use Caravel (or
newer) for DRUM-X saves; Felucca cannot load them. The editor's library capture, audition, import / export and direct device-slot write carry the
complete A/B kit. LIVE LOAD FULL restores the kits.
