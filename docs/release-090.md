<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Caravel 0.9.0

## Random Morph and Morph Journey, per track

Until 0.8.0 the four snapshot slots, the morph engine and the journey belonged to the whole project. In 0.9.0 **every track has its own**:

- **Four snapshot slots per track** (sixteen in the project). A slot holds the controls pinned on that track's HOME page, **at most 32 targets**,
  and only that track's own controls. SCOPE ALL (RANDOM SETUP) saves and deletes every track's slot at once.
- **One morph engine and one journey per track**, each with its own BARS, DEPTH, SCOPE, SHAPE, WAIT and direction. Morphs and journeys of different
  tracks run side by side; a RANDOM morph holds only the journey of its own track and it goes on by itself afterwards; a control that two tracks move
  belongs to the track that started last. With SCOPE ALL the RANDOM keys act on every track, each on its own controls. An engine holds up to 32 targets:
  **a track whose HOME pins more cannot start a RANDOM morph** (`RANDOM: TOO MANY TARGETS`, nothing moves).
- **The JOURNEY bank now follows RANDOM** (BANK steps RANDOM, JOURNEY, STASIS, PAIR, LIVE). Four small boxes under the bank chip show what each track does.
- **JOURNEY SETUP** (hold BANK in the JOURNEY bank): **K1 SCOPE** `THIS` or `ALL` (PLAY, PAUSE and NEXT act on every track's journey, each with its own slots
  and cycle) and **K2 TRANSP** `FREE`, `CONT` or `START`. `FREE` is the 0.8.0 behaviour (milliseconds). `CONT` counts the sequencer's own bars: Stop freezes
  a leg and its WAIT, a tempo change or an external MIDI clock moves it. `START` also begins the journey again at every Start. Both are saved per track.
- The owner labels (`S1:T3`) and the track skip mask of 0.8.0 are gone: they are not needed any more.
- A RANDOM morph moves the FX filter (FILT, HP / LP) only between -70 and +70.

Step by step with real firmware screens: [tutorial](tutorials/random-morph-journey.md). Reference: [MORPH JOURNEY](morph-journey.md).

## The SAX sample set is retired

To make room in flash, the SAX recordings are no longer in the firmware. **A sound, user preset or project that used SAX now plays PIANO**; the SAMPLE
preset SAX is gone from browsing and the GRAIN factory preset GLITCH uses PIANO as its source. PIANO and FLUTE are unchanged.

## Back up before you update: projects use a new format

0.9.0 saves projects in **format 3** (sixteen snapshot sets and the per-track settings, same flash space).

- **Updating is safe:** projects from every earlier version load. Projects from 0.7.1 to 0.8.0 are migrated: each target of an old shared slot moves
  into the slot of its own track, **at most 32 per slot; anything over that is dropped and the load says `n TARGETS DROPPED`**. The old global settings
  go to all four tracks. Projects from before 0.7.1 have no snapshots.
- **Going back is not:** **0.8.0 does not read a project saved by 0.9.0: that slot shows as empty there.** Nothing is deleted and 0.9.0 reads it again, but
  saving over it in 0.8.0 replaces it. (Tested with real flash images on the host; versions before 0.8.0 were not tested.) Keep a backup made before the
  update (the installer's backup, or `tools/caravel_backup.py save`), and restore it if you go back.

## Also fixed

- The OVERWRITE? dialog of a project SAVE over a used slot flickered while a RANDOM morph or a journey ran (the morph redrew the screen on every
  update). This was also the case in 0.8.0 and before.
- A morph or journey no longer captures FM6's patch selector, which the engine rewrites (issue #35).
- A learned MIDI CC wins over a running morph for every target (0.8.0), unchanged.

## Install

Back up first. Use the [web installer](https://bonszaimusic.github.io/caravel/webapp/installer/)
or download `Caravel-0.9.0-FM1-Testpaket.zip` from the
[release](https://github.com/bonszaimusic/caravel/releases/tag/v0.9.0). After reboot INFO must show `v0.9.0`.
Firmware: `caravel-0.9.0.fwsc`. Matching offline emulator: `Caravel-0.9.0-Emulator.html`. See the [FAQ](faq.md) and the
[installation guide](installing.md) for USB discovery, OCT-/OCT+ and recovery.

## What is tested, and what is not

- Host tests with the real firmware: journey scheduler (67 checks), panel keys and SETUP (79), hold and per-track sets (87), per-track engines, last started wins,
  settings per track, JOURNEY scope ALL and the FILT limit (247), TRANSPORT on the internal clock (18) and on a **simulated** external MIDI clock with ideal pulses (15),
  project round trip with old projects and invalid pads (137; worst case 2,355 of 3,328 bytes), snapshot persistence (5,653), random-track (40) and return-track (476)
  tests, MIDI-CC priority (22), and a stress suite (10.65 million checks: four concurrent journeys with 1,222 arrivals verified against their snapshots, a 300k-step
  chaos run with random CCs, tempo changes, hand edits and project capture/restore, six simulated hours).
- The complete validation of the exact firmware source passed (target build, resource guards, native, sanitizer, storage/USB, audio, WASM and Android-emulator suites).
  Target margins in the three layouts (default, one core, no RAM DSP): image 556,192 / 557,388 / 555,920 B in the 581,564 B app slot, so **about 25 KB are free again**
  (0.8.0: 352 B); RAM .data+.bss 68,700 B of 73,728 B (5,028 B above the 4,096 B guard).
- **On hardware: not tried.** No version of this release has been installed on an FM-1 by the maintainer before publication; the OVERWRITE? fix, concurrent journeys on
  several tracks and the migration were verified on the host only.
- **Not verified:** a controller, a real external MIDI clock (the simulation has ideal pulses; jitter and a real cable are not covered), long real-device runs with several
  concurrent journeys, and the CPU cost of the new transport counter in the audio block (a compare, a 32-bit divide and a few additions per block while the sequencer plays).
- No audio-engine or USB changes apart from the retired SAX set. Everything from 0.8.0 is included.
