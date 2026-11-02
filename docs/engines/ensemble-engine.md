# ENSEMBLE — strings, choir and dark texture with a built-in ensemble

ENSEMBLE is an original string-machine / choir / dark-pad engine by bonszaimusic (Caravel). It takes the idea of sawtooth registers played into a
slowly moving ensemble; it is not an emulation of any instrument or circuit. **Status: part of the prepared Caravel 0.10.0 release (branch `release/0.10.0`, not published).
Everything below is verified on the host only (real firmware code in the host harness); it has not been tried on an FM-1, and nobody has judged how it sounds.**

## Selecting ENSEMBLE

Select **ENSEMB** in PRESETS or with EDIT held and KNOB 1 (it is the last engine in the list). ENSEMB is the engine's name on the display (the PRESETS cell is too narrow for the full word: the UI lint cuts `ENSEMBLE` to `ENSEM…`); this guide and the code call it ENSEMBLE. Engine ID 17 identifies it; no existing engine number
changed. A project that holds engine 17 is not read by older firmware: **0.9.0 shows that slot as empty** (host test with real flash images: the slot reads as unused, a
load says nothing, nothing crashes; saving over it there replaces it, and a firmware that has ENSEMBLE reads it again). Versions before 0.9.0 were not tested, assume the same.

## Sound and controls

