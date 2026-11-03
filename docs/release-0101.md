<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Caravel 0.10.1

A fix release on [0.10.0](release-0100.md) (the ENSEMBLE engine). Nothing else changed: no new engine, no project-format change (still format 3), no change to the USB code.

## Fixed

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

- Host tests with the real firmware code: `journey_hold_test` (with the new section), the other journey suites (tracks, stress, transport, external clock, project) and the complete validation of the exact firmware source
  (target build, resource guards, native, sanitizer, storage/USB, audio, WASM and Android-emulator suites). The change is one condition in the journey leg; the margins of the three layouts are checked by the release gates (flash, RAM and pool guards).
- **On hardware:** the fix itself has **not** been installed or tried on an FM-1 before publication (the symptom was seen by the maintainer on the device; the cause was reproduced on the host only). Everything said about 0.10.0 on hardware still applies:
  no judgement of how ENSEMBLE or any preset sounds has been recorded.
