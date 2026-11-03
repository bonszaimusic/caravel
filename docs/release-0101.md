<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Caravel 0.10.1

A fix release on [0.10.0](release-0100.md) (the ENSEMBLE engine): one fix in RANDOM and one in ENSEMBLE. No new engine, no project-format change (still format 3), no change to the USB code.

## Fixed

- **ENSEMBLE: the three taps' slow LFOs now run 120 degrees apart, as documented.** In 0.10.0 only the fast LFOs of the taps were spread; the three slow LFOs all started at the same phase and moved all taps together, a common delay change (a vibrato of about +-10 cents for PROFILE A, calculated; the equal phases were read out of the ring state on the host). The ensemble now moves its taps against each other, as described.
  **This changes the sound of all fourteen ENSEMBLE presets** (the 14 golden renders changed; the 133 others did not): measured on the host, the level movement of a held chord repeats less regularly (autocorrelation 0.50 to 0.29, mean of four presets and two chords) and the tone is a little darker (mean centroid 692 to 656 Hz). The maintainer compared host renders of the old and the new behaviour (not on a device) and heard the difference; what that sounds like on the FM-1 has not been recorded.
  **PROFILE A was set again** (controls 123/15/70/42/55/71/31/126 become 120/15/80/60/70/71/31/126): the tone shift moved it away from its reference; it now measures 400 Hz and -3.4 / -3.3 / -11.4 / -27.9 dB against the reference's 380 Hz and -4.1 / -2.7 / -11.9 / -23.5 dB (before: 404 Hz, -5.0 / -2.0 / -12.5 / -29.6 dB; about as close, on the other side in the lowest two bands). The retuned preset was not listened to.
  Two sound-design checks of the preset test were adjusted and say why: the weakest pair of presets measures 2.65 instead of 3.2 (the common waver had kept presets of different RATE and ENS apart; threshold 3.0 to 2.5), and RUNNING PAD, the fastest RATE, no longer smears a held line most of all (it was below -6 dB, now -4.7 dB; ROUGH PULSE smears more). A project that uses ENSEMBLE keeps its settings and sounds slightly different. Costs: about 80 B of flash (measured on an experiment build, three layouts); no RAM or CPU change that could be measured.
  Not changed: ENSEMBLE stays mono (SPREAD is ignored); six taps, a second saw per register and a soft saturation were tried on the host and are not part of 0.10.1.
- **RANDOM: the ship on the sea chart sailed between the snapshots without any change of a parameter.** The slot RANDOM has selected (S1 to S4; the one the ship heads for when
  nothing runs on the selected track, and the one GO and the `Sx >` key name) was set by every leg of **any** track's journey. With a journey running on another track (or with
  JOURNEY scope ALL), the selected track's ship therefore kept moving between the slots, and GO and the key label changed with it, although nothing moved on the selected track.
  The slot now follows only a journey of the **selected** track. Found by the maintainer on the device and reproduced in the host harness with a journey on one track and another track selected (new section 4d of `journey_hold_test`, which failed before the change).
  It was the same in 0.9.0 and 0.10.0. Whether this is the only way the ship can move without a journey on the selected track has not been shown: the cause found here is the one the test reproduces.

## Projects and going back

No project-format change; projects, user presets and favorites load as before. 0.10.1 is a drop-in update of 0.10.0. As always, make a backup before installing.

## Install

Back up first. Use the [web installer](https://bonszaimusic.github.io/caravel/webapp/installer/)
or download `Caravel-0.10.1-FM1-Testpaket.zip` from the
[release](https://github.com/bonszaimusic/caravel/releases/tag/v0.10.1). After reboot INFO must show `v0.10.1`.
Firmware: `caravel-0.10.1.fwsc`. Matching offline emulator: `Caravel-0.10.1-Emulator.html`. See the [FAQ](faq.md) and the
[installation guide](installing.md) for USB discovery, OCT-/OCT+ and recovery.

## What is tested, and what is not

- Host tests with the real firmware code: `journey_hold_test` (with the new section), the other journey suites (tracks, stress, transport, external clock, project), the three ENSEMBLE suites (`ensemble_test`, `ensemble_presets_test`, `ensemble_layers_test`), the regression (147 renders, 14 ENSEMBLE goldens renewed) and the complete validation of the exact firmware source
  (target build, resource guards, native, sanitizer, storage/USB, audio, WASM and Android-emulator suites). The change is one condition in the journey leg; the margins of the three layouts are checked by the release gates (flash, RAM and pool guards).
- **On hardware:** neither fix has been installed or tried on an FM-1 before publication (the RANDOM symptom was seen by the maintainer on the device; its cause was reproduced on the host only; the ENSEMBLE change was compared in host renders only). Everything said about 0.10.0 on hardware still applies:
  no judgement of how ENSEMBLE or any preset sounds has been recorded.
