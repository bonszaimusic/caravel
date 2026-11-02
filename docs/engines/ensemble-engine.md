# ENSEMBLE — strings, choir and dark texture with a built-in ensemble

ENSEMBLE is an original string-machine / choir / dark-pad engine by bonszaimusic (Caravel). It takes the idea of sawtooth registers played into a
slowly moving ensemble; it is not an emulation of any instrument or circuit. **Status: on the branch `feature/ensemble-engine`, not part of a release.
Everything below is verified on the host only (real firmware code in the host harness); it has not been tried on an FM-1, and nobody has judged how it sounds.**

## Selecting ENSEMBLE

Select **ENSEMBLE** in PRESETS or with EDIT held and KNOB 1 (it is the last engine in the list). Engine ID 17 identifies it; no existing engine number
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

| Preset | Idea |
| --- | --- |
| VINTAGE STRINGS | open saw registers, slow ensemble |
| GLASS CHOIR | the choir end of MORPH, airy, a little faster ensemble (the DRONE engine has a preset of the same name) |
| DUSK ORCHESTRA | strings leaning to the choir, all registers |
| ABYSS PAD | the dark end, rough, slow, a long release |

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
  (measured on the host: between about -4 and +0.5 dB depending on the preset; not a loudness standard).

## Numbers (what was and was not measured)

- **Memory:** 4,136 B of engine memory per part (a 4,096 B int32 ring and 40 B of state; the region is 12,888 B and already exists for each part).
  See the implementation record in the metadata repository for flash, RAM and pool by layout.
- **CPU:** host instruction counts per sample from `tests/regress.c` (one part, notes held, the idle mix of about 361 included): 1,154 / 1,175 / 1,189 / 1,398 for the
  four presets (six voices, the cap), against 1,111 to 1,224 for the SEM presets with eight voices on the same count: about a third more per voice than SEM
  (three oscillators, a filter and the breath noise), plus the ensemble once per track. These are x86/arm host counts, not target cycles; the target's static loop
  costs of `ens_render` and `ens_post` come from `tests/target_budget.py`. **No CPU percentage was measured on a device.**
- **Sound:** plausibility checks only (energy shares, level movement of the ensemble, silence after release); WAV files for listening are rendered by
  `tools/ensemble_audition.c` into `build/ensemble-audio/`. A judgement of how it sounds has not been made.

## Tests

`experiments/action-banks/ensemble_test.c` (tables, all eight controls at both ends, the timbre morph, the ensemble on a synthetic signal including its bypass
and its tail, engine switches, voice cap, SPREAD, Pair, HOME / Random Morph / Journey, CC 74, project round trip, a four-track stress), plus the existing
regression, UI, editor, Pair and project suites, which pass with the engine added.
