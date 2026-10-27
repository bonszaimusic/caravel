# Caravel 0.6.0 - STASIS, ADD and BLEND

Capture a moment, keep its texture, and build on it while you play.

## Play it

- **STASIS HOLD:** capture recently played sound from the internal REV send.
  STR controls the foreground texture; MOTN moves reflections, filtering and
  HALL damping while retaining the grain playback rate.
- **ADD:** mix a new captured sound into the held texture with adjustable AMT.
  Complementary gains keep repeated additions bounded.
- **BLEND:** transition into a snapshot of a new sound over 1-30 seconds, then
  hold the destination. OCT+ explicitly starts or cancels WAIT/MIX.
- STOP and HOLD-to-LIVE release the audible mixture into the natural HALL tail.
  HOME, Pair Morph, macros and performance storage from 0.5.0 remain available.

## Learn and listen

- [Illustrated tutorials and downloadable PDF](https://bonszaimusic.github.io/caravel/tutorials/)
- [Three original sound examples with cue times](https://bonszaimusic.github.io/caravel/sounds/)
- [Browser emulator](https://bonszaimusic.github.io/caravel/emulator/)
- [USB web installer](https://bonszaimusic.github.io/caravel/webapp/installer/)
- [Full changelog](https://github.com/bonszaimusic/caravel/blob/v0.6.0/CHANGELOG.md)

The examples are rendered by the actual native firmware DSP, with level
normalization and no external sound effects; they are not FM-1 recordings.

## Read before upgrading

0.6.0 contains HALL and STASIS. ROOM, SPRING and the SLICE engine are omitted
to fit the target. The separate SLICER track effect remains. Projects using
omitted features show explicit substitutions and CANCEL / TEST before loading.
TEST converts a working copy, protects the source slot and lets you save elsewhere.
Export a complete backup first. Keep user banks with the performance.

STASIS uses internal REV send; there is no external microphone/USB audio capture.
Ordinary tempo DELAY is suspended while STASIS owns the shared 128-KiB buffer.
Project saves retain the controls, not the captured audio. Individual merged
layers cannot be removed. ADD/BLEND add 60 bytes of management state and no audio
allocation; target image is 554,384 bytes, RAM .data+.bss 64,460 bytes and pool
360,164 bytes in the tested profile.

## Validation and downloads

Publication is gated on the complete automated host suites, exact target profile,
audio references, MIDI, UI/storage, web/WASM, ASan/UBSan, ThreadSanitizer and
individual machine-readable results. The published release is rebuilt from the
same committed source, compared with the tested app, and carries a verified
package, complete recursive source, build record, notices and SHA256SUMS.

Actual FM-1 worst-case CPU, physical USB/OTA installation and long-term stability
remain hardware acceptance checks; host timing is not a hardware load estimate.
Install custom firmware at your own risk. An interrupted or failed update can
require recovery tools; reinstalling official firmware does not guarantee data
restoration or recoverability. Caravel is independent of M-VAVE and Hugelton Instruments.
