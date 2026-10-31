<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Caravel documentation

What Caravel adds to Felucca 1.6.1 and how it works. The list of changes is in [../CHANGELOG.md](../CHANGELOG.md); the origin and licences are in
[../NOTICE](../NOTICE), [../LICENSING.md](../LICENSING.md) and [../CREDITS.md](../CREDITS.md). Hardware acceptance on a real FM-1 is pending
for everything here unless a guide says otherwise; host tests are not hardware measurements.

- **[0.7.2 release and the stock-return fix](release-072.md)**: what changed, how to install, what is and is not tested.
- **[0.7.1 release and snapshot storage](release-071.md)**: saving S1–S4, compatibility, backup and installation.
- **[0.6.0 illustrated STASIS and performance tutorials](tutorials/README.md)**:
  HOLD, ADD/BLEND, safe project loading, HOME, Pair Morph, macros and saving.
- **[Original STASIS sound examples](https://bonszaimusic.github.io/caravel/sounds/)**:
  native firmware DSP renders with cue times, not recordings of an FM-1.
- [Troubleshooting & FAQ](faq.md): USB detection, firmware changes, stock-return error -5, UBOOT and recovery tools.
- [Installing Caravel](installing.md): web installer, command line, a specific release, going back to the official firmware.
- [Pages, groups and the settings menu](menus-and-pages.md): two parameter groups per screen, numbered footer, SELECT, MORE, EDIT and CLEAR STEP.
- [Per-track voice reservations](voice-reservations.md)
- [LATCH: notes that keep sounding](latch.md): NEW and STACK per track, with chord keys, ARP and MIDI IN.
- [FILTER: a low-pass / high-pass for every track](track-filter.md): one bipolar knob on the INSERT 2 page, the last stage of the track's chain, and what older firmware does with the saved data.
- [Modulation: LFO2](modulation/LFO2.md)
- [Engines](engines/README.md): [all engines and what they are good for](engines/overview.md), [tips for SEM, HADES and DRONE](engines/tips.md), the guides for SEM, HADES, DRONE and its cell modes, DRUM-X, MORPH and PHASE feedback, LOFI BYTE and FLOAT.
- [Sequencer and recording](sequencer/README.md): live parts and step recording, FX-special automation, DOUBLE, project INIT and the global FX
  track, external MIDI clock.
- [Editor protocol](../web/EDITOR_PROTOCOL.md)
- [Building](../BUILDING.md), [contributing](../CONTRIBUTING.md)

