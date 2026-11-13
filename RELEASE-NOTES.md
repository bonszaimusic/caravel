# Caravel 0.13.0

> [!IMPORTANT]
> **Newer FM-1 units (flash UC25HQ80, JEDEC ID `0xB36014`) are recognised** (since 0.12.0): saving to flash and firmware updates work there. Not tested by us on a real UC25HQ80 unit (host test of the ID list; one user's report for Felucca's 1.8.2 beta).

**ELASTIC P1.4.** A seventh HOME ACTION bank (between STASIS and PAIR) turns supported HOME controls into temporary springs: A1 ARM / OFF, A2 PULSE, A3 HOLD / RESUME, A4 RESET; hold the bank key for SETUP (SCOPE CELL / ROW, PULL, RETURN, RANGE; A4 in SETUP switches to BOUNCE, REFLECT, COUPLE, SEED). The four tracks are independent.
**BOUNCE is a musical period**, one full oscillation from 1/16 note to 64 BAR, following the internal tempo or the external MIDI clock; a tempo change keeps the phase, and a gesture starts at once rather than on a step or bar. **RETURN INF** keeps swinging until HOLD, RESET or OFF. The HOME cards show a spring mark and a moving spring. ELASTIC cannot be MIDI-learned.

**Smaller factory SAMPLE set.** PIANO uses three full recordings instead of five zones, FLUTE two instead of three: **27,014 B less flash**, and **some PIANO and FLUTE notes sound different** (wider transposition; an upward-transposed note can end earlier). Presets, user slots and the five-zone PIANO HD export are kept. The maintainer chose this variant after listening to host renders ("fein"); it is not a measured listening test of the final firmware.

**Fixes.** The One-Core flash layout fits again. The MOD slots' DST knob read one byte past its table when turned to its right end (0.12.0 and RC1 too) and could not select SCAN; fixed.

**Projects: back up first.** Every project saved by 0.13.0 carries an HPM1 **v5** HOME capsule. 0.13.0 loads v1 to v5; **0.12.0 reads only v1 to v3 and does not accept v5**, so projects saved by 0.13.0 are not readable there: **upgrading is safe, going back is not.** The command-line backup tool `tools/caravel_backup.py` of 0.13.0 saves and restores v4 and v5 projects byte-preserving; copies from 0.12.0 or earlier stop with `Invalid HPM1 capsule`. The web backup checks sizes only and has **not been verified on a device with v5 projects**.

**Costs and limits.** Final build: 28,548 B of the app slot are free in the default layout (image 553,016 B; 0.12.0: 3,060 B free); the static RAM headroom is 352 B above the guard in the default layout. BOUNCE is tempo-synchronous, not locked to the sequencer's steps or bars; long-run external-clock drift and the sound of 1/16 at 240 to 300 BPM are not covered by tests. CHORD EXT is not in this release.

**What is tested.** Host tests with the real firmware code on the RC1 source (the eight host groups with ASan / UBSan and host ThreadSanitizer, 28 ACTION steps, 45 target-validation entries, 147 of 147 audio hashes in three host configurations, three SDK layouts, the matching browser build with HPM1 v5 save / load); internally reviewed, **not independently**. **Re-run on the final 0.13.0 source:** the 28 ACTION steps, all eight host groups, the ELASTIC host scripts, the sample-zone policy test, the MOD DST order test and the USB backup tests passed; host ThreadSanitizer, the HARM / CHORD EXT fuzz runs and the GitHub-only jobs were not re-run.
**On hardware:** the 0.13.0-rc1 package was **tested successfully on an FM-1 by the maintainer (RC1, no individual results)** and without per-item results and without naming the tested package's hash. **The final 0.13.0 is not byte-identical to RC1**: it differs by the version text and the MOD DST table fix (plus a compile-time check); neither has been tested on a device. Not covered: a protocolled hardware, clock and listening test, the CPU on the device, a real UC25HQ80 unit.

Download `Caravel-0.13.0-FM1-Testpaket.zip` for the local installer, firmware,
verified source and notices, or `Caravel-0.13.0-Emulator.html` for the offline
emulator. Firmware: `caravel-0.13.0.fwsc` (SHA-256 `a1d560684519667b4051b35630380874065853a20a353a4f9d279c2bfee208b7` expected, checked against the draft before publication). INFO must show `v0.13.0` after reboot.
[Details](https://github.com/bonszaimusic/caravel/blob/v0.13.0/docs/release-0130.md).

Caravel is independent of M-VAVE and Hügelton Instruments. Firmware
flashing and returning to stock carry risks. See the
[installation FAQ](docs/faq.md).
