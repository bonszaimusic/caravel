<!-- SPDX-License-Identifier: GPL-3.0-only -->
# MORPH JOURNEY

**Planned for 0.9.0, not released: this page describes the development branch (per-track snapshot sets and one morph engine per track). Released in 0.8.0 with four slots shared by the whole project; tested on the host and in CI, tried on an FM-1 by the maintainer, not verified with a controller or external clock.** It chains the snapshot morphs S1–S4 into one long, hands-free movement.
It is played from the panel (the JOURNEY ACTION bank) and, if you like, from MIDI controllers. Playing a journey changes no snapshot and
no stored sound, and it ends when you stop it. What *is* kept with the project are the snapshots (four per track) and the performance settings (BARS,
DEPTH, SCOPE, SHAPE, WAIT, direction); the running journey itself (PLAY/PAUSE, the countdown, the selected slot) is not saved, see
[Saved with the project](#saved-with-the-project).

## Play it from the panel

1. **Save snapshots in RANDOM** (HOME with ACTION on): set the sound, hold **A4** to save it into the selected slot; tap **A4** to move to
   the next slot, change the sound, hold A4 again. You need two to four snapshots **on the selected track** (see *Every track has its own slots* below).
2. **Tap BANK once in RANDOM: JOURNEY is the bank right after it** (the order is RANDOM, JOURNEY, STASIS, PAIR, LIVE). The chip names the bank; on the sea chart the caravel is the journey: it sails from buoy to buoy while a leg moves and lies at anchor when the journey is off, waiting (`WAIT`), paused or held.
3. **A1 PLAY** starts: the sound morphs to the selected snapshot, rests, moves on to the next populated slot and loops
   (S1 → S2 → S3 → S1 …). Every leg starts from the values the sound has at that moment, so nothing jumps.

| Key | JOURNEY bank |
| --- | --- |
| A1 | **PLAY** / **PAUSE** / **RESUME**: starts, freezes the movement in place, continues |
| A2 | **DIR FWD / REV**: sets the direction. The running leg finishes, then the next one goes the new way (`REVERSE AFTER S3`) |
| A3 | **NEXT Sx**: skips the rest of the leg at once and goes to the next slot in the current direction |
| A4 | tap: **WAIT** steps 0 / 1 / 2 / 4 / 8 / 16 bars (the pause after each arrival); hold: **saves** the live sound into the selected slot |

A journey plays the **selected track's** slots. Everything else it needs is already somewhere you know:

| Value | Where | Note |
| --- | --- | --- |
| leg length | RANDOM SETUP, **K1 BARS** | the same value RANDOM uses; during a running journey a change waits for the next leg and the display shows it (likewise **K3 SCOPE**) |
| curve | RANDOM SETUP, **K4 SHAPE** | likewise shared and latched |
| WAIT | **A4 tap in the JOURNEY bank** (steps 0 / 1 / 2 / 4 / 8 / 16 bars), or a CC | journey only. **In RANDOM, A4 tap selects the next snapshot slot instead** |
| slot | RANDOM **A4 tap** (next slot), or the journey itself | the selected slot follows the journey; in the JOURNEY bank A4 tap is WAIT, so the slot is not changed there |

DEPTH and SCOPE exist only in RANDOM: a leg goes to stored values, and the controls it moves are already in the snapshot.

## Every track has its own slots

Each of the four tracks has **its own four snapshot slots S1–S4** (sixteen in the project). A slot holds the controls pinned on that track's HOME page
(SCOPE TRACK; ROW: the active row only) with their values at that moment. **Only the track's own controls are stored**: if the HOME of T1 also pins a
control of T2, it is not part of T1's slot (T2 has its own). Controls that belong to no track (global and Pair controls) are stored with the track
that saves them. A slot holds at most **32 targets**; if the HOME has more, nothing is saved and the screen says `TRACK 1: MAX 32 TARGETS`.

- **SCOPE ALL saves every track at once.** Hold A4 with SCOPE ALL and each track's own slot is written from its own HOME (`SNAP S3 SAVED: 2 TRACKS`;
  a track without controls of its own is left out). Deleting with SCOPE ALL empties the slot on every track.
- **A4 and the chart belong to the selected track.** `S2*` on the A4 key and the buoys of the sea chart show the selected track's slots (a buoy is filled when
  that track's slot holds a sound), and tapping A4 says `SNAP S2: 14 CONTROLS` or `SNAP S2 EMPTY`. Select another track and the chart shows that track's slots.