- **Registers:** a triangle one octave down (16'), a band-limited saw (8') and a saw one octave up (4'), locked to the same phase.
- **One filter, three timbres:** a trapezoidal state-variable filter whose cutoff, damping and low-pass / band-pass mix follow **MORPH**:
  STRINGS (open low-pass, TONE sets the brightness) to CHOIR (a resonant formant band, breath noise, TONE moves the vowel) to DARK
  (a darker low-pass with a rough ring-modulated pulse and a harder knee).
- **Ensemble:** three delay taps (6.0, 8.5 and 11.0 ms at rest) read from one ring and moved by a slow and a fast LFO, 120 degrees apart. One ensemble per
  track, applied to the track's mix: its cost grows with the number of tracks on this engine, not with the number of voices.

| Page | Control | What it does |
| --- | --- | --- |
| EDIT 1 / TIMBRE | **REG** | register mix: 0 is the 8' alone, 64 adds the 4', 127 adds the 16' as well |
| | **MORPH** | 0 strings, 64 choir, 127 dark; the steps in between blend smoothly |
| | **TONE** | brightness of the strings and dark timbres; the vowel (formant position) of the choir. MIDI CC 74 |
| | **DRIFT** | slow random pitch drift and a fixed per-voice detune, up to about +-7 cents |
| EDIT 2 / ENSEMBLE | **ENS** | depth of the taps' movement (0 is a static three-tap comb) |
| | **RATE** | speed of the slow (0.2 to 1.6 Hz) and the fast (6 to 8.6 Hz) LFO |
| | **TEXT** | roughness: harder knee, more ring-modulation at the dark end, more breath in the choir |
| | **MIX** | ensemble against dry: 0 is the dry voices, bit for bit, whatever ENS and RATE say |

HOME shows **MORPH, ENS, TONE, MIX**. All eight controls are editable, MIDI-learnable, automatable, morph-safe (Random Morph, snapshots and journeys move
them like any engine control) and stored in projects and user presets.

## Factory presets

Fourteen presets, in the stored order (a favorite or a project addresses a preset by its number, so new ones only go to the end). **The values below are design
intent: what each preset was set up to do. The numbers beside them are measures of the signal from the host harness, not a verdict on how anything sounds; nobody has
listened to them on an FM-1.** Category (PRESETS list filter): PAD, except GLASS SHIMMER (PLUCK), ROUGH PULSE (FX) and SOLO DRIFT (LEAD).

| Preset | Idea |
| --- | --- |
| VINTAGE STRINGS | open saw registers, slow ensemble |
| GLASS CHOIR | the choir end of MORPH, airy, a little faster ensemble (the DRONE engine has a preset of the same name) |
| DUSK ORCHESTRA | strings leaning to the choir, all registers |
| ABYSS PAD | the dark end, rough, slow, a long release |
| PROFILE A | controls set by measurement to the profile of a reference recording (see below); a soft attack and a medium release are this table's choice; no audio from the recording is stored anywhere |
| STRING MACHINE | the classic: bright saw registers, a deep slow ensemble, TONE full, a soft attack |
| BREATH CHOIR | the choir formant with a lot of breath (TEXT), quieter than the strings (the band-pass path is about 7 dB lower) |
| DEEP DRONE | the lowest and darkest: 16' register, TONE closed, a 1.3 s attack and a 3 s release; for notes in the bass |
| GLASS SHIMMER | a struck glass / bell tone: MORPH DARK with TEXT full (the ring-modulated pulse is inharmonic), TONE open, little ensemble, a decaying envelope |
| SLOW SWELL | strings that open and swell over about four seconds (ENV > FILTER 40, a long attack) and let go slowly |
| ROUGH PULSE | short and rough: MORPH DARK, TEXT full, a fast RATE, a short release; DIST send 10 |
| SOLO DRIFT | MONO (legato), DRIFT full: a single drifting voice for a line |
| HUGE ENSEMBLE | the deepest ensemble: ENS and MIX full, the slowest RATE, bright strings |
| RUNNING PAD | a pad with the fastest RATE: the fast LFO of the ensemble at about 8.6 Hz and the slow one at 1.6 Hz (VINTAGE STRINGS: 7 Hz and 0.8 Hz) |

Controls and measures (REG / MORPH / TONE / DRIFT / ENS / RATE / TEXT / MIX; ATK / DEC / SUS / REL). The measures are of the engine alone (sends 0, DIST, FILTER and
PAN neutral): E2 B2 E3 G3 held 10 s (the "low" chord; the second centroid is the "high" chord E3 G3 B3 E4); *centroid* is the spectral centroid of the settled part,
*bands* the energy shares in dB below 200 Hz / up to 1 kHz / up to 4 kHz / above, *level* the RMS in dB of full scale, *attack* the time to 80 % of the settled
level and *release* the time to -30 dB after the keys are released.

| Preset | REG / MORPH / TONE / DRIFT / ENS / RATE / TEXT / MIX | ATK / DEC / SUS / REL | centroid, Hz (low / high) | bands, dB (low chord) | level, dB | attack / release, s |
| --- | --- | --- | --- | --- | --- | --- |
| VINTAGE STRINGS | 64 / 0 / 80 / 40 / 70 / 50 / 10 / 100 | 60 / 90 / 112 / 80 | 487 / 634 | -5.2 / -2.2 / -10.4 / -24.9 | -20.8 | 0.05 / 0.26 |
| GLASS CHOIR | 40 / 64 / 60 / 50 / 60 / 40 / 15 / 90 | 80 / 90 / 115 / 95 | 645 / 740 | -7.1 / -2.0 / -7.4 / -34.4 | -29.2 | 0.29 / 0.76 |
| DUSK ORCHESTRA | 100 / 40 / 70 / 60 / 85 / 35 / 30 / 100 | 75 / 95 / 115 / 90 | 573 / 778 | -5.9 / -2.4 / -7.7 / -29.1 | -23.8 | 0.19 / 0.54 |
| ABYSS PAD | 80 / 110 / 40 / 90 / 75 / 20 / 90 / 95 | 100 / 100 / 120 / 105 | 361 / 498 | -5.3 / -1.6 / -18.5 / -49.2 | -21.4 | 1.03 / 1.44 |
| PROFILE A | 123 / 15 / 70 / 42 / 55 / 71 / 31 / 126 | 85 / 90 / 112 / 95 | 404 / 610 | -5.0 / -2.0 / -12.5 / -29.6 | -20.5 | 0.35 / 0.76 |
| STRING MACHINE | 64 / 0 / 127 / 25 / 100 / 22 / 0 / 127 | 85 / 90 / 115 / 95 | 664 / 943 | -8.6 / -1.5 / -8.9 / -17.1 | -20.2 | 0.35 / 0.74 |
| BREATH CHOIR | 64 / 64 / 90 / 35 / 40 / 45 / 127 / 100 | 90 / 95 / 115 / 100 | 796 / 924 | -7.9 / -3.0 / -4.8 / -28.6 | -27.6 | 0.53 / 1.06 |
| DEEP DRONE | 127 / 0 / 0 / 60 / 80 / 8 / 0 / 110 | 105 / 100 / 120 / 115 | 249 / 404 | -3.0 / -3.1 / -23.6 / -55.1 | -20.5 | 1.27 / 3.24 |
| GLASS SHIMMER | 64 / 127 / 127 / 15 / 10 / 60 / 127 / 100 | 55 / 70 / 60 / 100 | 587 / 719 | -6.8 / -1.8 / -8.9 / -20.9 | -25.3 | 0.03 / 1.06 |
| SLOW SWELL | 90 / 10 / 70 / 40 / 80 / 10 / 0 / 100 | 120 / 100 / 120 / 112 (ENV>FILTER 40) | 642 / 766 | -5.2 / -2.6 / -9.1 / -16.5 | -22.2 | 4.45 / 2.48 |
| ROUGH PULSE | 127 / 127 / 110 / 50 / 70 / 110 / 127 / 127 | 50 / 70 / 70 / 60 | 528 / 767 | -3.7 / -3.3 / -10.4 / -17.4 | -22.9 | 0.03 / 0.22 |
| SOLO DRIFT | 70 / 20 / 95 / 127 / 25 / 60 / 0 / 100 | 60 / 90 / 112 / 70 (MONO) | 625 / 964 | -9.5 / -1.1 / -9.5 / -22.9 | -26.3 | 0.07 / 0.14 |
| HUGE ENSEMBLE | 64 / 0 / 110 / 50 / 127 / 6 / 0 / 127 | 70 / 90 / 115 / 100 | 619 / 869 | -8.1 / -1.5 / -9.3 / -18.3 | -20.3 | 0.13 / 1.06 |
| RUNNING PAD | 40 / 30 / 100 / 40 / 127 / 127 / 0 / 127 | 95 / 90 / 115 / 100 | 650 / 792 | -6.6 / -2.1 / -8.0 / -21.1 | -24.3 | 0.75 / 1.04 |

**Why these are different:** `experiments/action-banks/ensemble_presets_test.c` measures every preset with two chords (spectral centroid, four band shares, level, the slow and
the fast movement of the level, how far the ensemble smears a held line, the struck start, attack and release) and fails if any two presets are closer than
a fixed distance in that space (the weakest pair of the table, DUSK ORCHESTRA and GLASS CHOIR, measures 3.2; one unit is a third of an octave of centroid, 4 dB of a band,
a 1.6 x change of attack or release, or 3 dB of level). It also checks each preset's job (for example: DEEP DRONE has the lowest centroid and the longest tail, SLOW SWELL
the longest attack, GLASS SHIMMER a struck start, RUNNING PAD the strongest smear, SOLO DRIFT is MONO with DRIFT full) and that every name is unique, fits the list cells and has a
category. This proves that the signals differ, not that they sound good.

