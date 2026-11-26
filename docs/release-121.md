<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Caravel 1.2.1

**Status: released 2026-11-26 (tag `v1.2.1`).** Host tests (all eight FORMAT 2 shards, including the sanitizers and the CHORD EXT fuzzers) and the browser emulator pass. **The release was not run on an FM-1** before publication.

## What is new

- **STASIS send per track.** While the reverb TYPE is STASIS, SENDS K3 (PLAY SPACE) and the third knob of the BUILD FX page set the track's **STASIS** send, 0 to 100 %, instead of DLY. **REV** is the send into the hall, **STASIS** the send into the ring; neither changes the other, and the two kinds of THROW add nothing to the ring. New projects and INIT start at 0. The DLY value is kept for HALL.
- **BUILD like PLAY** (STASIS only): TEXTURE (GRAIN, STR, MOTN, TONE), REGION (POS, LENGTH, REVERSE, SOURCE status), REVERB = TANK (TYPE, FEED, DRY, HALL) and SETUP (TYPE, CAPT, HISTORY).
- **Signal paths** on PLAY SENDS and TANK and on BUILD FX and REVERB:

```text
Track -- DRY ----------------------------------> output
Track -- CHO (chorus) -------------------------> output
Track -- REV --> hall ------------- HALL ------> output
Track -- STS --> ring/grains -- STR -----------> output
                 ring/grains -- STR x FEED ---> hall -- HALL --> output
```

  With STR at 0 the ring gives nothing, also not through FEED. Bright means the path can carry sound. A path set by a knob of the page has that knob's colour and pattern (K2 solid, K3 dashed, K4 dotted; other paths dash-dot), three line widths for the setting, and a second arrowhead while you turn it. The lines are not level meters.
- **Track mode symbols** (NOTES, FOLLOW, CHORD, EXT, CUSTOM) in PLAY and on BUILD pages of a track.
- **SOURCE** names its FX keys (B4 freeze, C5 new, D5 release); B4 and C5 restart the ring after RELEASE.

## Project format and compatibility

- The STASIS send uses a parameter slot that no earlier release used for anything audible (id 78). A stored value meaning "follow REV" (every 1.2.0 project) is read once as the track's saved REV value, also in motion data and HOME snapshots. This keeps the static setting, not the dynamic coupling to REV automation or THROW.
- A STASIS send at 100 % is written with a value that 1.2.0 and earlier do not accept: they refuse such a project. **Keep a backup of your projects before saving with 1.2.1** if you may go back.
- Reading an old project does not change the stored original; only saving does.

## Known limit

As in 1.2.0, the A4 audio test (HQ, FREEZE released) fails: a grain reading exactly where the writer resumes meets a seam. The crossfade for it stays a build flag, off by default.

## Validation

All eight FORMAT 2 host shards pass (`tools/docker_validate.sh host all`) and the three target layouts build; the release package is built from a clean tree in release mode. Not covered: anything on a physical FM-1 (display, listening, CPU load, restart after RELEASE for clicks), the hosted CI.
