# Caravel 0.12.0

> [!IMPORTANT]
> **Newer FM-1 units (flash UC25HQ80, JEDEC ID `0xB36014`) are recognised.** Up to 0.11.0 only the Puya P25Q80 (`0x856014`) was accepted: on a newer unit projects stayed in RAM (`SAVED (RAM)`, gone after power-off) and the installer's update was not started. 0.12.0 accepts both chips. Not tested by us on a real UC25HQ80 unit (host test of the ID list; one user's report for Felucca's 1.8.2 beta).

**HARMONY.** One chord progression that your tracks follow: four chords of 1, 2, 4 or 8 bars on the transport's bar clock (the internal BPM or the external MIDI clock). A track that follows takes chord tones on the sequencer's strong steps and every ARP step, and its live keys are snapped to the current chord;
CHRD = BUS plays the whole chord from one key. **LINK** copies one track's key to another, and the **SCL pages** are a family you walk with SELECT. With HARMONY off nothing changes.

**HOME ACTION has new keys and a sixth bank.** C#5 steps one bank back, D#5 forward, D5 / E5 / F5 / G5 are A1 to A4: **the 0.11.0 keys moved.** The new bank **HARM** has HOLD / RESUME, PREV, NEXT, RESET and, in SETUP, SHIFT (the progression moved by scale degrees).
Also in: SCL + OCT+ copies the key to all tracks, GLO + OCT+ arms a bar reset, SEQ TOOLS G5 GENERATE, ARP mode TURING, LATCH STACK on the chord bus, and removed undefined signed arithmetic in several DSP paths (no change in the compared renders).

**Newer FM-1 units** with a UC25HQ80 flash (JEDEC ID 0xB36014) are recognised: saving to flash and updating work there (host-tested, not tried on such a unit).

**Projects: back up first.** The project format changes (FUN11, same size). **0.11.0 and earlier show a project saved by 0.12.0 as an empty slot**; 0.12.0 loads every earlier project with HARMONY and LINK off.

**Costs and limits.** The flash margin is thin: 3,060 B of the app slot are left (0.11.0: 21,164 B), and the RAM headroom is 48 B above the guard. CHORD EXT is not in this release; HARMONY is one global loop of four chords; LATCH STACK with three or more overlapping latched chords is not exact.

**What is tested.** Host tests with the real firmware code (strict-UBSan UI test, the HARM panel test, the eight host shards, the ACTION / WASM suites, the three layouts with the budget gate) and a local Linux container; every review fix has a regression test that fails before it. **On hardware:** the maintainer reports a good device test of an **earlier** 0.12.0 test package (review round 4, 2026-11-09) **without per-item results**; the firmware changed after it and **none of the later fixes (including the UC25HQ80 detection and the round-7 review fixes) has been tested on a device**; no protocolled listening, readability, clock or controller test, and no CPU measurement on the device.
The backup tool and the editor were not run against a device holding FUN11 slots.

Download `Caravel-0.12.0-FM1-Testpaket.zip` for the local installer, firmware,
verified source and notices, or `Caravel-0.12.0-Emulator.html` for the offline
emulator. Firmware: `caravel-0.12.0.fwsc`. INFO must show `v0.12.0` after reboot.
[Details](https://github.com/bonszaimusic/caravel/blob/v0.12.0/docs/release-0120.md).
