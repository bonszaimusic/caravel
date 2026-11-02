<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Caravel 0.10.0

## A new engine: ENSEMBLE (shown as ENSEMB)

ENSEMBLE is a string-machine / choir / dark-pad engine with a built-in ensemble: three oscillator registers (16', 8', 4') into one filter whose timbre
follows **MORPH** (strings, a resonant choir formant with breath, dark and rough), and a three-tap ensemble that is part of the track (it comes before DIST, INSERT, FILTER
and the sends, so the REV send, HALL and STASIS hear it). Eight controls: **REG, MORPH, TONE, DRIFT, ENS, RATE, TEXT, MIX**; HOME shows MORPH, ENS, TONE, MIX; MIDI CC 74 is TONE.
At most six voices per track; SPREAD is ignored (the ensemble is mono); MIX 0 is the dry voices bit for bit. It is the last engine in the list (engine 17; no existing engine number changed).
Full guide: [ENSEMBLE](engines/ensemble-engine.md).

- **Fourteen factory presets**, each set up for a different job: VINTAGE STRINGS, GLASS CHOIR, DUSK ORCHESTRA, ABYSS PAD, PROFILE A, STRING MACHINE, BREATH CHOIR, DEEP DRONE,
  GLASS SHIMMER, SLOW SWELL, ROUGH PULSE, SOLO DRIFT (MONO), HUGE ENSEMBLE, RUNNING PAD. PROFILE A has its controls set by measurement to the profile of a reference recording;
  no audio from it is stored. A host test measures every preset (spectrum, level, level movement, attack, release) and keeps every pair a minimum distance apart:
  **the presets differ as signals; whether they sound good is not something a test can say.**
- **Five layer recipes** for a second sound under or over an ENSEMBLE preset, played with Pair Morph from the same notes: STRING MACHINE + the PIANO set, GLASS SHIMMER + FM6 BELL,
  BREATH CHOIR + FM6 TINE EP, DEEP DRONE + the PIANO set, DUSK ORCHESTRA + a DRONE cell preset (SUB FOG). A pair takes two engine voices per note, so a four-note chord fills the budget of
  eight; a pair with a DRONE of TOPO 0 to 2 is one voice. A MIDI channel is heard by one track only: a keyboard that should play two tracks sends on two channels. The recipes
  are in the [guide](engines/ensemble-engine.md#layering-with-a-piano-or-bells).
- The web editor lists ENSEMB with all fourteen presets.
- **The version is shown as `v0.10.0`** (the minor number has two digits for the first time; the installer, the release tools and the website scripts were changed to accept it).

## Projects and going back

**No project-format change** (still format 3). Projects, user presets and favorites from every earlier version load as before. **A project that uses ENSEMBLE on any track is not read by
0.9.0: that slot shows as empty there** (tested with real flash images on the host; nothing is deleted, 0.10.0 reads it again, but saving over it in 0.9.0 replaces it; versions before 0.9.0
were not tested, assume the same). Keep a backup made before you update (the installer's backup, or `tools/caravel_backup.py save`).

## Install

Back up first. Use the [web installer](https://bonszaimusic.github.io/caravel/webapp/installer/)
or download `Caravel-0.10.0-FM1-Testpaket.zip` from the
[release](https://github.com/bonszaimusic/caravel/releases/tag/v0.10.0). After reboot INFO must show `v0.10.0`.
Firmware: `caravel-0.10.0.fwsc`. Matching offline emulator: `Caravel-0.10.0-Emulator.html`. See the [FAQ](faq.md) and the
[installation guide](installing.md) for USB discovery, OCT-/OCT+ and recovery.

## What is tested, and what is not

- Host tests with the real firmware code: `ensemble_test` (the tables, all eight controls at both ends, the timbre morph, the ensemble and its bypass and tail, engine switches, the voice cap,
  SPREAD, Pair, HOME / Random Morph / Journey, CC 74, project round trip with real flash images, a four-track stress), `ensemble_presets_test` (names, level, distinctness, the job of each preset)
  and `ensemble_layers_test` (the five recipes, the voice budget, MIDI on two channels), plus the existing regression (147 golden renders), UI (0 lint findings with the longer version text),
  editor, Pair, HOME, STASIS and project suites. Golden hashes come from the Linux CI.
- The complete validation of the exact firmware source passed (target build, resource guards, native, sanitizer, storage/USB, audio, WASM and Android-emulator suites).
  Target margins in the three layouts (default, one core, no RAM DSP): image 559,420 / 560,236 / 559,164 B in the 581,564 B app slot, so **22,144 / 21,328 / 22,400 B are left** (0.9.0: 25,372 / 24,176 / 25,644 B; the engine and its ten
  later presets cost about 3.2 KB); RAM .data+.bss 68,700 B of 73,728 B (5,028 B headroom, 932 B above the 4,096 B guard; 64,632 B in the one-core layout) and the pool 10,168 B spare, both unchanged by the engine
  (its memory is the per-part engine memory every part already has). The gates (4 KB flash, 300 B RAM above the guard, 2 KB pool) hold in all three layouts.
- CPU: only static counts. Host instructions per sample for the ENSEMBLE presets are 1,139 to 1,465 against 1,111 to 1,224 for SEM (about a third more per voice); the target's loop costs of
  `ens_render` (342) and `ens_post` (176) are static. **No CPU percentage was measured on a device.**
- **Not tried on an FM-1, and nobody has listened to the engine or the presets on the device.** WAV renders from the host DSP exist for listening, but a judgement of how any preset or recipe
  sounds, or whether PROFILE A is close to its reference, has not been made. Not verified: the engine on hardware at the voice cap with the other engines, long runs, the layer recipes on the device.
- No change to existing engines, the USB code or the project format. Everything from 0.9.0 is included.
