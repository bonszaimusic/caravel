<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Live Harmony and HOME ACTION keys

Development feature; physical FM-1 acceptance is pending.

Double-tap **HOME** to enable ACTION. The physical key positions stay the same
when OCT, transpose or scale quantization changes:

| Key | Function |
| --- | --- |
| C#5 | Previous ACTION bank |
| D#5 | Next ACTION bank |
| D5 | A1 |
| E5 | A2 |
| F5 | A3 |
| G5 | A4 |

Tap a BANK key to change bank on release. Hold either BANK key for 500 ms to
open or close its SETUP; a long press does not also change bank. The actions
remain usable in SETUP. F#5 is an ordinary playable key. With ACTION off, all
keys play normally; a note already held when ACTION is enabled keeps its
normal release.

The forward order is **RANDOM → JOURNEY → HARM → STASIS → PAIR → LIVE → RANDOM**.
Backward selection follows the reverse order.

## Prepare the progression

SELECT moves between the rows only while **SELECT = PAGE** (the display preference); with
another SELECT setting use the SCL button. Open **SCL** and turn **SELECT** one detent per row:
**SCALE → CHORD → LINK → HARMONY SETUP → HARMONY ROOTS → DURATIONS**.
Turn left to follow the same rows backward; the first and last rows stop at
their endpoints. Fast turns consume every detent, including page transitions.
HARMONY SETUP is always accessible, including when Harmony is OFF. On that
row, turn **K1** right to enable the bus. The roots and durations become
available after it; they are hidden while the bus is OFF. SELECT only moves
the selection. K1–K4 edit the highlighted row. The SCL button also advances
through the rows and wraps back to SCALE. On HARMONY SETUP only **K1** works;
the rest of the row is a hint.

Enable the bus on **SCL > LINK**, lower HARMONY SETUP row. Set the four roots
and durations on **HARMONY**; a root turn moves along the scale degrees of T1's
scale, and each name shows the chord built on that root (C, Am, F, G in C major).
A root stored outside the scale (an older project, or a scale changed later) plays and
shows as the scale note below it; the stored value is not rewritten. With SHIFT on, the
roots stay the prepared chords (PREP); the slot that sounds now also shows LIVE.
The four slots are **CHORD 1..4**; each lasts 1, 2, 4 or 8 bars (the DURATION row). The
HARMONY, DURATION and SETUP rows carry the **ALL** badge (they are global, not the selected
track's) and name the key the bus is built in, from T1's ROOT and SCALE (for example
C MAJ T1). The line under the DURATION cards says what HARMONY does per note source (ARP: every
note; SEQ: steps 1 5 9 13) and, with SHIFT, the shift in degrees (scale steps, not semitones).
On **LINK**, the track switch is **FOLLOW** (K4: turn right ON, left OFF) and the bus
itself is **HARMONY**. Set FOLLOW on each participating track; enable
RETRIG CHORD/BOTH for a held CHRD BUS chord to change with the bus. Kits, slices and
other engines that map the keys themselves are never moved by Harmony.

## Play the HARM bank

| Action | Effect |
| --- | --- |
| A1 HOLD / RESUME | Hold the current harmony while rhythm and ARP keep running. RESUME starts the next chord at the next transport bar. |
| A2 PREV | Queue the previous chord, wrapping from chord 1 to chord 4. |
| A3 NEXT | Queue the next chord, wrapping from chord 4 to chord 1. |
| A4 RESET | At the next bar, return to chord 1, clear HOLD and set SHIFT to zero. |
| SETUP K1 SHIFT | Queue a shift of the entire progression by -7 to +7 scale degrees within T1's scale. Zero is the original progression. K2–K4 are unused. |

The bank's status line spells out what is queued for the next bar (PREV BAR, NEXT BAR,
RESUME: NEXT BAR, RESET: CHORD 1, SHIFT 0) or the run state (RUN, HOLD) and the SHIFT in
degrees (DEG). In the other banks, and with ACTION off, a line under the bank (HARM HOLD,
SHIFT, a queued command) keeps the state in view while HOLD, a SHIFT or a command is on.
A track whose CHRD is BUS but has no bus (HARMONY off, or FOLLOW off) plays its own in-key
triad; its CHRD card says **OWN DIA3**.

PREV, NEXT, RESET and SHIFT take effect at the next **transport bar line**.
The most recent PREV/NEXT/RESET request replaces any earlier queued movement.
An explicit jump starts that chord's full configured duration. PREV/NEXT while
held choose a different held chord; RESUME releases the hold. Press HOLD again
before a queued RESUME to cancel that resume. When stopped, changes are applied
immediately without starting any notes or the transport.

The four displayed chords are the effective live chords, with the current one
highlighted. HOLD, SHIFT and starred queued actions are shown below them;
the star means the next bar. The regular HARMONY editor continues to show the
prepared roots. SHIFT changes scale degrees/pitch classes, not the keyboard's
octave: C–Am–F–G in C major becomes Dm–Bdim–G–Am at +1. A complete turn around
the scale repeats the same pitch classes.

The overlay is global; FOLLOW still decides which tracks participate. Changing
ACTION bank keeps the overlay active, allowing STASIS and HARM to work together.
Switching ACTION off also keeps it active. Return to HARM and use RESET to
restore the original progression.

HOLD, queued movement and SHIFT are temporary and are not written into the
prepared roots/durations or project format. A new PLAY/Start, BAR RESET,
project initialization/load, or turning the bus off/on clears the overlay.
Stop/Continue preserves the overlay and any requests already waiting for a bar. The same bar scheduling follows internal BPM or
external MIDI clock; external clock loss retains the existing transport-stop
behavior.

## MIDI Learn

Enable MIDI Learn, then press A1–A4 in HARM to select HOLD, PREV, NEXT or RESET;
turn SETUP K1 to select SHIFT. Send the desired controller CC and finish Learn.
The four buttons trigger on a low-to-high crossing (values below 64 to 64 or
above), so a repeated high value does not fire repeatedly. SHIFT is absolute:
0 = -7, 64 = zero, 127 = +7. Existing MIDI Learn mappings keep their IDs.

Learned HARM controls work while another ACTION bank is visible, including
STASIS. They control the internal Harmony bus; this feature does not add MIDI
OUT for Harmony, ARP or sequencer notes.
