<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# LATCH: notes that keep sounding

**Scope:** per track, the keys, the chord keys (CHRD), the arpeggiator and MIDI IN. Hardware acceptance on a real FM-1 is pending; the
behaviour below is covered by host tests (`tests/latch_test.c`), which are not hardware measurements.

LATCH keeps the notes you play sounding after you let go of the keys, so drones, pads, arpeggios and chords play hands-free. It works
on the **selected track's** setting: each of the four tracks has its own. It is on the **CHORD** page (SCL button, then SCL again), KNOB 3.

| Value | What it does |
| --- | --- |
| **OFF** | As always: a note ends with its key. |
| **NEW** | Notes stay on after release. A key (or chord, or MIDI note) pressed while **no other key or MIDI note is held** ends the latched set and starts a new one, so you change chords hands-free. Keys pressed while another is held add to the set. |
| **STACK** | Notes stay on and new ones are **added**. Pressing a key (or chord) whose notes **all** sound latched ends exactly those notes; its release does nothing. Use it to build a drone note by note and take notes out again. |

## What counts

- **Keys and chord keys:** with CHRD on, a key latches all the chord's tones; in STACK the chord key again ends all of them. If only some of
  the chord's tones are latched, the press adds the rest.
- **ARP:** the arpeggiator plays the latched keys. **NEW** works like ARP HOLD (the old chord is replaced by the next one once all keys
  were up); **STACK** adds to the list and a key again takes its note out. ARP HOLD on its own behaves exactly as before and also works without LATCH.
- **MIDI IN:** notes on the track's channels latch the same way. The sustain pedal (CC64) works as before.
- **MONO / LEGATO / UNISON:** the notes stay in the track's note stack (the eight most recent); in STACK, ending the sounding note falls back to the previous one.
- **Engines:** LATCH only withholds the note-off, so every engine sees an ordinary held note, DRONE (including its cell modes) too. A long
  ADSR release only starts when the notes end. Kits (DRUM, slices) take no latch.
- **MIDI OUT:** unchanged, it sends the keys' own note-ons and note-offs.
- **Recording:** step and live recording write the keys as you play them (key down to key up); LATCH only holds the sound, so a latched
  drone does not turn into endless steps. The sequencer plays over latched notes on the same track; voices are shared, so a full track steals voices as usual.

## Ending it

- **NEW:** play the next chord or note (see above). **STACK:** press the same key or chord again.
- **OCT- and OCT+ together:** let go of everything latched on the selected track (also an ARP HOLD chord). It also resets the octave to 0.
- **LATCH OFF:** ends every latched note of the track (a key still held plays until you release it).
- **ARP switched on** ends the latched keys of the non-arp mode; ARP switched off drops the arp's chord.
- **STOP** (also an external clock STOP) ends the latch of every track that has it on, including the arp's latched chord.
- **Panic:** a preset, engine or voice-mode change, MIDI All Sound Off (CC120), All Notes Off (CC123) and loading a project end the notes.
  **LATCH stays on**, and the next key latches again. A sound load does not change the LATCH setting (it is a track setting like ARP and SCL).

## Saving and compatibility

LATCH is saved with the track (project, user presets, editor protocol) like CHRD. It uses the track parameter that Felucca kept unused
(parameter 8, the retired "FX" slot of the ENV DEST page), so **no format changed**: projects and presets of Caravel 0.1.0 load as before, and a
firmware without LATCH ignores the value. Only the values 1 (NEW) and 2 (STACK) switch it on; any other stored value counts as OFF. The
latched notes themselves are never saved.

## Limits

- The latch is runtime state: the notes are not in the project, and a reset or loading a project starts without them.
- If the track runs out of voices the oldest latched note may be stolen like any other; a press of a latched note that no longer sounds starts it again instead of ending it.
- Up to 16 distinct notes are kept for the arp's list (as ARP HOLD); the arp plays at most the first four of a chord in CHORD mode.