**REG is not "brightness":** 0 is the 8' alone, 64 adds the 4' (the brightest setting), above 64 the 16' joins and the sound gets darker. The ensemble is mono (SPREAD is
ignored), so "wide" and "huge" mean deep and thick, not spread in the stereo field.

### PROFILE A

PROFILE A is the controls of the best match, by measurement, to the profile of one reference recording (REG 123, MORPH 15, TONE 70, DRIFT 42, ENS 55, RATE 71, TEXT 31, MIX 126),
found with a search over the eight controls against a few measured features. Recording: spectral centroid about 380 Hz, energy shares -4.1 / -2.7 / -11.9 / -23.5 dB (below 200 Hz,
up to 1 kHz, up to 4 kHz, above), a level movement of about 7.8 Hz at a depth of 0.25. The preset, E2 B2 E3 G3 held: 399 to 406 Hz, -4.1 / -2.7 / -11.1 to -11.4 / -28.9 to -29.0 dB,
7.4 Hz at 0.25. The region above 4 kHz stays about 5 dB lower than the recording: the engine has no more there. This is a match of a few measured numbers, not a claim that it
sounds like the recording; **that judgement is the listener's.** The recording is not part of this repository and no audio from it is used in a preset.

## Layering with a piano or bells

An ENSEMBLE preset belongs to **one track**, and a MIDI channel is heard by one track only (`seq.c midi_slot`: the first track on that channel; host test: two tracks on the same
channel, only the first sounds). A second sound under or over it therefore needs one of:

- **Pair Morph** (one keyboard, one set of notes): the pair sends every note to both members and crossfades them with the MORPH position. Each note takes **two engine voices**, so a
  four-note chord fills the budget of eight (host test: four voices on each member, a fifth note takes a voice, nothing above eight). **A pair with DRONE** is one voice
  when the DRONE preset has TOPO 0 to 2 (DRONE has one voice per part and the pair takes the smaller cap: a drone under a melody or a bass note, not under a chord); with a cell TOPO
  (3 to 5, for example SUB FOG) the pair keeps its four slots and DRONE plays its one voice under four ENSEMBLE voices (host test, see the last recipe).
- **Two tracks on two MIDI channels** (a keyboard or a DAW sending the same notes on two channels, MENU > MIDI > T1..T4 CHANNEL with MIDI IN TRACK): host test, four voices on each track.
- **The sequencer or ARP of both tracks**, as for any two tracks.

The pair crossfade is linear (each member has `1 - m` and `m` of its level), so a layer is quieter than a single preset: raise LEVEL (shared in a pair).

Five recipes, each tested in the host harness with the two reference chords (both members sound, each within 8 dB of the other at the stated MORPH, the sum peaks under 0.9 of full scale,
four voices on each member; on the DRONE recipe four and one). The MORPH value is the Pair MORPH position, 0 = the ENSEMBLE alone, 100 % = the second engine alone; sends 0, LEVEL 100. Design intent, not a verdict:

