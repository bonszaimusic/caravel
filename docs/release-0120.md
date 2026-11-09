<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Caravel 0.12.0

**HARMONY.** One chord progression that your tracks can follow, per-track key LINK, live control of the progression from the HOME ACTION keys, and a rebuilt SCL page family. **This release changes the project format (FUN11): a project saved by 0.12.0 shows as an empty slot on 0.11.0 and earlier. Back up first.**
Previous release: [0.11.0](release-0110.md). Step-by-step guides with real firmware screens: [HARMONY](tutorials/harmony-live.md) and the [tutorial overview](tutorials/README.md).

> [!IMPORTANT]
> **Newer FM-1 units (flash UC25HQ80, JEDEC ID `0xB36014`) are recognised.** Up to 0.11.0 only the Puya P25Q80 (`0x856014`) was accepted: on a newer unit projects stayed in RAM (`SAVED (RAM)`, gone after power-off) and the installer's update was not started. 0.12.0 accepts both chips. Not tested by us on a real UC25HQ80 unit (host test of the ID list; one user's report for Felucca's 1.8.2 beta).

## What is new

- **HARMONY: a chord bus for all tracks.** Four chords, each lasting 1, 2, 4 or 8 bars; the roots are scale degrees of track 1's scale and each chord is built on its root. The bus counts the **transport's bars**, not an audio metronome: the internal BPM clock or the external MIDI clock, whichever the sequencer uses.
  PLAY starts at bar 1, STOP holds the chord, Continue goes on, BAR RESET returns to bar 1. A track that **follows** the bus takes chord tones on the sequencer's strong steps (steps 1, 5, 9 and 13) and on every ARP step (also in ARP mode TURING); GENERATE puts chord tones on strong steps; its live keys and MIDI notes are snapped to the nearest chord tone.
  Drum lanes, kits, slices and engines that map the keys themselves are never moved. With HARMONY off nothing changes.
- **CHRD BUS:** on the CHORD page, CHRD = BUS makes one key play the whole current bus chord at the octave nearest the key. A held bus chord changes with the bus when RETRIG is CHORD or BOTH. A track without a bus (HARMONY off, or FOLLOW off) plays its own in-key triad and says **OWN DIA3**.
- **LINK, per track:** SOURCE (OFF, T1 to T4) copies the source's ROOT, SCL and QNT (TRN, CHRD and VOIC stay the track's own; a linked track is not offered as a source, so there are no chains); RETRIG (OFF, CHORD, LINK, BOTH); FOLLOW switches the track's part in the bus on and off.
- **The SCL pages** are now a family: SCALE, CHORD, LINK, HARMONY SETUP, and (with the bus on) HARMONY roots and durations, shown as two rows of four cells like ENV. **SELECT walks every row, forward and backward**; the SCL button steps through the same rows. Two quick SCL taps no longer lock the SCL layer.
  Rows that apply to all tracks (the bus) carry the badge **ALL** and name the key the bus is built in (for example `C MAJ T1`); the four slots read CHORD 1 to 4.
