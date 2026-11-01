<!-- SPDX-License-Identifier: GPL-3.0-only -->
# MORPH JOURNEY (pilot)

**Pilot, not released, tested on the host only (no FM-1 yet).** It chains the snapshot morphs S1–S4 into one long, hands-free movement.
It is played from the panel (the JOURNEY ACTION bank) and, if you like, from MIDI controllers. Nothing about it is stored: no project,
snapshot or sound is changed, and a journey simply ends when you stop it.

## Play it from the panel

1. **Save snapshots in RANDOM** (HOME with ACTION on): set the sound, hold **A4** to save it into the selected slot; tap **A4** to move to
   the next slot, change the sound, hold A4 again. You need two to four snapshots of **one track** (TRACK scope).
2. **Tap BANK until JOURNEY** (it is the fifth bank, after LIVE). The title shows the slot and the state (`OFF`, `PLAY`, `WAIT`, `PAUSE`).
3. **A1 PLAY** starts: the sound morphs to the selected snapshot, rests, moves on to the next populated slot and loops
   (S1 → S2 → S3 → S1 …). Every leg starts from the values the sound has at that moment, so nothing jumps.

| Key | JOURNEY bank |
| --- | --- |
| A1 | **PLAY** / **PAUSE** / **RESUME**: starts, freezes the movement in place, continues |
| A2 | **DIR FWD / REV**: sets the direction. The running leg finishes, then the next one goes the new way (`REVERSE AFTER S3`) |
| A3 | **NEXT Sx**: skips the rest of the leg at once and goes to the next slot in the current direction |
| A4 | tap: **WAIT** steps 0 / 1 / 2 / 4 / 8 / 16 bars (the pause after each arrival); hold: **saves** the live sound into the selected slot |

**The JOURNEY bank has no SETUP of its own.** What a journey needs is already somewhere you know:

| Value | Where | Note |
| --- | --- | --- |
| leg length | RANDOM SETUP, **K1 BARS** | the same value RANDOM uses; during a running journey a change waits for the next leg and the display shows it |
| curve | RANDOM SETUP, **K4 SHAPE** | likewise shared and latched |
| WAIT | **A4 tap** in either bank | journey only |
| slot | RANDOM **A4 tap**, or the journey itself | the selected slot follows the journey; there is nothing to set |

Holding BANK in the JOURNEY bank only reminds you of this (`LEG+SHAPE: RANDOM SETUP`), and entering the bank leaves SETUP.
DEPTH and SCOPE exist only in RANDOM: a leg goes to stored values, and the controls it moves are already in the snapshot.

## How RANDOM and JOURNEY work together

- **One set of snapshots, one selected slot.** The slot shown in RANDOM (`S2*`) is the one the journey is heading for, so you can switch to
  RANDOM, look at it, and save a new sound into it with A4-hold.
- **Saving ends a running journey** (the moving leg is cancelled by a save): press PLAY to go on from the selected slot.
- **One movement at a time.** A RANDOM morph started by hand (RANDOM bank, A1) ends the journey. **A3 GO Sx** in RANDOM steers a running
  journey to that slot and it goes on from there. RANDOM's A2 FREEZE holds the current leg like PAUSE does.
- **The journey keeps running when you change bank**, so STASIS, PAIR and LIVE stay usable meanwhile.
- **Hands and controllers win.** A parameter you turn, or set from a MIDI CC, keeps its value for the rest of the leg; the next leg
  moves it again.
- **SETUP is one switch for the other banks.** Changing bank with SETUP on gives the knobs the new bank's values; the JOURNEY bank has none.

## MIDI (optional)

Start MIDI Learn (GLO + D4), go to HOME with ACTION on and pick a target: in RANDOM's SETUP K1–K4 pick BARS, DEPTH, SCOPE and SHAPE; in
either bank A1–A3 pick PLAY, DIR and NEXT, tap A4 picks WAIT, hold A4 picks SLOT (the keys only pick while learning).

| Target | CC behaviour |
| --- | --- |
| PLAY, DIR, NEXT | buttons: rising edge through 64 (a repeated high value or the release does not trigger again) |
| WAIT | absolute, 6 bands |
| SLOT | absolute, 4 bands: selects, never saves |
| BARS / DEPTH / SCOPE / SHAPE | absolute, 7 / 6 / 3 / 3 bands; held back while a leg moves, applied at the next leg |

## Limits of the pilot

2 to 4 populated snapshots of one track, TRACK scope and the same pinned controls, otherwise PLAY says `JOURNEY: SNAPS DIFFER` or
`NEED 2 SNAPS` and nothing moves. Starting replaces pending RANDOM return points, like an A3 recall. Leg lengths follow the tempo setting,
not an external MIDI clock. **Not tested:** a real FM-1 and controller, CPU and memory on the target, long runs, every engine.
