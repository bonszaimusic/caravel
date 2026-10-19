# FX-special automation

The FX layer's fourteen special effects can be recorded with their press and
release times. Hardware listening and timing checks are still pending.

This page covers operation and limits.

## Playing and recording

1. Hold HOME, open AUDIO and choose FX TRACK (T1–T4, default T1). Select that track.
2. Tap REC to arm that track for **LIVE REC** and start playback. STEP REC does not capture FX.
   You may then select another sound track; FX recording still follows FX TRACK.
3. Hold FX (or lock its layer open) and play the white-key special effects.
4. Release a key to record its end. With FX LATCH ON, the second press records
   the end; releasing the key leaves the latched effect running.
5. Disarm REC after the take. The events repeat with that track's loop and play
   without holding FX. SAVE > PROJECT stores them with parts A–D as usual.

The FX still process the **master mix**. Their owning track supplies the loop
length, step division and timing; it does not isolate their audio to that track.
All fourteen effects are supported, including REPEAT, REVERSE, filters, TAPE
STOP, FREEZE, octave shifts, FLANGER, PHASER and REV/ECHO THROW. Reassigning the
key map later does not change the recorded effect identities. Black-key mutes,
solo, FX knob macros and incoming MIDI notes remain live controls.

The two edges of a short tap are separate records, even within one step. Timing
is stored in 1/256 of each swung step and rendered at the existing 32-sample
control-block resolution. It follows tempo changes and the external clock.
REPEAT and REVERSE retain their usual activation on the next 1/16 boundary.
Step CHANCE and note NUDGE do not suppress or move FX performance events.
Holds can cross steps and loop boundaries. Different effects stack as before;
the last activated buffer effect has priority. Live keys remain independent:
a recorded release cannot release a key you are holding manually.

## Editing start and duration

Select an FX ON or OFF row with KNOB1 or PRESETS on SEQ > AUTOMATION. KNOB2
changes STEP, retaining the fine position; KNOB4 changes only the fine position.
For example, ON at step 3 and OFF at step 7 lasts four steps: move ON to step 5
to shorten it to two, or move OFF to step 9 to lengthen it to six. OFF before ON
means a hold across the loop end. Steps use the track's swung clock; changing
the tempo keeps the relative timing.

While playing, the replay state and next deadline are refreshed at the current
playhead. Notes, transport and manual hold/latch controls keep their state.
Equal-time edges of the same effect show FX TIME OCCUPIED; the move and previous
undo stay intact. End any unfinished FX take on this track first (END FX REC
FIRST). Active SONG retains STOP TO EDIT. Other tracks and parameter events are
untouched. Edges are independent, so moving across other takes changes their
combined timeline; the normal redundant-OFF cleanup still applies on save/load.

## Deleting one performance

EDIT on OFF removes that row and the nearest ON before it; EDIT on ON removes
that row and the next OFF after it. Only the same effect on the same track is
considered. Fine position determines the order within a step, and the search
wraps at the track's loop boundary. Without an opposite edge in the active loop,
only the selected row is removed. Rows beyond LEN are dormant and deleted alone.
SAVE held undoes/redoes the two deletions together. Playback is refreshed at
the current playhead, preserving manual held/latched effects and transport.
An unfinished FX take on this track must end first (END FX REC FIRST).

This is chronological pairing, not a stored identity for the original take.
For ON at step 3, ON at step 5 and OFF at step 7, deleting OFF at step 7 also
deletes ON at step 5; ON at step 3 remains and can keep the effect active.
Moving an edge across other performances therefore changes which edge EDIT
chooses. All other events remain in place.

## Saving, clearing and limits

- SAVE during playback captures the duration of any recorded hold **up to the
  save moment**, while the live recording continues. The saved part initially
  resides in RAM and is committed after STOP.
- STOP, external-clock timeout and part changes release sequencer-owned FX.
  Live held/latched controls retain their existing behavior. Captured holds are
  closed before STOP or a live part switch so they do not become orphan events.
- SEQ > AUTOMATION lists each edge as FX, the effect name and ON/OFF. The cards
  also show its phase within the step. KNOB2 moves the selected ON or OFF to
  another step; KNOB4 edits its ON AT/OFF AT phase (0-255/256). The other edge
  stays put, so these edits change the start or duration independently. Effect
  identity and ON/OFF kind are fixed. SAVE held undoes/redoes each knob gesture.
  EDIT deletes the selected chronological pair as described above. UNDO
  restores both edges. CLEAR removes all automation on that track; PLAY OFF
  bypasses it. Other tracks' events stay intact.
- FX edges share the existing **128-record pool** with knob motion and parameter
  locks. A normal tap uses two records. A new captured ON reserves capacity for
  its OFF; when there is insufficient room, AUTOMATION FULL appears and the
  effect still works live. Identical overdubs reuse an existing edge; different
  timings add events, so disarm after a take or clear an old effect lane.

The project payload and event pool retain their sizes. New FX records use a
marked extension of FUN10; existing projects still load. Older firmware refuses
a project containing these new FX events, so export a backup before downgrading.
The existing Web MIDI editor's parameter-motion list excludes FX records; the
device/emulator list and project backup preserve them.

## Validation

`tests/fx_motion_test.c` exercises the real panel, sequencer, project, live part
and TRS MIDI-clock paths, plus latch, lifecycle, full-pool reservations and
malformed data. `web/emu/emu_test.mjs` checks the REC + FX/key gesture and replay
in WebAssembly. The firmware workflow runs the new test alongside the existing
full suite and target image, RAM and ISR-budget checks. Hardware listening and
timing checks remain pending; the release does not claim hardware validation.

## Project controls

FX TRACK is saved with the project and preserved when automation is cleared.
Older stores default to T1; no recordings are migrated. Changing owner closes
pending captures and releases old replay. Parameter automation remains per sound
track. SONG uses the working project's designated FX track; live part launch
adopts the new part's setting with its motion data at the boundary.

SAVE > PROJECT > INIT (KNOB2, OCT+, then confirmation OCT+) restores the boot project without
rebooting. OCT- cancels. It clears the working notes, automation, song order, recording arms and
performance state and restores default project values and sounds, including FX TRACK T1. Saved
slots A–D, pending RAM commits, USER presets, FM6 bank, MIDI-learn map and device preferences stay
intact. The fresh working project follows the usual autosave policy; no named slot is overwritten.

Redundant consecutive releases are compacted after capture and when saving/loading. All ON events
remain because they update buffer-effect priority; the first OFF survives for cross-loop holds.