- **A journey plays the selected track's slots.** It takes the track selected when you press PLAY; the other tracks' slots do not matter. A track with
  one filled slot has no journey (`NEED 2 SNAPS`). Saving or deleting a slot on **that** track ends its journey.
- **Gaps are skipped.** Empty slots do not take part: with S1, S3 and S4 filled a journey goes S1 → S3 → S4 → S1.

## JOURNEY SETUP: SCOPE and TRANSPORT

Hold BANK in the JOURNEY bank: **K1 SCOPE** (`THIS` or `ALL`) and **K2 TRANSPORT** (`FREE`, `CONT`, `START`). Both are saved per track with the project.

- **SCOPE ALL:** the keys act on **every track's journey**. PLAY starts every track that has two filled slots (`JOURNEY PLAY: 3 TRACKS`); it follows the selected track after that: if that one runs, PLAY pauses all running journeys, if it is paused, PLAY resumes the paused ones. NEXT skips the leg on every running journey. DIR and WAIT are set to the selected track's new value on every track. The cycles stay **independent**: each track steps through its own filled slots with its own BARS, SHAPE and WAIT, so with unequal gaps (T1: S1, S3, S4; T2: S1, S2, S3) the journeys drift against each other by design. `THIS` (default): the keys act on the selected track only.
- **TRANSPORT** says what keeps the journey's time, as the sequencer follows its own clock source (internal tempo or external MIDI clock):
  - **FREE** (default, as in 0.8.0): milliseconds at the tempo setting. The journey keeps moving with the sequencer stopped and ignores an external clock.
  - **CONT** (continue): the time is the **transport's musical time**: a leg of 8 bars takes 8 bars of the running clock, a tempo change (or an external tempo) speeds it up or slows it down, and **Stop freezes the leg and the WAIT**. Start or Continue let it go on from where it stood. PLAY with the transport stopped says `JOURNEY: WAITS FOR PLAY`.
  - **START:** like CONT, and every **Start** (PLAY from stop; an external MIDI Start) also **begins the journey again** from its first filled slot in its direction, with a leg from the live values. An external MIDI Continue only resumes.

RANDOM morphs stay free (milliseconds). Held, paused and PLAY-again follow the same time base. **Not hardware-tested:** the transport time is host-tested with the internal clock only; the external clock uses the same counter but is not verified.

## One engine and one set of settings per track

Every track has **its own morph engine and its own journey**, and its own **BARS, DEPTH, SCOPE, SHAPE, WAIT and direction** (saved per track with the project). A journey on T1 and a journey or RANDOM morph on T2 run at the same time, each
with its own phase, length and rests; the keys, the A4 label and the sea chart always show the **selected** track. Each engine holds **up to 32 targets** (the controls of its own HOME page, plus parked RETURN origins): a track whose HOME pins more cannot start a RANDOM morph
(`RANDOM: TOO MANY TARGETS`, nothing moves).

**A control two tracks move belongs to the track that started last** (a control pinned on two HOME pages, or the Pair morph amount): the morph or journey leg that starts takes it over, the other lets go until it starts again.

**SCOPE ALL** (RANDOM SETUP, K3) makes the RANDOM keys act on every track: A1 starts a morph on each track that has controls (`RANDOM START: 4 TRACKS`), A2 freezes or resumes them all, A3 returns them all or morphs each track to its own slot (`MORPH TO S2: 3 TRACKS`). Each track uses its own BARS, DEPTH and SHAPE. The journey has its own SCOPE (JOURNEY SETUP, above).

## How RANDOM and JOURNEY work together

- **One selected slot.** The slot shown in RANDOM (`S2*`) is the one the journey is heading for, so you can switch to
  RANDOM, look at it, and save a new sound into it with A4-hold.
- **Saving or deleting a snapshot of the journey's track ends a running journey** (its set of slots changed): press PLAY to go on from the selected slot. Saving or deleting on another track leaves it alone.
- **A RANDOM morph on a track holds that track's journey** (other tracks' journeys are not affected). A RANDOM morph started on the journey's track, by hand (RANDOM bank, A1) or its RETURN,
  **holds** the journey: it stops where it is (`HOLD`), and **when that morph is done it goes on by itself**, with a new leg from the live values
  to the same slot (or, if it was resting, with the rest of the pause). The picture below shows it. **A1 while held is PAUSE**: then it stays
  paused until you press A1 again. **A3 NEXT** or **A3 GO Sx** in RANDOM steer the journey to a slot at once. RANDOM's A2 FREEZE on the journey's own
  leg holds it like PAUSE does; a frozen RANDOM morph keeps the journey held.
