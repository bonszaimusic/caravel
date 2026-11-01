<!-- SPDX-License-Identifier: GPL-3.0-only -->
# MORPH JOURNEY (pilot)

**Pilot, not yet released, tested on the host only.** It chains the snapshot morphs S1–S4 into one long, hands-free movement and is
played from MIDI controllers. Nothing about it is stored: no project, snapshot or sound is changed, and a journey ends when you stop it.

## What it does

With two to four snapshots saved (RANDOM bank: hold A4 to capture), PLAY morphs from the current sound to the selected snapshot, rests
there, then moves on to the next populated one, and loops: S1 → S2 → S3 → S1 … Every leg starts from the values the sound has at that
moment, so nothing jumps. The leg length is the RANDOM **BARS** setting; the **SHAPE** and **DEPTH** settings apply as in RANDOM.

**Pilot limits:** the populated snapshots must be of one track, with TRACK scope and the same pinned controls; otherwise PLAY says
`JOURNEY: SNAPS DIFFER` or `NEED 2 SNAPS` and nothing moves. Starting a journey replaces any pending RANDOM return points, like a recall
with A3 does. A RANDOM, RETURN or another morph started by hand ends the journey. A physical A3 recall during a journey is not followed.

## Controls (MIDI Learn targets)

Start MIDI LEARN (GLO + D4), go to HOME with ACTION on and the RANDOM bank, then pick a target and send its CC:

| Pick with | Target | CC behaviour |
| --- | --- | --- |
| A1 | PLAY | button: play / pause (rising edge through 64) |
| A2 | DIR | button: forward / reverse (turns back toward the slot the leg came from) |
| A3 | NEXT | button: skip to the next slot |
| tap A4 | WAIT | absolute, 6 bands: rest 0 / 1 / 2 / 4 / 8 / 16 bars after each arrival |
| hold A4 | SLOT | absolute, 4 bands: selects S1–S4 (steers a running journey there smoothly; never saves) |
| K1–K4 in ACTION SETUP | BARS, DEPTH, SCOPE, SHAPE | absolute, 7 / 6 / 3 / 3 bands |

A button needs a rising edge (below 64, then 64 or more): a repeated high value or the release does not trigger again. Setting
changes from a CC are held back while a leg moves and take effect at the next leg. Learned CCs are stored with the other MIDI Learn
assignments; with MIDI Learn on, the keys above only pick a target and do nothing else.

## Hands and controllers win

A parameter you turn, or set from a CC, keeps its value for the rest of the leg; the next leg moves it again. Pausing freezes the
leg in place; A2 FREEZE also holds it.

## What has and has not been tested

Host tests of the real firmware (chained legs reach each snapshot's values, edge detection, pause, direction, next, wait, slot, latched
settings, control by hand, learn entry points). **Not tested:** a real FM-1, long runs, CPU and memory on the target, external MIDI
clock (leg lengths follow the tempo setting, not an external clock), and the interaction with every engine.
