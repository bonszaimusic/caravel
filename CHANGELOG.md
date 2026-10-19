# Changelog

What Caravel adds to Felucca and what it fixes in Felucca's own behaviour. Caravel is a fork of Felucca by Leo Kuroshita and
Hügelton Instruments (see [NOTICE](NOTICE)): it grew out of the author's use of **Felucca 1.5**, the first recorded commit builds on
**Felucca 1.5.1**, and the 1.6 and **1.6.1** sources were taken in later. Everything below is on top of Felucca 1.6.1; Felucca's own
release notes are in its wiki. Features that come from other people's work name their authors. Nothing here is tested on a real FM-1
unless it says so; host tests and simulations are not hardware measurements.

## [0.2.0] — 2026-10-19

### Added
- **LATCH** (CHORD page, KNOB 3): OFF, NEW or STACK per track keeps the notes of the keys, the chord keys and MIDI IN sounding after release.
  NEW replaces the latched set with the next press, STACK adds notes and ends them with the same key or chord again; with ARP the arp keeps the list.
  Ends with LATCH OFF, ARP on, STOP, a panic, or **OCT- + OCT+ together** (which also still resets the octave). See [docs/latch.md](docs/latch.md).
  It uses the unused track parameter 8 (the retired FX slot), so no project, preset or protocol format changes. In the web editor the
  inert FX slot is gone from ENV DEST and LATCH is on the CHORD page. Host tests only (`tests/latch_test.c`).