- **What every track does is always shown, in every bank:** four small boxes under the bank chip are T1 to T4 (accent fill: its journey sails or rests; outline: paused or held; theme fill: a RANDOM morph or GO moves it; grey: idle; bright frame: the selected track).
- **What is running is always shown, in every bank.** The sea chart under the HOME rows is the status display: the caravel is the running morph or journey.
  It **sails** from the slot it left to the slot it heads for (by the progress of the leg) and bobs while it moves; it lies **at anchor** (a short line
  below the hull) while the journey is off, waits, is paused or held, or no morph runs. A RANDOM morph or a GO to a slot also sets it sailing. The
  buoys are S1 to S4 of the selected track: filled when its slot holds a sound. The chip on the left names the bank (or `SETUP`).
  Messages (`SNAP S1 SAVED`, `JOURNEY STOPPED` ...) appear in the top bar as before.
- **The journey keeps running when you change bank**, so STASIS, PAIR and LIVE stay usable meanwhile.
- **Hands and controllers win.** A parameter you turn, or set from a MIDI CC, keeps its value for the rest of the leg; the next leg
  moves it again.
- **The JOURNEY bank has its own SETUP** (hold BANK there, see below). Entering the bank leaves any open SETUP. Leg length and curve are RANDOM's BARS and SHAPE of the track.

![Diagram: a RANDOM morph on T1 holds only T1's journey, which goes on by itself](tutorials/img/journey-hold.png)

## Delete a snapshot

In RANDOM, hold BANK to open SETUP, select the slot with A4 tap, and **hold A4**: `SNAP S3 DELETED`. Outside SETUP, A4 hold still saves, so a
deletion cannot happen by a simple save. A slot that is already empty says `SNAP S3 EMPTY`. The delete acts on the selected track (SCOPE ALL: on every track) and ends that track's running journey. A deleted slot
is saved as empty with the project.

## Saved with the project

The performance settings are saved with each project, next to the sixteen snapshots (slots A–D, RESTORE LAST, USB backup, export and import):
**BARS, DEPTH, SCOPE, SHAPE, WAIT, the direction, the JOURNEY scope and the TRANSPORT option**. A project saved with BARS 16, SHAPE DRIFT and WAIT 4 sounds the same after reopening it.
If a change is still waiting for the next leg when you save, the waiting value is the one saved.

**Not saved:** a running journey, PLAY / PAUSE, the WAIT countdown, RETURN points, and the selected slot. A loaded project always starts
**stopped**, with S1 selected. Projects made before this feature get the defaults: BARS 8, DEPTH 35, TRACK, SMOOTH, WAIT 0, forward.
**Project format (0.9.0): version 3.** The sixteen sets fit the same protected flash copy (worst case measured in the host test: 2,355 of 3,328 compressed bytes), but
**0.8.0 and earlier refuse a version-3 project** (`PERF PROJECT INVALID`) instead of misreading it: back up before moving between versions.
**Projects from 0.7.1–0.8.0 load and migrate without loss of what they hold:** each target of an old shared slot goes to the slot of the track it belongs to (global and Pair
controls to the track that saved it), at most 32 per slot; anything over the limit is dropped and the load says `n TARGETS DROPPED`. The first save writes the new format.

## MIDI (optional)

Start MIDI Learn (GLO + D4), go to HOME with ACTION on and pick a target: in RANDOM's SETUP K1–K4 pick BARS, DEPTH, SCOPE and SHAPE; in
either bank A1–A3 pick PLAY, DIR and NEXT, tap A4 picks WAIT (while learning, a hold of A4 learns nothing and saves nothing; outside learning, A4 tap is WAIT in the JOURNEY bank and the next slot in RANDOM).

| Target | CC behaviour |
| --- | --- |
| PLAY, DIR, NEXT | buttons: rising edge through 64 (a repeated high value or the release does not trigger again) |
| WAIT | absolute, 6 bands |
| BARS / DEPTH / SCOPE / SHAPE | absolute, 7 / 6 / 3 / 3 bands; held back while a leg moves, applied at the next leg (SCOPE resets the morph, so it must wait) |

## Limits

At least two populated snapshots on the selected track, otherwise PLAY says `NEED 2 SNAPS` and nothing moves. Starting replaces pending RANDOM return points, like an A3 recall. Leg lengths follow the tempo setting,
not an external MIDI clock. **Not verified:** a controller, external MIDI clock, CPU and memory under real load, long real-device runs, every engine.
