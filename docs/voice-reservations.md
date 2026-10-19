<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Per-track voice reservations

The four tracks share a budget of eight voices. Reservations let you give each track its own share. Hardware acceptance on a real FM-1 is pending.

Hold **HOME**, then turn **ALGORITHM** to **VOICES**. **OCT+** selects **FIXED**, **OCT-** returns to **AUTO**. The four knobs always set
T1..T4's reservations: 1..5 voices each, at most eight in total. Reduce a track first to make room for another. The prepared allocation defaults
to 2/2/2/2 and stays saved while AUTO is selected.

The tab's last row, **FX HOLD**, is not a voice setting: select it with PRESETS and it is an ordinary OFF / ON row (OCT and the knobs then change it, not the
reservations). With ON a factory preset load keeps the track's sends; see [Loading sounds and the track's FX](menus-and-pages.md#loading-sounds-and-the-tracks-fx).

In **FIXED**, idle reservations stay with their track. New notes, release tails, ARPs, UNISON and DRUM / DRUM-X hits replace only that track's own
voices, within the engine's existing polyphony limit. FX do not use synth voices. The CPU overload protection can still fade voices to protect
audio timing.

The allocation is a device preference, independent of project and sound loads; close the menu with HOME to save it. The web editor exposes the
same settings (menu ids 36..40). Settings written by Felucca (PER1..PER5) migrate to PER6 with AUTO selected, keeping MIDI Learn and the other
preferences. Older firmware cannot read PER6: back up before going back.
