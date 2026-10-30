<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Caravel 0.7.1

## Keep four performance destinations

S1–S4 Random Morph snapshots now belong to the performance project. Each slot
keeps its empty/occupied state, ROW/TRACK/ALL scope, captured track/row, pinned
parameter references and values. All four slots retain the 128-target capacity.

1. Double-tap HOME for ACTION, then choose RANDOM with a short BANK tap.
2. Tap A4 to select S1–S4. Hold A4 at least 0.5 seconds to capture the current
   audible values of the morph-safe controls assigned to HOME.
3. Tap A3 to morph from the current sound to the selected slot. A2 freezes or
   resumes; manually edited controls keep priority as before.
4. Save to A–D. During playback this is RAM-only; STOP commits it to flash.
   Wait for save confirmation before powering off. Loading another project
   replaces all four slots with that project's set.

RESTORE LAST includes slots when enabled and after the quiet autosave delay.
USB backups and `.hpm` export/import include them. Project INIT, native-only
project loads and old HPM1 imports start with empty slots. Presets do not store
these project slots. STASIS audio, running morphs and RETURN origins stay volatile.

## Install

Back up first. Use the [web installer](https://bonszaimusic.github.io/caravel/webapp/installer/)
or download `Caravel-0.7.1-FM1-Testpaket.zip` from the
[release](https://github.com/bonszaimusic/caravel/releases/tag/v0.7.1).
The ZIP includes the firmware, verified source, checksums, licences and a local
installer. On Android use a Web MIDI capable browser and the online installer.
After reboot INFO must show `v0.7.1`.

Firmware: `caravel-0.7.1.fwsc`. Matching offline emulator:
`Caravel-0.7.1-Emulator.html`. The installer verifies exact bytes, SHA-256,
product, layout and app/loader CRC before install/resume. A digest served by
the same website is a consistency check, not independent authenticity proof.
See the [FAQ](faq.md) and [installation guide](installing.md) for USB discovery,
OCT-/OCT+, stock-return error -5 and recovery. This is not a general issue #4 fix.

## Storage and rollback

Old HPM1 v1/10,000-byte projects load with empty slots. New HPM1 v2 files are
7,200 bytes and require 0.7.1 or later. Keep pre-update backups for rollback.
Older firmware cannot read the new combined copies and may show an older
native project fallback.

HOME/Pair metadata and slots are compressed inside the existing 7,168-byte
protected flash-copy payload. Flash regions and audio allocations are not
enlarged. Writes keep two copies and commit the header last; native originals
remain intact during first migration.

The checked full case contains 32 HOME rows per track, 128 performance events
and four 128-target snapshots. Arbitrarily dense, poorly compressible data may
exceed the fixed space. `PROJECT TOO LARGE / NOT SAVED` rejects it before erase
or replacing an acknowledged pending save. Reduce unused rows/routes or slot
contents and save again. USB WORK export uses the same bounded representation.

HALL/STASIS and the SLICER effect remain; ROOM, SPRING and the SLICE engine
remain omitted with compatibility prompts. Ordinary tempo DELAY is suspended
while STASIS owns the shared buffer.

## Validation boundaries

Release automation requires exact target/resource guards, native storage/USB/UI
and audio regressions, sanitizers, C/WASM snapshot save/load and fresh-instance
NOR reboot, and complete source validation. Tests include all A–D slots,
autosave, pending live saves, legacy projects, corrupt data and interrupted
first migration.

These are software simulations and builds. No new physical FM-1 installation,
power-cut, save/reboot/recall or USB install/resume acceptance is claimed for
0.7.1. Earlier 0.7.0 listening reports are not tests of this storage change.
DIVE is excluded; no new audio engine or extended STR/MOTN range is included.
