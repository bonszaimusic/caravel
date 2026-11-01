<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Random Morph and Morph Journey - tutorial

**Status:** for Caravel 0.8.0 (not released yet). Everything below is tested on the host with the real firmware; a real FM-1 and
a controller are still to be checked. Every screen is the actual firmware renderer (HOME with ACTION on, SEM track), not a drawing.
The same tutorials, in one place: [README](../../README.md#tutorials) · [all tutorials](README.md) · [reference: MORPH JOURNEY](../morph-journey.md).

Random Morph moves the controls on your HOME page, by hand or to a stored sound. Morph Journey chains those moves into one long, hands-free
movement through up to four sounds. You need **HOME with a few controls on it** (hold GLO and tap HOME on a parameter's page to add it) and
**ACTION switched on** (double-tap HOME). BANK steps through the ACTION banks: RANDOM, STASIS, PAIR, LIVE and JOURNEY.

## 1. Save a sound into a slot

Make a sound you like with the HOME knobs. In the **RANDOM** bank, **hold A4** to save it into the selected slot; **tap A4** to select the next
slot (S1 to S4); change the sound and hold A4 again. A star (`S1*`) marks a slot that holds a sound. Two to four slots are enough.

![SNAP S1 SAVED](img/morph-save.png)

A snapshot remembers the HOME controls of the **track** (SCOPE TRACK) and their values. It is saved with the project (slots A to D).

## 2. Morph to a slot

Select a slot and tap **A3 (GO Sx)**. The controls glide from where they are to the stored values; nothing jumps. **A2** freezes the
movement in place and resumes it. **A1 (RANDOM)** is the other kind of move: every control drifts to a random value near where it is (how far: DEPTH, below).

![A morph on its way to S2](img/morph-go.png)

## 3. Set how it moves (RANDOM SETUP)

Hold **BANK** to switch SETUP on. K1 to K4 now set the morph:

| Knob | Setting | Values |
| --- | --- | --- |
| K1 | **BARS**: how long a move takes | 1, 2, 4, 8, 16, 32, 64 bars |
| K2 | **DEPTH**: how far a random move may go | 10, 25, 35, 50, 75, 100 % |
| K3 | **SCOPE**: what moves | ROW, TRACK, ALL |
| K4 | **SHAPE**: the curve | LINEAR, SMOOTH, DRIFT (three slightly different paths with the same ends) |

Hold BANK again to leave SETUP. BARS, DEPTH, SCOPE and SHAPE are saved with each project, so the performance sounds the same after reopening it.

![RANDOM SETUP](img/morph-setup.png)

## 4. Start a journey

Tap **BANK** until the title reads **JOURNEY** (the fifth bank, after LIVE). It shows the slot and the state: `OFF`, `PLAY`, `WAIT` or `PAUSE`.

![The JOURNEY bank, stopped](img/journey-off.png)

Tap **A1 (PLAY)**. The sound morphs to the selected slot, rests, moves on to the next slot that holds a sound, and loops
(S1, S2, S3, S1 ...). Each leg starts from the values the sound has at that moment. The leg length is the **BARS** value from RANDOM SETUP,
the curve is its **SHAPE**. A journey needs two to four slots of the same track.

![PLAY: on the way to the next slot](img/journey-play.png)

| Key | What it does in the JOURNEY bank |
| --- | --- |
| A1 | **PLAY**, then **PAUSE** (freezes the movement), then **RESUME** |
| A2 | **DIR FWD / REV**: the running leg finishes, the next one goes the new way |
| A3 | **NEXT Sx**: skip the rest of the leg and go on at once |
| A4 | tap: **WAIT** (the rest after each arrival: 0, 1, 2, 4, 8 or 16 bars); hold: **save** the live sound into the selected slot |

## 5. Rest between legs, change direction

Tap **A4** to set a **WAIT**: the journey then stays on each sound for that long. The title shows `WAIT` while it rests.

![WAIT 2: resting on S1](img/journey-wait.png)

Tap **A2** to reverse. The message says when it happens: **REVERSE AFTER S1** means the leg to S1 finishes first.

![DIR REV, after the current leg](img/journey-reverse.png)

## Good to know

- **The journey keeps running when you change bank.** Use STASIS, PAIR or LIVE meanwhile; come back to JOURNEY to steer.
- **Saving ends a journey.** A4 hold saves a sound, so PLAY starts again from the selected slot. A RANDOM move started by hand also ends it.
- **RANDOM's A3 (GO Sx) steers a running journey** to that slot and it goes on from there.
- **Hands win.** A knob you turn, or a MIDI CC, keeps its value for the rest of the leg; the next leg moves it again.
- **BARS, SCOPE and SHAPE changed during a journey wait for the next leg**, so a running leg keeps its speed. The display shows the waiting value.
- **A loaded project starts stopped** with S1 selected. Projects from before 0.8.0 get the defaults (BARS 8, DEPTH 35, TRACK, SMOOTH, WAIT 0, forward);
  0.7.x firmware refuses a project saved with these settings, so back up before moving between versions.
- **Tempo:** leg lengths follow the tempo setting, not an external MIDI clock.
- **MIDI:** every control above can be learned (MIDI LEARN: GLO + D4, then press the key or knob). See [MORPH JOURNEY](../morph-journey.md#midi-optional).

## If PLAY does nothing

`NEED 2 SNAPS`: save a second slot. `JOURNEY: SNAPS DIFFER`: the slots must be from one track, with TRACK scope and the same HOME controls.
Save them again after changing HOME.

*Screens are generated by `tools/morph_tutorial_frames.py`.*
