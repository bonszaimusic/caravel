# Caravel 0.5.0 - Performance quickstart

[Download the six-page PDF](Caravel-0.5.0-Quickstart.pdf). Every screen below comes
from the actual firmware renderer; explanatory labels are outside the display.
This is documentation for 0.5.0 in preparation. The latest published version stays
0.3.3 until release approval.

## 1. Read and play HOME

SELECT chooses the active row, K1-K4 edit it, and ALGO selects the track. The active
row grows; the other stays compact. The preset name remains at the top. A star
means the sound differs from its saved preset. Pair members also show the partner
preset and effective morph share.

![HOME with either row active](img/guide-1.png)

## 2. Add and arrange controls

Turn a parameter on its original page. Hold GLO and tap HOME to add it. Hold HOME
for the menu, then HOME > EDIT CELLS to reorder or remove references. K1 selects
row, K2 cell, K3 action and K4 target. K4 acts immediately; OCT+ executes structural
actions and OCT- returns to HOME. SOURCE returns to the original page.

![Capture and edit HOME](img/guide-2.png)

## 3. Set up Pair Morph

Prepare two eligible pitched tracks; stop, release notes and let track tails finish.
Cycle ENV to PAIR. K1 chooses the partner, K2 enables the pair, K3 morphs and K4
switches optional MOTION. The partner takes A's voice mode. SPREAD must be off,
AUTO voices must be selected and only one Pair is available. The page explains
why a request is refused. Turn MORPH and use GLO+HOME to bring it onto HOME.

![Pair setup and live morph](img/guide-3.png)

## 4. Map a macro

Cycle ENV to MACRO. Turn the chosen macro and press OCT+ to open its map. K1 picks
the macro; K2 picks one of four slots; K3 chooses a continuous parameter on the
same track; K4 sets the signed amount. Example: FILT +32 and REV +16. OCT- returns
to the four macro controls. Macro value 0 is neutral. NONE or amount 0 disables a
route. Original parameter pages show base values, not the temporary macro sum.

![Macro mapping](img/guide-4.png)

## 5. Save the entire performance

SAVE > PROJECT > A-D keeps all tracks, HOME assignments, macro definitions/values,
performance events and Pair state. A preset keeps the sound, not the whole panel.
A save during PLAY takes a RAM snapshot; STOP commits it. Use matching tools for
complete USB backups, or tools/caravel_backup.py to transfer WORK/A-D as .hpm.
Keep user banks together with the performance. Older firmware lacks this layer.

![Complete performance storage](img/guide-5.png)

## 6. Second core and Track Mute

SYSTEM > 2ND CORE > ON (BETA), allow settings to save, then restart. HOME's 1C/2C
reports actual operation; disabling the worker also needs a restart. Second core
does not increase the eight-voice shared budget. Track Mute now ramps already
sounding LATCH voices down; held voices may resume on unmute and global FX tails
continue to decay.

![Headroom and muting](img/guide-6.png)

## Boundaries

Macros route within their own track; HOME can reference another track. New Macro
and Pair events use a bounded performance pool and are not shown in the native
event-list editor. Performance locks use assigned HOME cells; not every HOME target
gains automation. No freely named rows/cells, cross-track macro routes, CPU GUARD,
or V11 silent-FX experiment are included. No universal CPU percentage is promised.
The maintainer reports D3 passed hardware testing; the final 0.5.0-labelled package
still requires its install/version acceptance check.