- **FX HOLD** (the fourth card of INSERT A / B / C, and MENU > VOICES, last row; OFF by default): with ON, loading a factory preset (PRESETS, an engine
  change, MIDI program change, the editor) keeps the track's four sends (DST CHO DLY REV) and its INSERT instead of setting the preset's sends and
  switching the INSERT OFF. The rest of the sound is still the preset's; the SLICER, ARP, SCL and the mix stay as they always did. A user preset
  still loads its own sends and INSERT. SOUND INIT, project INIT and power-on always start from the preset's sends; CLEAN INIT clears them. The
  global DLY, REVERB and CHORUS are never touched by a sound load. See
  [Loading sounds and the track's FX](docs/menus-and-pages.md#loading-sounds-and-the-tracks-fx). Editor menu id 41 (OFF / ON, VOICES tab); a device
  setting stored in the settings byte that already holds 2ND CORE and EXT START (bit 2), so user presets, projects and settings formats are unchanged.
- **The caravel**: Caravel's mark, a 28 x 17 pixel ship (72 bytes of flash, no RAM), is a faint watermark in the header, between the track and the
  transport, and sails over the power-on screen for about 0.6 s (MENU > ANIM OFF keeps the plain 0.4 s splash).
- **A tag in every group's header** says whether it is for **ALL** tracks (the global effects) or for the selected track (T1 to T4).
- **Pictures below a group**: SLICER shows its step pattern and what it does to a tone, INSERT the curve of its type, SCL its scale on the keys and
  CHORD the chord on each scale note (DIA3, DIA7) and the last chord's notes.
- **docs/installing.md**: installing a specific release (web installer and command line), checking it and going back to the official firmware; the
  command-line installer prints a risk notice before its question.

### Changed
- **The FX family** is four views: FX SENDS | DELAY, REVERB | CHORUS, INSERT TYPE / MIX | INSERT A / B / C and SLICER (before: FX | INSERT, INSERT 2,
  SLICER, DLY, REVERB, CHORUS).
- **The LFO family** is three views: LFO | LFO SYNC, LFO2 | LFO2 SYNC and LFO DEST | MOD. The MOD group shows SLOT, SRC, DST and AMT of the slot being
  edited, and four small pips in its header show which of the four slots are in use.
- **ANALOG's FILTER** shares a view with MORE (after VOICE); on SEM the MORE page is not shown (SEM MODE has SPREAD).
- **Every page with a card row frames it** in the theme colour (as the active group of a two-group page is), so the cards KNOB 1-4 edit look the same
  everywhere (not in the LINE style, the layers and the name entry).
- **The splash** shows the name, the version, the maker and the licence; the origin from Felucca stays in ABOUT and the credits. A development build says
  `-dev` after its version (a release build does not).
- Menu id 41 is FX HOLD.

## [0.1.0] — 2026-10-19 (first version under the name Caravel)

### Changed
- **Release/source delivery:** complete source with pinned engine files, matching notices, build identity and checksums;
  the installer and emulator use the selected release's source. Git-less source archives can rebuild the versioned binary.
- **New name and version line**: Caravel 0.1.0, based on Felucca 1.6.1. Display names, USB manufacturer and product strings, installer,
  editor and emulator texts, package names (`caravel-X.Y.fwsc`), ABOUT / CREDITS, and the legal texts ([NOTICE](NOTICE),
  [LICENSING.md](LICENSING.md), [CREDITS.md](CREDITS.md)). Data formats and package markers are unchanged, and the editor, installer
  and `tools/fm1_install.py` accept devices named Caravel and Felucca.
- **The FX family order** is FX, INSERT, INSERT 2, SLICER, DLY, REVERB, CHORUS (Felucca: FX, SLICER, INSERT, INSERT 2, ...).
- **SELECT starts on PAGE** on a device without saved settings (saved settings, including BPM, are kept).
- **EDIT on the STEP page opens the EDIT pages** like on every other page; it no longer clears the cursor step.
- **DOUBLE** (Felucca 1.6) also copies locks, automation and fine FX edges, up to 64 steps.

### Added: usability
- **Two parameter groups on one screen** for LFO, LFO2, ENV, ARP, VOICE, EDIT 1 / 2 (engines without a sound picture), LANES and
  FX / INSERT. The active group is framed and marked EDIT K1-4, SELECT steps through the groups, and the footer shows a numbered view
  ("LFO TOP 1/4"; a pair counts as one view). A small pictogram shows the selected LFO wave and the ENV shape.
- **In the settings menu, SELECT walks the rows** and continues into the next tab (with SELECT on PAGE).
- **CLEAR STEP: hold SEQ and press F5** clears the cursor step and its locks and moves on; one undo (SAVE held).
- **MORE page** (EDIT family): SPREAD, MORPH and PHS FB on one page; slots the engine lacks are empty. It replaces the single-parameter
  pages MORPH, FEEDBACK and VOICE 3.
- **Per-track voice reservations** (HOME held, ALGORITHM to VOICES): AUTO or FIXED, 1..5 voices per track, at most eight; also in the
  editor.

### Added: live performance and sequencer
- **Live part switching A–D** at track 1's loop boundary, **saving parts while playing** (RAM ONLY until STOP commits them to flash),
  and **LIVE LOAD FULL** (MENU > AUDIO), which also restores sounds, engines, track FX, FM6 patches, DRUM-X kits and the global FX.
- **STEP REC while the pattern plays** (SEQ held + REC, an input step independent of the playhead).
- **FX specials recorded** (all fourteen white-key specials with their timing), edited (start and duration of each FX event) and deleted
  (one performance, one undo); a **global FX track** (FX TRACK T1–T4, saved with the project); **project INIT** without a reboot.
- **External MIDI clock**: GLO + PLAY and GLO + F4 resync or start on the next bar or pulse, and EXT START (PULSE or BAR) for plain PLAY.

### Added: sound and modulation
- **LFO2**: an independent second LFO per track (speed, wave, phase, fade, sync, trigger, polarity), routable through MOD as LFO2,
  S&H2 or SLEW2. The former "LFO 2" settings page is named LFO SYNC.
- **Engines**: **SEM** (after the Oberheim SEM: two oscillators, LP / notch / HP blend, band-pass, voice drift, eight presets),
  **HADES** (after the Dreadbox Hades: dry bass voice with two phase-locked suboctaves, three-pole low-pass, drive and BODY, eight
  presets), **DRONE** (four coupled generators; experimental) with **cell modes** (TOPO CELLPAIR / CELLCHN / CELLRING, slow neighbour
  inhibition) and cell presets. SEM, HADES and DRONE are own designs, not emulations.
- **DRUM-X**: eight lanes with A/B patches and MORPH, modes, drive, layer, feedback, sends, global X-MOD and IMAGE, per-lane MIDI Learn.
  Adapted from **JIANT** by juanjiant-bit; the idea follows Sonic Charge's Microtonic.
- **MORPH** on ANALOG, PHASE, LOFI and DRUM-X, and **PHASE feedback**: adapted from **JIANT** by juanjiant-bit. Both support step
  locks, automation, MIDI Learn and MOD destinations.
- **LOFI BYTE and FLOAT waves** (32 formulas each, presets BYTEBEAT and FLOAT FM): adapted from **JIANT** by juanjiant-bit; the
  bytebeat and floatbeat formulas acknowledge Tejeez, xpansive and viznut.
- **Web editor**: support for LFO2, DRONE cells, DRUM-X, voice reservations and LIVE LOAD.

### Fixed in Felucca's behaviour
- **External MIDI clock drift**: sequencers, ARPs and the metronome keep fractional timing continuously, the clock counts fixed musical
  units, and external ARPs keep step overshoot. A simulated 20-minute run at 120 BPM drifted by about 109 ms in Felucca's 1/16 sequencer;
  the new regression allows 2 ms. Host simulation only.
- **TRS MIDI clock under CC load**: clock, start, continue and stop are timestamped during audio rendering, so heavy controller traffic
  no longer delays them.

### Storage and compatibility
- Settings grew append-only (PER5 to PER7: DRUM-X assignments, voice reservations, favorites of the new engines). Older settings import;
  older firmware cannot read PER6 or PER7 records, so back up before going back to Felucca.
- Sounds and projects with LFO2, DRONE cells, DRUM-X, MORPH or PHS FB need Caravel; existing Felucca projects and sounds load unchanged.
- Menu ids 32 and up are Caravel's (FX TRACK, LIVE LOAD, 2ND CORE, EXT START, voice reservations); 0..31 follow Felucca 1.6.
