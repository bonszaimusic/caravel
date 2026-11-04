<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Caravel documentation

What Caravel adds to Felucca 1.6.1 and how it works. The list of changes is in [../CHANGELOG.md](../CHANGELOG.md); the origin and licences are in
[../NOTICE](../NOTICE), [../LICENSING.md](../LICENSING.md) and [../CREDITS.md](../CREDITS.md). Hardware acceptance on a real FM-1 is pending
for everything here unless a guide says otherwise; host tests are not hardware measurements.

- **[Tutorial: Random Morph and Morph Journey](tutorials/random-morph-journey.md)**: save sounds, morph between them and chain the morphs, step by step with real firmware screens.
- **[0.11.0 release: ENSEMBLE in stereo](release-0110.md)**: the width, the pad presets, Pair Morph with a width, what is and is not tested. ([0.10.1](release-0101.md))
- **[0.10.1 release: a fix for the ship on the RANDOM sea chart](release-0101.md)**: what changed, what is and is not tested. ([0.10.0](release-0100.md))
- **[0.10.0 release: the ENSEMBLE engine](release-0100.md)**: what changed, project compatibility, what is and is not tested. ([0.9.0](release-090.md))
- **[0.9.0 release: Random Morph and Journey per track](release-090.md)**: what changed, project compatibility, what is and is not tested. ([0.8.0](release-080.md))
- **[MORPH JOURNEY](morph-journey.md)**: chained snapshot morphs from the panel (JOURNEY bank) or MIDI, rules and MIDI table.
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

