<!-- SPDX-License-Identifier: GPL-3.0-only -->

# ELASTIC P1.4 — rhythmically timed springs and animated HOME cells (experimental)

> [!NOTE]
> **Historical design note.** This is the working note of the P1.4 prototype, kept for the reasoning behind the design. ELASTIC is released in Caravel 0.13.0: for what it does and how to use it, read the [ELASTIC first steps](tutorials/elastic-first-steps.md) and the [release notes](release-0130.md). The status lines and wording below (a private branch, "not a release", "not hardware-approved", checks "not yet run") describe the prototype at the time it was written and are **not** the status of the release.

**Status when written: private feature branch only, not a release, not hardware-approved.**
The experiment branched from the 0.13.0 line at `d3aeb0a`. It was not to be merged
into the release branch without an exact-target firmware/flash,
storage-compatibility, WASM, audio and real-FM-1 acceptance round.

## Performance controls

The ELASTIC PHYSICS page now treats **BOUNCE** as the duration of one complete
spring oscillation, not the number of bounces:

| Encoder selection | One complete cycle at 120 BPM |
| --- | ---: |
| 1/16 | 125 ms |
| 1/8 | 250 ms |
| 1/4 | 500 ms |
| 1/2 | 1 s |
| 1/1 | 2 s |
| 2 BAR | 4 s |
| 4 BAR | 8 s |
| 8 BAR | 16 s |
| 16 BAR | 32 s |
| 32 BAR | 64 s |
| 64 BAR | 128 s |

**The minimum selectable BOUNCE period is now 1/16** (the former 1/32 prototype index is reserved, cannot be selected or loaded as a valid tagged P1.4 setting). The units above follow the FM-1's four-quarter transport grid, independent of
other hypothetical time signatures. The period is stepped directly with K1
(no hidden raw-value snapping). PULL still controls the attack/gesture; RETURN
still controls the total **decay envelope**, with an added final **INF** option.
In INF mode the oscillator retains constant requested amplitude until HOLD,
RESET, OFF or target invalidation. REFLECT and RANGE keep the parameter legal.
There is no new audio voice or stored persistent oscillator state.

The phase is accumulated in quarter-note fixed-point units, following
`song.g[G_BPM]`, which is maintained by the internal tempo or filtered
external MIDI clock estimator. Tempo changes preserve phase rather than
restarting the wave. The initial knob/PULSE impulse is immediate; the first
impulse is **not delayed to the next bar**. HOLD freezes phase and RETURN
envelope time. On project reload arms, HOLD, positions and phases start at rest.

## Persistent project compatibility

Existing HPM1 **v4** projects encode BOUNCE as an untagged integer 1–1000.
The new project writer uses HPM1 **v5** while keeping the nine-byte-per-track
ELASTIC record unchanged. New rhythmic periods have bit-tagged `0x8000 | index`
values; the return encoder's new `101` value means infinity only for a
rhythmically tagged spring. Legacy numeric values remain accepted and
continue to use the old oscillator until the user changes the BOUNCE control.
The saved settings can be migrated back and forth by this firmware, but
**older 0.13.0 firmware cannot be assumed to open v5 projects**. Back up first.

## HOME display

When ELASTIC is the selected ACTION bank, supported cells keep their numeric
readout and show a small spring mark. Armed cells retain their ARM border and
dot. During actual PULL/RETURN (or coupled ROW movement), the larger moving
six-link spring takes the primary value area's place; the current value is
temporarily visible after a real encoder turn. The static base/value gauge is
retained. HOLD freezes the spring and adds the pre-existing pause mark.
Unrelated cells remain numerical. MOTION priority and the existing four-track
ARM HUD are unchanged.

## Evidence, limits, and follow-up checks

- A standalone local C harness for `home_elastic_sync.h`, compiled with
  `cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined`, passed
  discrete selection, previous 1/32 timing prototype (now disabled), 4-BAR phase, 64-BAR/128s period,
  tempo change and overflow-boundary checks.
- A separately recreated isolated C harness for the new trajectory path,
  compiled with `-fsanitize=undefined`, passed pulse, endless repeat,
  phase HOLD/RESUME, tempo changes, bounds and cancel. This is **not** an
  exact-source repository-wide test and does not verify the full firmware.
- New source tests: `tests/home_elastic_sync_test.c`, updated trajectory,
  HPM1 v5/v4 persistence, and `tools/test_elastic_p14_host.sh`. They are
  committed but **not yet run as a complete suite against the actual branch**.
- No SDK linker/layout measurements, WASM build, browser comparison, hardware
  audio/CPU/clock tests, compiler size report or public CI PASS is claimed.
- **Critical audio/sync risk:** the present publishing path is
  main-loop/control-rate and the HOME redraw is capped at 80 ms. At 120 BPM
  the new minimum 1/16 is 125 ms at 120 BPM and 62.5 ms at 240 BPM. A correct mathematical
  period does **not** prove sufficient audible/control update resolution; the
  real audio modulation could alias or sound stepped. This must be tested and,
  if necessary, moved to a bounded higher-rate interpolated modulation path
  while retaining the no-extra-voice and CPU2/ISR safety guarantees.
- Before review: validate external MIDI clock/tempo jumps/STOP/START, phase
  continuity, microstepping and zero crossings, all three firmware layouts
  (especially historically 108 B spare one-core), full v1–v5 SAVE/LOAD and
  corrupted input, original HOME regressions, CELL/ROW/MOTION/COUPLE, visual
  fidelity/contrast and owner hardware test.

Neither the previous 0.13.0 release commit nor public `bonszaimusic/caravel`
is changed by this prototype.
