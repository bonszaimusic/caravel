<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Caravel 0.11.0

**ENSEMBLE in stereo.** The pad sounds of the ENSEMBLE engine now have a width of their own, and a Pair Morph may have a width on any member. No new engine, no project-format change (still format 3), no change to the USB code.
Details of the engine: [ENSEMBLE](engines/ensemble-engine.md); previous release: [0.10.1](release-0101.md).

## What is new

- **ENSEMBLE is stereo, and SPRD is its width.** Six delay taps read the same ring: the three taps of 0.10.1 (6.0 / 8.5 / 11.0 ms) are the left channel, three more (6.8 / 10.1 / 12.7 ms, with their own slow and fast LFO rates, 60 degrees on) the right.
  Each channel is an ensemble at the mono ensemble's level. The width is the track's SPRD parameter (EDIT, MORE page): 0 is the mono ensemble of 0.10.1 bit for bit, 127 the full image. Only POLY and UNISON parts are stereo; MONO and LEGATO stay mono; MIX 0 is the dry voices in every case.
  PAN works as without SPREAD.
- **The factory pads load with a width.** A factory ENSEMBLE preset used to set SPRD to 0; it now loads: VINTAGE STRINGS 127, GLASS CHOIR 127, DUSK ORCHESTRA 127, ABYSS PAD 127, PROFILE A 127, SLOW SWELL 127, RUNNING PAD 127, STRING MACHINE 112, DEEP DRONE 112, BREATH CHOIR 96, HUGE ENSEMBLE 96.
  GLASS SHIMMER, ROUGH PULSE and SOLO DRIFT stay mono. (The widest of 127 / 112 / 96 at which no octave band between 125 Hz and 8 kHz is anti-correlated between the channels, measured on the host.) User presets and projects keep the SPRD they store.
- **Pair Morph: a member may have SPRD above 0, on any engine.** Until 0.10.1 a Pair showed `SPREAD OFF` for a member with SPRD above 0 and switched itself off when you raised it. Now the morph weight scales the member's sum and its side signal, and the Pair's level and pan apply:
  the Pair is stereo at a stereo member's end, half as wide at half the morph and mono at a member with SPRD 0. This holds for an ENSEMBLE member's width and for another engine's voice SPREAD alike (a MONO or LEGATO member has no side, as outside a Pair). The five ENSEMBLE layer recipes keep their ENSEMBLE member's width.
- 0.10.1's changes (the RANDOM slot and ship, the ENSEMBLE slow-LFO fix) are included.

## What it costs, and what changes in the sound

- **The mono sum is lower.** What a mono listener hears, and what the REV send, HALL and STASIS take, is the mean of the two channels: on the host about 2.0 to 3.5 dB lower than the mono preset (mean about 3 dB over the pads), because the two channels are only partly alike. Each channel stays near the mono level (-1.3 to 0.0 dB).
  A mean lift was tried (x1.19: mono sum -1.6 dB; x1.41: -0.2 dB, channels +0.6 / +1.5 dB) and is not in; the release has none (x1.00).
- **The reverb return is lower with a width.** In the measured case (VINTAGE STRINGS, REV 127, LEVEL 40, the 0.8 s after the voices are free) the HALL return was about 6 dB lower than at SPRD 0, more than the 2.7 dB of the dry mono sum, and **that extra part is not explained**. If a pad sounds drier than before, raise REV or LEVEL.
- **CPU (host instructions per sample, not target cycles):** about +80 per ENSEMBLE track with a width (nothing per voice), and about +77 more for a Pair with a stereo ENSEMBLE member. **Flash:** the default layout is 560,400 B in the 581,564 B app slot (+780 B against 0.10.1), so 21,164 B are left; one core 561,336 B (20,228 B left); no RAM DSP 560,160 B (21,404 B left). RAM `.data+.bss` and the pool are unchanged (the ensemble's state, 4,164 B, sits in the part's engine memory).
  The static cost of `ens_post` in the target build rose from 186 to 332 (six taps and the side output in one loop); the budget file says so.

## Projects and going back

No format change. A project saved in 0.11.0 with a stereo ENSEMBLE pad loads on 0.10.1: the SPRD is kept and shown, the released ENSEMBLE ignores it and plays the mono ensemble (tested with real flash images on the host); nothing is lost, and 0.11.0 plays the stored width again.
On 0.10.x a Pair still refuses a member with SPRD above 0 (`SPREAD OFF`), so a stereo pad has to be set to SPRD 0 there to switch a Pair on. On any other engine SPRD above 0 is the voice SPREAD in every version. Back up before you update, as always.

## Install

Back up first. Use the [web installer](https://bonszaimusic.github.io/caravel/webapp/installer/)
or download `Caravel-0.11.0-FM1-Testpaket.zip` from the
[release](https://github.com/bonszaimusic/caravel/releases/tag/v0.11.0). After reboot INFO must show `v0.11.0`.
Firmware: `caravel-0.11.0.fwsc`. Matching offline emulator: `Caravel-0.11.0-Emulator.html`. See the [FAQ](faq.md) and the
[installation guide](installing.md) for USB discovery, OCT-/OCT+ and recovery.

## What is tested, and what is not

- Host tests with the real firmware code: `ensemble_test` (stereo, the Pair with a stereo ENSEMBLE member, snapshots, the HALL return and a STASIS hold of a stereo pad), `ensemble_presets_test` (every check at SPRD 0 and at each preset's own width), `ensemble_layers_test`, `flex_test` (all 225 ordered engine pairs a second time with SPRD 100 on both members: 403,220 checks),
  `pair_test`, `usability_test`, the regression (147 renders: the 11 ENSEMBLE pad renders renewed on purpose, 136 unchanged), the web editor's mock against the firmware tables, and the complete validation. The golden hashes were taken on a Mac and are compared with Linux in the validation.
- A local Linux container ran the ACTION job (27 of 27 steps) and all eight host shards on the candidate; the three ThreadSanitizer tests (shard `cpu_dual`) cannot run there and are a CI job.
- **On hardware:** nothing. Test builds (0.11.0-rc1 and -rc2) were prepared for the maintainer's FM-1, and whether they were installed and how they sounded on the device is not recorded here. The maintainer listened to host renders of the stereo ENSEMBLE at full and half width without a mean lift, also in Pair recipes, and judged them good. **Not verified:**
  the Pair page and the MORE page on the device, the real HALL / STASIS balance with a width, the CPU on the device, a Pair of two voice-SPREAD engines by ear (signal facts only), and the Pair's browser tests (the validation job "experiments") were not run in the local container.
