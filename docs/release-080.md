<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Caravel 0.8.0

## MORPH JOURNEY

A journey chains your Random Morph snapshots S1–S4 into one long, hands-free movement: the sound morphs to a
slot, rests, moves on to the next slot that holds a sound and loops. It has its own ACTION bank, the fifth after
LIVE, and is played from the panel; MIDI Learn can also map it to a controller.

1. Double-tap HOME for ACTION. In the RANDOM bank hold A4 to save a sound into the selected slot, tap A4 to select
   the next slot. Two to four slots are enough; the four slots are shared by all tracks, and the owner of each is shown (`S1:T1 S2:T3 S3:ALL S4:-`).
2. Tap BANK until the title reads JOURNEY. **A1** PLAY / PAUSE / RESUME, **A2** direction (the running leg finishes,
   then the next one goes the new way), **A3** NEXT, **A4** tap WAIT (0, 1, 2, 4, 8, 16 bars of rest), A4 hold saves.
3. Leg length and curve are RANDOM SETUP's BARS and SHAPE. A journey keeps running when you change bank.
4. **One morph engine, one morph at a time.** A RANDOM morph on any track holds the journey and it goes on by itself when that morph is
   done; A1 while held pauses it. Saving or deleting a snapshot ends it. The title and the line under the keys always say which track owns
   what is running.
5. **Four slots for the whole project.** Every slot is a scene: a leg moves exactly the controls stored in its target slot, so slots of
   different tracks (and an ALL slot) form one journey. The owner of each slot is shown (`S1:T1 S2:T3 S3:ALL S4:-`).
6. **Tracks left out:** hold BANK in the JOURNEY bank, K1 to K4 are TRACK 1 to 4, `ON` or `SKIP` (saved with the project).
7. **Delete a slot:** in RANDOM's SETUP, select it and hold A4.

Step-by-step with real firmware screens: [Random Morph and Morph Journey tutorial](tutorials/random-morph-journey.md).
Reference, rules and MIDI table: [MORPH JOURNEY](morph-journey.md).

## MIDI CC and a running morph

A learned MIDI CC now takes its parameter over from a running Random Morph for every target. For the LFO2 and
DRUM-X controls the ownership check compared a truncated 8-bit id (their ids start at 256 and 400), so a running
morph kept overwriting the CC. The full 16-bit id is used now.

## Performance settings are saved with the project

RANDOM's BARS, DEPTH, SCOPE and SHAPE and the journey's WAIT and direction are saved with each performance project,
next to the snapshots S1–S4 (slots A–D, RESTORE LAST, USB backup, `.hpm` export and import). A project saved with
BARS 16, SHAPE DRIFT and WAIT 4 sounds the same after reopening it. A loaded project always starts with the journey
stopped and S1 selected; the running journey, the countdown and the selected slot are not saved.

**Back up before installing, and before going back.** The settings use bytes that earlier firmware requires to be
zero, so the project size and layout do not change, but **0.7.x and earlier refuse a project saved by 0.8.0**
(`PERF PROJECT INVALID`). Projects made before 0.8.0 load with the defaults: BARS 8, DEPTH 35, TRACK, SMOOTH, WAIT 0,
forward. Going back to 0.7.2 means restoring a backup made before the update.

## Install

Back up first. Use the [web installer](https://bonszaimusic.github.io/caravel/webapp/installer/)
or download `Caravel-0.8.0-FM1-Testpaket.zip` from the
[release](https://github.com/bonszaimusic/caravel/releases/tag/v0.8.0). After reboot INFO must show `v0.8.0`.
Firmware: `caravel-0.8.0.fwsc`. Matching offline emulator: `Caravel-0.8.0-Emulator.html`. See the [FAQ](faq.md) and the
[installation guide](installing.md) for USB discovery, OCT-/OCT+ and recovery.

## What is tested, and what is not

- Host tests with the real firmware: MIDI-CC priority (22 checks), journey scheduler (71), panel keys and SETUP rules (69), hold, tracks left out, mixed slots and delete (64),
  project round trip with old projects and invalid pads (98; worst case 2,493 of 3,328 bytes), and a stress suite
  (2.57 million checks: 600 legs with 128 targets per slot verified against their snapshots, a 300k-step chaos run with random
  CCs, tempo changes, hand edits, FREEZE/RESUME, capture/restore, and six simulated hours at 60 and 200 BPM).
- The complete validation of the exact firmware source passed: target build, resource guards, native, sanitizer, storage/USB,
  audio, WASM and Android-emulator suites. Target margins of the build: image 581,408 B, **only 156 B left in the app slot**
  (0.7.2: 5,600 B; the next feature must make room first), RAM 4,196 B left, pool 10,168 B left (unchanged).
- **On hardware:** the maintainer installed the final build on an FM-1 and reports that it works. That was a first trial, not a systematic
  acceptance with a measured scope.
- **Not verified:** a controller, external MIDI clock (leg lengths follow the tempo setting, not an external clock), sustained real-device
  audio and CPU load, and worst-case timing of the ownership check in the MIDI context (static code only).
- Known and not fixed here: the FM6 patch selector can be pinned by a morph although the engine rewrites it
  (tracked as an issue; not part of the journey).

No audio-engine or USB changes. Everything from 0.7.2, including the stock-return candidate fix, is included.
