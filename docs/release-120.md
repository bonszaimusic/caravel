<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Caravel 1.2.0

**Status: release candidate, not published.** Built from a clean tree in release mode; host tests and the browser emulator pass locally (see *Validation*). A test build of the same STASIS 2.0 code was installed on one FM-1 and the owner reported no flicker, no problems and a good sound. The CPU load of this build against 1.1.1 was not measured, and the hosted CI was not available.

## What is new

- **STASIS 2.0 in the standard build.** The source is one full 65,536-frame PCM16 ring. HISTORY (the capture rate: 44.1 / 22.05 / 11.025 kHz) gives 1.5 / 3.0 / 5.9 s. ADD and BLEND are gone. FREEZE keeps the recording, NEW SOURCE (held) starts a fresh capture, RELEASE lets the clouds ring out. REGION chooses where in the history the grains read: **POS** slides the window, **LENGTH** sets its width, **REVERSE** plays the grains backwards.
- **PLAY SPACE** has five groups, each with four knobs: SENDS (DIST, CHOR, DLY, REV), FILTER, **TEXTURE** (GRAIN, STR, MOTN, TONE), **REGION** (POS, LENGTH, REVERSE, SOURCE) and **TANK** (TYPE, FEED, DRY, HALL). TEXTURE is the character, REGION the source material, TANK the signal path. With another reverb type, TEXTURE and REGION are hidden and TANK keeps TYPE reachable; the STASIS-only knobs show N/A.
- **DRY, FEED and HALL.** **DRY** (per track, TANK K3) sets how much of the track is heard beside STASIS; with DRY 0 you hear only STASIS. **FEED** is how much of the grains goes into the hall, **HALL** is what the hall returns; both are shown as 0 / 100 / 300 % (the middle of the knob is 100 %) and are not saved with the project.
- **Grains.** The sizes are **23 / 46 / 93 / 186 ms** (the 372 ms step is replaced by 23 ms; no extra cost per sample). **MOTN now scatters the grains across the REGION window**: before, both heads started 0.3 to 0.4 s behind the writer and followed the input. 0 is a fixed start, 127 uses the whole window. The direct grains are louder than before (x3) so that STASIS with HALL 0 is usable.
- **The V7 scope** draws the recorded PCM peaks, the real grain heads (their size and direction), the region, the STR ring, the MOTN dots and a centre glyph for the setting you turn (FEED, HALL, DRY, TONE, HISTORY). It refreshes at most 6 times a second and never blocks the audio.
- **BUILD FX in the PLAY look.** GRAIN and REGION are orbit pages with the scope in the centre, DLY shows its rails with "OFF: STASIS" while STASIS uses the delay memory, the page title replaces the first-use hint. BUILD stays recognisable: the BUILD tab with the warning mark, the warning stripe, the page number and the family tabs are unchanged.

## Project format and compatibility

- The REGION settings (POS, LENGTH, GRAIN, HISTORY, REVERSE) are saved in the FORMAT 2 FX extension with marker **A6**, and only when they differ from the defaults. Projects of 1.1.1 and earlier load unchanged; projects with default REGION settings keep the old layout. A project with non-default REGION settings is not known to 1.1.1 or earlier, which refuse it.
- The GRAIN value is a 2 bit index: 0..3 = 23 / 46 / 93 / 186 ms. The index of a given size changed against the private candidates (186 ms is now 3); no released firmware wrote A6.
- **DRY** is stored with the track in a parameter slot that no released version used (the old operator-4 level, id 80). FEED, HALL and the SOURCE state are runtime only.
- The default build stays with the 1.1.1 audio paths where STASIS 2.0 is not involved. Any of the new parts can be built out again (`CARAVEL_STASIS2=0`, `CARAVEL_STASIS_SCOPE_V6=0`, `CARAVEL_UX_BUILD_FX_PLAY_STYLE=0`).

## Known limit

The A4 audio test (HQ, FREEZE released) fails: when FREEZE is released in HQ, a grain that reads the source exactly at the point where the writer resumes meets a seam. It was not audible in the owner's tests. A short crossfade at that point exists as a build flag (`CARAVEL_STASIS_SOURCE_JOIN_EXPERIMENT=1`); it makes the test pass, costs about 160 bytes and is off by default because its effect on sound and on the audio interrupt load was not measured on the FM-1.

## Validation

See the release record in the metadata repository for the commit, the commands and the results. Not covered: CPU load on the FM-1 against 1.1.1 (the owner's baseline is about 68 %), the hosted CI, listening on other hardware, projects saved with A6 loaded on other firmware.