- **HOME ACTION: new key layout and a sixth bank, HARM.** With ACTION on (double-tap HOME): **C#5 steps one bank back, D#5 one forward** (hold either for SETUP), **D5, E5, F5 and G5 are A1 to A4**, F#5 returns to playing. The banks run RANDOM, JOURNEY, **HARM**, STASIS, PAIR, LIVE.
  HARM: A1 HOLD or RESUME, A2 PREV, A3 NEXT, A4 RESET; in SETUP, K1 is **SHIFT** (-7 to +7 scale degrees, the whole progression moved). Movement and SHIFT take effect at the next bar line. The four live chord names, the current chord and what is queued show on the HOME screen.
  The five HARM controls can be MIDI-learned (virtual IDs 2064 to 2068, earlier IDs unchanged). **If you know the 0.11.0 layout:** BANK was one key (D#5) and the four actions sat on E5 to G5; they have moved.
- **LATCH on the chord bus:** pressing a chord again in LATCH STACK ends only the notes no other latched chord holds; a key held on the panel or by MIDI keeps its notes.
- **Coupling and generative tools:** SCL + OCT+ (let go) copies the selected track's ROOT, SCL and QNT to all four tracks; GLO + OCT+ arms BAR RESET (all tracks go to step 1 at the next bar line while the transport runs); SEQ TOOLS G5 **GENERATE** writes a scale-aware phrase with chord tones on the strong steps while the bus is on; **ARP mode TURING** walks the held notes with a feedback register (no new parameter).
- **Fixes without a sound change in the checked renders:** signed-shift and overflow undefined behaviour in the DC blocker, Chorus, Delay, CRUSH, ANALOG, SAMPLE, FM6 and the DRUM renderers is removed (found with the undefined-behaviour sanitizer; 24 parent and fixed renders are byte-identical).
  A SLICE user-slot offset no longer indexes the built-in sample array.
- **Newer FM-1 units (flash UC25HQ80, JEDEC ID 0xB36014):** the boot check accepted only the Puya P25Q80 (0x856014), so on those units settings, projects and presets stayed in RAM and **firmware updates were refused**. Both parts are accepted now (as in Felucca, issue #320). Nothing else in the flash code changed.
- 0.11.0's changes (stereo ENSEMBLE, Pair Morph with a width) are included.

## What changes for you

- **Project format FUN11.** The project size is unchanged (the new state lives in spare bits of the song chain block), but the format has a new magic: **0.11.0 and earlier show a 0.12.0 slot as empty.** 0.12.0 loads every project that 0.11.0 loads (FUN10 and older), with HARMONY and LINK off. Going back is possible, but projects saved by 0.12.0 stay on 0.12.0. Back up before you update, as always (`tools/caravel_backup.py` or the installer's backup).
- **The ACTION keys moved** (see above).
- Nothing else changes unless you switch HARMONY on or use one of the new keys.

## What it costs

- **Flash:** the default layout is 578,504 B in the 581,564 B app slot (+18,104 B against 0.11.0), so **3,060 B are left** (one core 579,296 B, 2,268 B left; no RAM DSP 578,264 B, 3,300 B left). This is a thin margin for the next release.
- **RAM** `.data+.bss`: 69,584 B of 73,728 B in the default layout (+884 B), 65,516 B on one core; **4,144 B headroom, 48 B above the 4,096 B guard**; the pool is unchanged (358,472 B). A per-pitch ownership count for LATCH was tried and dropped because it broke that guard.
- CPU on the device is **not measured** (host instruction counts and the static target budgets pass).

## Known limits

- **HARMONY is one global loop of four chords**, not a progression per song section. The roots are scale degrees of track 1's scale.
- **LATCH STACK with three or more overlapping latched chords is not exact:** ownership is one bit per pitch, so an older chord's toggle cannot tell a note shared by two older chords from one it owns alone. Two overlapping chords are exact (tested).
- **CHORD EXT** (extensions of a played chord) is **not** in this release; it needs settings storage that does not fit the parameter space and the RAM headroom today.
- **External MIDI clock:** HARMONY's bar clock is aligned at every start (internal and external Start) and at BAR RESET, but the external clock's own bar sync (a resync while running) does not realign it, and a tempo change in the middle of a bar can shift HARMONY's bar line against the pattern's until the next PLAY or BAR RESET.
- A chord progression is counted in a 16-bit bar counter that wraps with it; after about 36 hours of continuous play without a stop a progression whose length is not a power of two can jump once.
- Some older DSP code still has signed shifts and sums that the undefined-behaviour sanitizer reports (not new, masked in the project's own sanitizer runs); a small block of UI code for the LINK and HARMONY graphs is never reached and costs flash.
- **ARP and LATCH:** switching ARP on while LATCH holds notes, or with several keys and STOP, can leave notes or counters behind that only a new press, the latch gesture or switching the ARP off clears (found by fuzzing; the same on 0.12.0's predecessor in the development line, not new). With CHRD BUS, STOP ends the newest bus chord only; another bus chord still held by an older key sounds until that key is released. A sequencer slide/tie step followed by a step that repeats the held note with its chance skipped can leave that note gated (older code).
- **HOME editor:** cells for FM6 operator, DRUM and LFO2 pages that an older firmware saved can open a neighbouring page from the editor's SOURCE action (two pages were inserted in the SCL page list); cells for ordinary parameters and the MOD page are handled.
- DRONE and ENSEMBLE are chosen with KNOB 1 in the engine picker (EDIT held); the picker's key grid has no cell for them.
- The Pair Morph browser tests ("experiments" job) and the ThreadSanitizer tests run in the hosted validation only, not in a local container on Apple silicon.

## Install

Back up first. Use the [web installer](https://bonszaimusic.github.io/caravel/webapp/installer/)
or download `Caravel-0.12.0-FM1-Testpaket.zip` from the
[release](https://github.com/bonszaimusic/caravel/releases/tag/v0.12.0). After reboot INFO must show `v0.12.0`.
Firmware: `caravel-0.12.0.fwsc`. Matching offline emulator: `Caravel-0.12.0-Emulator.html`. See the [FAQ](faq.md) and the
[installation guide](installing.md) for USB discovery, OCT-/OCT+ and recovery. The firmware identity is `FM-1_999`, the same as every Caravel release since 0.3.4, so the installer updates 0.11.0 in place.

## What is tested, and what is not

- Host tests with the real firmware code: the native UI test under strict UBSan (SCL and HARMONY pages, FUN11 round trip, LINK, CHRD BUS, LATCH ownership, the transport bar clock, MIDI Learn), the HARM panel test (39 groups, 313 checks), the ACTION and C/WASM emulator suites, the complete validation shards (`ui_storage`, `synthesis_midi`, `sanitizers`, `storage_usb`, `effects`, `cpu_dual`, `engines_samples`, `web_target`), the target-profile layout and budget gates of the three layouts.
  Every fix that answered the review rounds of 7 and 8 November came with a regression test that fails on the code before the fix.
- **Reviews:** the HARMONY work went through repeated source reviews while it was developed. Before this release it got a last **internal** review in several passes (fresh model instances without the author's context, four angles: state logic, persistence and format, build and release plumbing, real-time safety with sanitizer fuzzing; **not an independent review**, the reviewers are the same model family as the author). It found a blocker (the HARMONY bar clock after STOP/PLAY and BAR RESET), a build error in the ACTION workflow and a number of smaller problems, some of them side effects of earlier fixes of the same review; all were fixed with tests that fail before the fix, and the last passes found nothing that is new in the fixed state beyond the limits below.
- **On hardware:** the maintainer reports a good device test of an **earlier** 0.12.0 test package (the state of review round 4, identity `FM-1_999`) on their FM-1 on 2026-11-09, **without per-item results**: what was checked, how it sounded and read, with which clock and controller, is not recorded. **The firmware changed after that test** (the build fix and the UC25HQ80 detection, and the fixes of the internal review in round 7, among them the HARMONY bar clock); **none of these changes has been tested on a device**, and the released build has not been installed on one.
  An Android installer test of an earlier state was prepared; its result is not recorded.
- **Not covered:** any test on a real UC25HQ80 unit (the ID list is tested on the host only); a protocolled hardware, listening, readability, clock and controller test; the CPU on the device; the backup tool and the editor against a device holding FUN11 slots (no round trip exists); how 0.11.0 behaves when you save over a 0.12.0 slot (not tried with a device); a toggle key held across a bar line; ThreadSanitizer and the Pair browser tests in a local container.