| Recipe | Track A (ENSEMBLE) | Track B | MORPH | Measured (low chord) |
| --- | --- | --- | --- | --- |
| PIANO UNDER STRINGS | STRING MACHINE | SAMPLE, SET 0 (the PIANO set), preset PIANO | 70 % | the piano speaks for about a second, the strings stay; shares -3.5 / -3.4 dB, mix RMS 0.043 |
| BELLS IN GLASS | GLASS SHIMMER | FM6 BELL | 75 % | both are struck sounds; shares -1.5 / -2.8 dB, mix RMS 0.017 (quiet: the crossfade and the quiet FM6 bell) |
| EP IN THE CHOIR | BREATH CHOIR | FM6 TINE EP | 50 % | the electric piano over a choir formant; shares -4.4 / -0.3 dB, mix RMS 0.032 |
| PIANO ON THE DEEP | DEEP DRONE | SAMPLE, PIANO | 60 % | the piano first, the drone swells under it over 1.3 s; shares -5.3 / -1.6 dB, mix RMS 0.029 |
| STRINGS ON A BED | DUSK ORCHESTRA | DRONE, SUB FOG (TOPO 4) | 70 % | a drone network under a four-note string chord (4 + 1 voices, 5 of 8); shares -3.5 / -1.1 dB, mix RMS 0.033 |

The same five, as audio, are rendered by `tools/ensemble_audition.c presets-v2` (below). No `.hpm` performance setup is shipped: it cannot be produced cleanly without a device, so the recipes
are the parameter lists above.

## How it behaves

- **Voices:** POLY holds at most **6 voices** per track on this engine (the global budget of eight voices across the tracks is unchanged). Pair Morph accepts
  the engine (two voices per note while both members play).
- **SPREAD is ignored:** the ensemble makes its own width (mono here); a SPRD above 0 does nothing on this engine.
- **Signal path:** voices, then the ensemble, then DIST, INSERT, SLICER, FILTER and the sends. The REV send, HALL and STASIS hear the ensembled sound, so a STASIS
  HOLD freezes the ensemble. The global CHORUS is untouched and can be added on top.
- **Tail:** after the last voice the part keeps running 28 blocks (about 20 ms) so the taps run out; there is no cut.
- **Engine switches:** the ring lives in the part's engine memory, which every part already has. Another engine that uses that memory zeroes it; when the part
  was away (SEM and the engines without memory leave it as it is) the ring is cleared as soon as ENSEMBLE runs again, so no old sound comes back.
- **Level:** the three taps are averaged with a 1.19 make-up, so switching the ensemble on changes the level of a sustained chord by a dB or two
  (measured on the host with the renders of `tools/ensemble_audition.c`: between about -3 dB (GLASS CHOIR) and -0.3 dB (VINTAGE STRINGS) of RMS depending on the preset; not a loudness standard).

## Numbers (what was and was not measured)

- **Memory:** 4,136 B of engine memory per part (a 4,096 B int32 ring and 40 B of state; the region is 12,888 B and already exists for each part).
  See the implementation record in the metadata repository for flash, RAM and pool by layout.
- **CPU:** host instruction counts per sample from `tests/regress.c` (one part, notes held, the idle mix of about 361 included): 1,154 / 1,175 / 1,189 / 1,398 for the
  first four presets (six voices, the cap; the ten later presets 1,139 to 1,465), against 1,111 to 1,224 for the SEM presets with eight voices on the same count: about a third more per voice than SEM
  (three oscillators, a filter and the breath noise), plus the ensemble once per track. These are x86/arm host counts, not target cycles; the target's static loop
  costs of `ens_render` and `ens_post` come from `tests/target_budget.py`. **No CPU percentage was measured on a device.**
- **Sound:** plausibility checks only (energy shares, level movement of the ensemble, silence after release); WAV files for listening are rendered by
  `tools/ensemble_audition.c` into `build/ensemble-audio/` (`presets-v2/` for PROFILE A and the nine later presets with two chords each, SOLO DRIFT also as a line, and the four layer recipes).
  A judgement of how it sounds has not been made.

## Tests

`experiments/action-banks/ensemble_test.c` (tables, all eight controls at both ends, the timbre morph, the ensemble on a synthetic signal including its bypass
and its tail, engine switches, voice cap, SPREAD, Pair, HOME / Random Morph / Journey, CC 74, project round trip, a four-track stress), plus the existing
regression, UI, editor, Pair and project suites, which pass with the engine added. `ensemble_presets_test.c` (the preset table: names, level, distinctness, jobs) and `ensemble_layers_test.c`
(the five recipes, the voice budget, a pair with DRONE, MIDI on two channels) are the checks behind the two sections above; `PRINT=1` prints their tables.
