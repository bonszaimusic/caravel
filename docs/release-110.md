<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Caravel 1.1.0 (release candidate notes)

**Status: prepared, not published.** Host tests and the browser emulator pass (see *Validation*). A related test build was installed on one FM-1 and the owner reported the hardware test as successful; the final 1.1.0 package itself has not been installed or listened to. Previous release: [1.0.0](release-100.md).

## What is new

- **HARM in BUILD.** The centre of the BUILD orbit is a route map of the HARMONY links: the bus node (it shows the sounding chord's root, the next chord and its bars in the label) and the four tracks. One colour per role: FOLLOW green with an arrow, CHORD orange (C), CHORD EXT violet (E), KEY yellow and dashed. Routes are orthogonal; every route of the selected track is drawn in its colour (the accent) and thicker.
- **PLAY, HARM area.** K1 H ROOT, K2 **CHORD** (scrolls the four chords, taking effect at the next bar line and keeping the progress in the chord), K3 **LEFT** (the bars left in the chord, counting down; turn it up to hold the chord longer, up to 16 bars and then INF = HOLD; turn it down to shorten it, at least one bar stays), K4 LATCH. On a track that plays from the HARMONY bus or CHORD EXT, ROOT edits the global HARMONY key. The SCL hold layer follows the same rule (its K1 no longer changes the KEY FROM link).
- **Harmony position.** A line under the PLAY header shows the four chords by length, the elapsed part in light blue on the orange rest of the current chord with a pearl as the position, the next chord white; it follows LEFT edits and HOLD.
- **Orbit look.** Arcs have round ends; the end marker is a pearl; the fill is a soft tube. One colour rule: cyan = a value of the selected track, orange = a global value (no more G marks on the arc screens). ELASTIC is drawn as a rubber band (anchor ring, band with tension ticks, the armed thumb outlined).
- **LFO held** now makes the FX macros (FILTER, CRUSH, THROW, DEPTH) and the STASIS TONE momentary like the other PLAY arcs: they go back when LFO is let go.
- **LATCH STACK is not available for chords that come from the HARMONY bus or CHORD EXT** (stacked chords use up the voices); those tracks latch as NEW (a new set replaces the old one). Local chord shapes, plain and FOLLOW tracks keep STACK.
- **CHORD EXT from physical keys.** The source chord is the set of keys physically held when the gap (41 control blocks) ends; a single key teaches no chord, a perfect fourth or fifth is a valid power chord, keys held together count together (legato). RETRIG HARM/BOTH is reachable in the HARM page.
- **Independent HARMONY key and local CHORD shape** are saved in the project (FORMAT 2 reserved bytes); the integrated candidate also carries CAPTURE QUANT (hold REC, K2 QUANT), PLAY SAVE into PROJECT and the STASIS changes of the private candidate (see the commit history from `422e150`; not itemised here).

## Project format and compatibility

- **AFTERBURNER and the STASIS TONE are now part of the saved PROJECT** (1.0.0 kept them for the session only and reset them on INIT or LOAD). They are stored in the FORMAT 2 project without changing its size, offsets or checksum (a marker in two header bytes and the free top bits of the four HARMONY DURATION bytes). Projects with default values are byte-identical to 1.0.0; **a project saved with non-default AFTERBURNER or TONE is refused by 1.0.0 firmware.**
- The candidate that preceded this release could not save projects at all (the FX extension did not fit its reserved bytes); this release does.

## Validation (local; the hosted CI budget was exhausted)

`tools/docker_validate.sh host all` (prepare and all eight shards) passes, including the CHORD EXT fuzzers and the eleven mutations M1–M11 (all detected) and the differential baseline. Known gaps, stated plainly: the ThreadSanitizer tests do not run under the emulation on Apple silicon; `tests/capture_post_quant_test.c` and `tests/stasis_project_menu_ui_test.c` do not build yet and are listed as external/OPEN; the old 0.6.0 workflow `action-banks-android.yml` (job `wasm`) no longer matches the current UI and is not a valid gate; one baseline line (`chordext_format2_harm`, latch_stack) was changed on purpose for the STACK rule. Host tests and the emulator are not hardware measurements: RAM is 68,928 B of 73,728 B (headroom 4,800 B, pool headroom 10,848 B); CPU on the FM-1 and the added drawing cost were not measured.
