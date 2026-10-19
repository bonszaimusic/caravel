#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Host tests of the Felucca sources (no hardware). Run from the repo root after ./build.sh:
#   tests/run_tests.sh
#
# Regression suite (tests/regress.c, tests/target_budget.py; details at the top of regress.c):
#   golden renders  every engine x preset, the GM map on DRUM, voice modes, FX sends, a 4-track mix: one hash
#                   each in tests/golden.txt. A change of the sound fails with the list of renders.
#   health          clipping, DC, peak level, voices free after the release, silence at the end.
#   CPU             instructions / sample per preset and mix (tests/cpu_baseline.txt, +25 %), ns printed;
#                   target: loop instructions of the render functions in the pi32v2 disassembly
#                   (tests/target_budget.txt, +10 %; exact, static).
#   voices          the budget of 8, steal fades, MONO / LEGATO / UNISON keep their note, the VOICE cap,
#                   no hanging notes on any MIDI / key routing.
# FELUCCA-ENGINES (engines/tests, MIT): ANALOG LOFI TRIO WHEEL NOISE on their own host against their golden renders,
#                   once with their own tables and once with build/gen's (the two generators agree).
# ENGINE MEMORY (tests/union_test.c, 1.5): engine switches on a part (every pair and A -> B -> A of the engines that
#                   share the part's state region, with others), hashed against 1.4's (tests/union_golden.txt).
# UI renders (tests/ui_render.c): every screen in every palette from the real drawing code: the layout lint (no text
#                   off the screen, cut, hidden, overlapping or spilling out of its cell / card; only free text
#                   ellipsised), GREY gray, MONO neutral, the draw cost, the text audit (build/ui_new/text_audit.tsv);
#                   PNGs of GREY MONO GREEN PAPER NIGHT in build/ui_new (tests/ui_render.py), the findings in build/ui_new/report.txt;
#                   FM6's 32 algorithm charts as drawn (no box overlapping, no route through a box or crossing another), again
#                   on OPERATOR with each operator picked and its frequency text at its widest (1.2, #76);
#                   DIGITAL's screens (its algorithm charts, OP ENV) with FELUCCA_FM4=1 too (build/ui_fm4: lint, GREY, MONO);
#                   every frame of the rolling digits (lint, GREY, MONO), their filmstrips in build/ui_slot;
#                   the alignment: every text / icon / keycap meant to be centred, or on its neighbours' line, by its
#                   ink against its box (cells, chips, buttons, rows, knobs, the roll's strip, the keycaps' pills), every
#                   screen in FLAT and LINE and every palette, over every value it can show; 1 px off or more fails
#                   (build/ui_new/align.txt). MENU > LARGE: every screen again (FLAT, LINE, every palette) and the page / value
#                   sweep with the tall cards, the same lint and alignment; sheet_LARGE_GREY.png, sheet_LARGE_MONO.png.
# UI (tests/ui_test.c): the UI sources against stub display / buttons / knobs: sound loads keep the steps and
#                   the track's ARP / SCL / SLICER, the SAVE > PHRASES loader and its REPLACE? dialog, the
#                   one-step undo of both (SAVE held), REC on TRACKS / SEQ / ARP, STEP and ARP while recording,
#                   FX HOLD (MENU > VOICES): off, a sound load sets the sends and INSERT as before; on, every load path (PRESETS, MIDI
#                   program change, engine change, user preset) keeps them; SOUND / CLEAN / project INIT and a project load do not; the
#                   global FX untouched; undo; a settings round trip,
#                   MIDI IN ROUT, saves refused while playing and the OVERWRITE? dialog, MUTE on TRACKS KNOB 1, FM6's operator
#                   pages (each byte alone, SLOT OWN, the sound, undo, the macros, projects / user presets),
#                   the DRUM grid (keys, knobs, LEDs, pages, live recording into it, BEAT from PATTERNS).
# DRUM STRESS (tests/drumfuzz_test.c, #211): ASan / UBSan, a synth sequence playing while the DRUM grid is edited at random
#                   (keys, locks, lanes, HIT / ACC, EDIT, undo, SEQ TOOLS, engines, kits, the MENU), the audio ISR emulated
#                   anywhere in the main loop; also with a SONG playing; a step over 5 s is a hang.
# Audio / persistence / editor: the real C paths against simulated DMA and NOR flash: bounded overload
#                   fades, shared-voice limits, deferred settings and retries, failed-save rollback, the autosave
#                   (1.2: when it writes, the power-on restore, damaged copies, a write cut short, wear over a session),
#                   malformed transfers, transport-stop timeouts, MIDI and UART recovery; the MENU settings over the
#                   editor (MENU_DESC / MENU_SET: every item, clamping, unknown ids, saving, USB SERIAL applied later).
# FM6 BANK (tests/fm6bank_test.c, 1.4.1): the voice bank (src/fm6_vbank.c) and its editor commands against simulated NOR:
#                   written whole and read back, its sectors (0xE7000, staged in the autosave's older copy, nothing else
#                   touched), a power cut after any programmed byte or in any erase (the old bank or the new one, the
#                   autosave kept), a wrong CRC, an autosave between requests, SLOT B1..B32 loading the exact records,
#                   empty bank slots skipped / refused, projects and user presets keeping a bank voice, stored SLOT values
#                   (projects, user presets, motion) loading as before.
# CHORD (tests/chord_test.c): the chord keys (src/chord.c): diatonic triads / sevenths of several scales and roots,
#                   the fixed shapes and voicings (at most 4 notes), names, MONO plays the root, a release ends
#                   exactly what its key / MIDI note started, recording, the ARP, MIDI IN, kits ignore CHRD.
# FUN10 (tests/fun10_test.c, 1.2): LFO 2 (SYNC one cycle per note value at several tempi and on the external clock, OFF bit
#                   for bit as before; TRIG NOTE / FREE; POL UNI 0..+1, the matrix's AMP), NUDGE (live recording keeps a
#                   note's time, QUANTIZE ON plays as before, OFF plays the nudges late and early once a pass, never twice
#                   with live notes, RATCH on its start, nudges 0 play the same either way, a tempo change of the external
#                   clock, SEQ > AUTOMATION's QUANTIZE and NUDGE rows, STEP steps held + PRESETS), 128 motion records, FUN10 round trip, FUN9 /
#                   FUN8 / FUN7 as 1.1 / 1.0 wrote them (tests/old_pack.h) loading exactly, user presets of 99 / 103.
# RATCH (tests/ratchet_test.c): a step's ratchet (x1..x4): its parts in the sequencer (equal, gated, chords and drum
#                   hits whole, one chance roll, swing, no slide or tie out, STOP), FUN8 round trip and older projects x1,
#                   user preset patterns, SEQ > AUTOMATION's RATCH rows and the roll / grid drawing.
# STEP 1.5 (tests/step15_test.c, #178 #173): SEQ > STEP's KNOB 3 LEN on a note (clean TIEs written after it and taken
#                   back, stopping at the next note and at LEN, chords, undo, TIME elsewhere, the DRUM grid untouched,
#                   the longer note played), a step's VEL as it plays (96 by default, ACC 127, drum hits) and
#                   SEQ > AUTOMATION's VEL rows (SHOW / HIDE, KNOB 4, EDIT, undo).
# DETAIL 1.5.1 (tests/detail_test.c, #199): SEQ > DETAIL (STEP -> DETAIL -> AUTOMATION), KNOB 1..4 the cursor step's
#                   CHANCE RATCH NUDGE VEL walked with PRESETS (NO NOTE on a REST / empty step), the same values as
#                   AUTOMATION's rows (gone at their default; EDIT there deletes one), EDIT resets, undo per gesture, the
#                   DRUM grid's keys picking steps (several held), a chord's ratchet, playback, STEP's marks.
# SEQ 1.5.5 (tests/seq155_test.c, #209 #186 #85): CONDITIONS (1:2 .. 4:4 by each track's loops from PLAY / a SONG
#                   section, FILL / !FILL with GLO + E4, a NUDGE early step 1, no lock on a failed one, the chance draws
#                   as before), DETAIL's KNOB 1 past 100 % and AUTOMATION's COND rows, FUN10 round trip and what 1.5 /
#                   1.5.1 do with them; SEQ TOOLS' EUCLID (hits / steps / rotation, DRUM lane or notes, OCT+ / OCT-,
#                   undo) and DOUBLE / HALF (ties, locks, the steps' own fields, 64 steps, undo).
# 1.5.5 (tests/ui155_test.c): PAN reads -100..+100 % (#199, the stored range as before); SEQ > AUTOMATION: KNOB 3 keeps
#                   its row in place (a step's records in the order they were added, #199); MENU > CONTROL > SELECT (#188):
#                   BPM by default, PAGE steps the family's pages (round, HOME MIXER CLOCK one round), GLO + SELECT the tempo;
#                   OCT- (1.5.6: #205's "OCT- held + a knob = default" taken out): the octave on the press as in 1.5.1, a
#                   knob with OCT- held an ordinary turn, the menu's OCT- back; SAVE > PROJECT ERASE (#199): KNOB 2, the dialog,
#                   refused playing / empty (tests/persistence_test.c: the flash side, a cut before the record keeps it);
#                   CLEAN INIT (#190): every engine's basic sound (no sends, INSERT, LFO / ENV / MOD amounts; neutral ENV; FM6
#                   the init voice), heard and ending; TOOLS' KNOB 2 on, EDIT + F5; one undo; INIT as before. OCT- next to
#                   EUCLID (OCT- cancels, OCT+ applies, no octave) and FILL (GLO + E4).
# 1.5.6 (tests/v156_test.c): HUMANIZE (#213): the matrix's VEL (a note-on's velocity, RAND per note, clamped 1..127) and
#                   TIME (a sequencer step late, 0..30 ms, at most 7/16 of the step, with NUDGE, a ratchet on time),
#                   appended after DEPTH; nothing changes without such a slot (RAND draws, mod_begin, timing); saved.
#                   KITS (#216): DRUM KIT DEEP TIGHT PERC (9..11) from the five kits' pieces and their three presets after
#                   DRUM KIT, the lanes' names, the 8 lanes play, saved, an older range reads them as 77.
#                   LEDS (#212): INV LO / INV HI after INV (INV's lit LEDs at the mid level), saved as INV + a level byte.
#                   SELECT (#84): MODW (the track's mod wheel, CC1 out) and VEL (the keys' velocity) after BPM / PAGE.
# 1.6 (tests/v16_test.c): SEQ's piano roll follows the play position (#242, Discussion #238): a melodic pattern over 16 steps,
#                   the transport playing: the roll's bank is the playhead's (STEP and DETAIL), the cursor and the pattern
#                   untouched; not for 16 steps, stopped, DRUM's grid, AUTOMATION, HOME; held from a knob's move of the
#                   cursor, an edit of its step or step entry, free again after 3 s or one pass round; reset by stop / track.
# MOD (tests/mod_test.c): the modulation matrix: slots that do nothing are bit-identical, every source on each
#                   kind of destination, clamping, MIDI CC1 / CC11 / aftertouch routing, the cost of 4 active
#                   slots (at most +5 %), demos in build/mod_demo/.
# SOUND (tests/sound14_test.c; #132 #104 #148): the matrix's S&H / SLEW sources (a value per LFO cycle, the glide) and
#                   DEPTH destination (the LFO's gain; none = untouched), ANALOG WAVE SYNC / SUB (pitch, no DC, bounded,
#                   the band-limited restart), SPREAD (SPRD 0 / MONO / LEGATO bit for bit, a note hard left / right, the
#                   mono sum, UNISON, the SLICER's gate and the mute key on both sides, mono sends), the cost; demos in
#                   build/sound14_demo/.
# INSERT (tests/insert_test.c, 1.5; #78 #177): the per-track INSERT (src/fx.c track_insert): TYPE OFF and MIX 0 bit for
#                   bit (POLY, SPREAD), each TYPE at its corners on 3 and 8 voices (no DC, bounded, MIX 50 % halfway
#                   to the wet), the MIX glide and TYPE changes, MIX / TYPE as automation and locks, FUN10 and user
#                   presets of 109 and 104, the pages' labels, the cost per type; demos in build/insert_demo/.
# SOUND 1.5 (tests/sound15_test.c; #104 #175): ANALOG's filter TYPE (LP bit for bit, every other engine ignores it, BP / HP
#                   responses around the cutoff, RES, bounded at the corners), ENV SYNC (OFF bit for bit, the note values
#                   on the knob, attack / decay / release times at 60 / 120 / 173 BPM and on the external clock), the cost;
#                   demos in build/sound15_demo/ (analog_bp, analog_hp, env_sync).
# PHASE FOLD 1.5.5 (tests/fold155_test.c; #104): E7 FOLD (0..127, default 0, the presets at 0), FOLD 0 bit for bit with
#                   1.5.1's loop, bounded and without DC at FOLD 1..127 (every WAVE, MIX / RING, SUB, notes 12..127),
#                   brighter with FOLD, the gain falling with the pitch above A4, the cost; build/fold155_demo/phase_fold.wav.
# PERFORM (tests/perform_test.c): the FX hold layer (src/perform.c): 1/16 starts, stereo buffer effects, the
#                   too-long REPEAT, the SLICER interplay, silent layer keys, idle bit-identical, cost; build/perform_demo/;
#                   OCT UP / DN (the harmonizer): pitch, stereo, clicks, the shimmer bounded, cost; FLANGER / PHASER (1.2): no DC,
#                   bounded, the sweep in time with the tempo, clean release, stacked with REPEAT / LPF / HPF, cost; the key
#                   map (any effect or none on any key, a key remapped while held, FX LATCH); build/fx_demo/; 1.5.1: the
#                   delay's dotted TIME (lengths, the 1/4D clamp, the knob order), REV / ECHO THROW (the sends as at 127,
#                   the glide, ECHO's capped feedback bounded, the tails ringing out, assignable); build/throw_demo/.
# REVERB (tests/reverb_test.c): REVERB TYPE (src/fx.c): ROOM bit for bit as before, SPRING's decay against SIZE,
#                   its chirp (group delay rising with frequency), stability at the corners, level, a model change
#                   without a click, its cost against ROOM (+30 % at most); demos in build/fx_demo/. 1.2's HALL: SPRING
#                   bit for bit as before, decay against SIZE and DAMP, stability, exact silence after the tail (27
#                   settings and levels), no offset, level, stereo, echo density / ringing / flutter against ROOM, SPRING
#                   and COMB8 (8 combs + 4 allpasses, host only, for comparison), model changes both ways, cost; the
#                   listening set in build/reverb_demo/ (drums, pluck, piano: dry and each model at SIZE 40 / 90 / 127).
# SLICE (tests/slice_test.c): slice tables, AUTO onsets of a user-slot loop, reverse, keys, modes, the MAN slices
#                   (SLICES page) and their store in the slot (src/slice_store.c); the presets and the loop;
#                   demos in build/slice_demo/.
# SAMPLE (tests/sample_test.c, 1.5): PIANO is the 5-zone piano of 1.0 .. 1.1.5 again: zones, splits, pitch over the
#                   keyboard (YIN), 0.75 s at a root, the aliases, GRAIN SRC 0; SLICE's PIANO its middle C zone (shared);
#                   1.2's PIANO HD user-slot file (gen_samples.py --user-slot: build/piano_hd/PIANO_HD), as old users may
#                   have installed it: valid in USR1 and bit for bit as SET 0.
# INPUT (tests/input_test.c): the key / button debounce of hal/fm1_input.h against the TIMER5 scan and bouncing
#                   contacts: a press within 2 scans (<= 2.3 ms), one note per bouncy press, no early or hanging
#                   release, stray samples ignored, fast repeats, the encoders' detents; the LED scan: lit LEDs every
#                   frame, dim ones a short pulse (the second line write) every frame, each only on its own column;
#                   the breath (#119): dark .. ~60 % of lit (DIM LO ~30 %), smooth, no dark run over ~10 ms near its peak;
#                   the power-on sweep (1.1, hal/fm1_led_anim.h): its length, every LED every frame at its level, the
#                   head left to right with its tail, the buttons, the end on the idle glow, the tick as before after;
#                   the idle animation (1.2, MENU > ANIM IDLE): endless, every LED every frame, the light there and back,
#                   the buttons breathing, a key down ends it before its next frame.
# CLICK (tests/click_test.c, 1.1): the metronome (src/click.c) and the count-in (src/seq.c) through audio.c: each beat at
#                   the first sample of its block, with a 1/16 track's steps 1 5 9 13 (internal and external clock, DIV 1/8
#                   and SWING too), the accent, the frequencies, length, LEVEL, MASTER, OFF / REC / ON, USB audio bit for bit
#                   without it; COUNT-IN 1 / 2 BARS (step 1 exactly N bars after PLAY, STOP cancels, PLAY again nothing,
#                   never with an external clock), notes in its last eighth onto step 1; build/click_demo/.
# USB audio (tests/uac_test.c): the UAC1 descriptors as a host parses them (with and without CDC; 44100 and
#                   48000 Hz), the ring and packetiser: 44.1 frames per packet, every frame in order (bit for bit),
#                   underrun / overrun, restart; SET_CUR; the 44.1 -> 48 kHz resampler (every frame against a
#                   double-precision one, response, SNR, cost); the stream at 48 kHz (47..49 frames per packet,
#                   fast and slow I2S clocks, switching rates mid-stream).
# web (web/test_web.mjs): the editor protocol against its mock device, whose tables must equal the
#                   firmware's (tests/descdump.c -> build/host/desc.json; the MENU settings: tests/editor_test.c -> build/host/menu.json),
#                   the package builder, the updater.
# Browser emulator (web/emu, when emcc is there): the firmware in WebAssembly (build/emu) boots, plays keys and MIDI,
#                   draws, lights its LEDs, keeps a save across instances, plays a song bit for bit as the same file
#                   built with cc (web/emu/native_check.c); the cost of 1 s of a heavy song against real time.
# PHYS (tests/phys_test.c): stability over the whole parameter and pitch range, the worst-case cost against
#                   the heaviest factory preset, demos in build/phys_demo/; tests/phys_ref.cpp compares the
#                   fixed-point models with DaisySP's float originals when DaisySP is there (DAISYSP=path).
# DRUM (tests/drum_test.c): the drum voices (src/drum_voice.c): pitch, decay, centroid and level against
#                   Felucca's targets, the controls' directions, no clipping, DC, retriggers, the hat choke, the
#                   kick on a small speaker; the DRUM engine (src/eng_drum.c): its key map, the 8 lanes together,
#                   one hit per lane, the choke between lanes; the cost per voice; demos in build/drum_demo/.
# NOISE (tests/noise_test.c): the engine (engines/src/eng_noise.c): COLR's slope (white, pink, brown), the filter and the
#                   register clock following the key, META periodic at the key, no DC, no clipping at the
#                   corners, a note from silence the same twice, the cost per voice; demos in build/noise_demo/.
# DIGITAL -> FM6 (tests/fm4_test.c, built with FELUCCA_FM4=1): the retired four-operator engine against its conversion
#                   (src/fm4_convert.c): routes and carriers per algorithm, the presets' PTCH, and the sound (pitch,
#                   centroid, RMS envelope) of its presets and algorithms; demos in build/fm4_demo/. tests/digital_test.c
#                   (FELUCCA_FM4=1 too): DIGITAL's operator envelopes. Default builds have no DIGITAL (engine 1 reserved).
#                   tests/fm4_div0_test.c (UBSan, #61): the conversion without a divide that can be 0 gives the values the
#                   guarded form gave (every INDEX x FLT ENV, random sounds); a 0.9 project with DIGITAL ORGAN converts.
# FM6 (tests/fm6_test.c): the 6-operator FM engine (src/eng_fm6.c, src/fm6_core.c): the 32 algorithms' carriers, the
#                   operator envelopes (stages, rates, the voice ending), bit-stable notes, a click-free retrigger, no DC /
#                   clipping over the factory patches, the macros' directions, PTCH, pack / unpack and the SysEx
#                   layouts, the 6-voice cap, the cost per voice; demos in build/fm6_demo/.
# ROBUST (tests/robust_test.c): damaged or crafted stored data and editor requests: a SLICE scan bounded by the slot,
#                   a slot that fails its check leaves no zone, engine numbers past the last refused, a retained older
#                   RAM project bounded, user preset patterns inside their fields, malformed requests never stop PLAY.
# Sanitizers (ASan + UBSan, when the compiler has them; SANITIZE=0 skips): the stored-data and protocol tests again
#                   (loader, M-UPGRADE entry, editor, projects, backup, ROBUST) and three short fuzz runs with fixed
#                   seeds: tests/fuzz_ed.c (editor SysEx and raw USB-MIDI packets), tests/fuzz_proj.c (mutated project
#                   stores -> import -> restore -> render), tests/fuzz_smp.c (user sample slot headers -> scan -> SAMPLE /
#                   GRAIN / SLICE). Longer runs: build/host/asan/fuzz_ed 300000 7 (iterations, seed), the same for the others.
#                   UBSan leaves out the DSP's intended wraps (signed overflow, shifts) and the XIP rebase of a user zone's
#                   offset (bounds, object-size, pointer-overflow: correct on the device's flat flash, not in C's model).
# Change baseline entries only for reviewed, intentional differences in sound or cost;
# retain every unaffected golden / CPU / target entry. VERBOSE=1: every render.
set -e
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
CC="${CC:-cc} -O1 -Wall -Wno-unused-function"
fail=0
run() { label=$1; echo "== $label"; shift; "$@" || { rc=$?; echo "FAILED (exit $rc): $label"; fail=1; }; }

run "release artifacts: offline synthetic fixture checks" python3 -B tests/release_artifacts_test.py
run "site: exact release source, notices and navigation" python3 -B tests/site_release_test.py

$CC -o "$OUT/storage_test" tests/storage_test.c
run "flash storage (A/B, torn writes)" "$OUT/storage_test"

$CC -o "$OUT/upreset_test" tests/upreset_test.c
run "user presets (UP_PUT parser, bank round trip, versions, PHYS DRUM and SAMPLE PERC -> DRUM, grid records, DIGITAL kept)" "$OUT/upreset_test"

$CC -o "$OUT/input_test" tests/input_test.c
run "keys and buttons: fast press, long release, bouncy contacts (one note each), glitches, encoders" "$OUT/input_test"

$CC -o "$OUT/midi_uart_test" tests/midi_uart_test.c
run "TRS MIDI parser" "$OUT/midi_uart_test"

$CC -O2 -o "$OUT/timer5_midi_test" tests/timer5_midi_test.c
run "TRS timestamping during audio with saturated CC input" "$OUT/timer5_midi_test"
$CC -O2 -DFELUCCA_UART=0 -o "$OUT/timer5_midi_nouart_test" tests/timer5_midi_test.c
run "TIMER5 with TRS input disabled" "$OUT/timer5_midi_nouart_test"

HALF=$(sed -n 's/^#define HALF_FRAMES \([0-9]*\).*/\1/p' firmware/src/core.h)
$CC -O2 -DT_CDC=1 -DHALF_FRAMES=$HALF -o "$OUT/uac_test" tests/uac_test.c -lm
run "USB audio input: descriptors (with CDC), ring and packets, 44.1 -> 48 kHz resampler, 48 kHz stream" "$OUT/uac_test"
$CC -O2 -DT_CDC=0 -DHALF_FRAMES=$HALF -o "$OUT/uac_test_nocdc" tests/uac_test.c -lm
run "USB audio input: descriptors (without CDC), ring and packets, resampler, 48 kHz stream" "$OUT/uac_test_nocdc"
# USB descriptor layouts (#67): CDC UAC LAYOUT CDC-presented 48K; layout 0 and the console left out = 1.0's bytes
# (+ the 48 kHz rate with 48K = 1, the default; 48K = 0: byte for byte; + the AC collection's audio streaming
# interface first, FELUCCA_UAC_AS_FIRST = 1, the default since 1.6)
for v in 1.1.0.1.1 1.1.1.1.1 1.1.2.1.1 1.1.3.1.1 1.1.0.0.1 1.1.2.0.1 0.1.0.1.1 1.1.0.1.0 1.1.0.0.0 1.1.2.1.0 0.1.0.1.0 \
         1.0.0.1.1 1.0.1.1.1 1.0.0.0.1 0.0.0.1.1; do
    IFS=. read -r t_cdc t_uac t_lay t_on t_48 <<EOF
$v
EOF
    $CC -DT_CDC="$t_cdc" -DT_UAC="$t_uac" -DT_LAYOUT="$t_lay" -DT_ON="$t_on" -DT_48K="$t_48" -o "$OUT/usb_desc_test" \
        tests/usb_desc_test.c
    run "USB descriptors: CDC $t_cdc (presented $t_on), UAC $t_uac (48 kHz $t_48), layout $t_lay" "$OUT/usb_desc_test"
done
# FELUCCA_UAC_AS_FIRST=0 (#67): the AC collection of 1.0 .. 1.5 (MIDI first), 1.0's bytes; the parser model of the
# kernel AppleUSBAudio (macOS <= 15) stops before the audio streaming interface there
for v in 1.1.0.1.1 1.1.0.0.1 1.1.0.1.0 1.1.0.0.0 0.1.0.1.1; do
    IFS=. read -r t_cdc t_uac t_lay t_on t_48 <<EOF
$v
EOF
    $CC -DT_CDC="$t_cdc" -DT_UAC="$t_uac" -DT_LAYOUT="$t_lay" -DT_ON="$t_on" -DT_48K="$t_48" -DT_ASF=0 \
        -o "$OUT/usb_desc_test" tests/usb_desc_test.c
    run "USB descriptors (AC collection MIDI first): CDC $t_cdc (presented $t_on), UAC $t_uac (48 kHz $t_48)" \
        "$OUT/usb_desc_test"
done

[ -f build/caravel.fwsc ] || { echo "run ./build.sh first"; exit 1; }

$CC -DOWN_PKG=1 -o "$OUT/ota_test" tests/ota_test.c
run "M-UPGRADE entry (own loader)" "$OUT/ota_test" build/caravel.fwsc

head -c 200000 build/felucca.bin > "$OUT/old_app.bin"
python3 tools/fm1pkg_make.py "$OUT/old_app.bin" build/loader/ota.bin "$OUT/old.fwsc" >/dev/null
$CC -o "$OUT/ldr_test" tests/ldr_test.c
run "update loader: other app -> this build" "$OUT/ldr_test" "$OUT/old.fwsc" build/caravel.fwsc

if [ -f build/gen/felucca_tables.h ]; then
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/hostsim" tests/hostsim.c -lm
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/scale_test" tests/scale_test.c -lm
    run "scales: white-key mapping and note lifecycle" "$OUT/scale_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/chord_test" tests/chord_test.c -lm
    run "chord keys: diatonic and fixed chords, voicings, MONO root, releases, recording, ARP, MIDI IN, kits" "$OUT/chord_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/speaker_test" tests/speaker_test.c -lm
    run "SPEAKER EQ: FLAT / LOWCUT / BASS+ responses, BASS+ harmonics of the bass, the sub cut, no offset after, headroom on a full-scale kick (#180)" "$OUT/speaker_test"
    run "DSP render (ANALOG preset 0)" "$OUT/hostsim" 0 0 1 "$OUT/render.wav"
    mkdir -p build/tracks_demo
    run "TRACKS: 4-track pattern, live recording (lengths, swing), voice budget, engine switch, cost" env TRACKS=build/tracks_demo "$OUT/hostsim" 0 0 1 "$OUT/tracks.wav"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/project_test" tests/project_test.c -lm
    run "project formats (FUN1..FUN5 -> FUN6, the grid and song chain; DIGITAL tracks -> FM6, SAMPLE PERC -> DRUM)" "$OUT/project_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fx_motion_test" tests/fx_motion_test.c -lm
    run "FX-special automation: fine-timed edges, latch, lifecycle, capacity and saved parts" "$OUT/fx_motion_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fx_edit_test" tests/fx_edit_test.c -lm
    run "FX edge editing: independent step/phase, live chase, undo, loop and ordering" "$OUT/fx_edit_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/duplicate_test" tests/duplicate_test.c -lm
    run "DUP x2: step/motion copies, shared capacity, undo, live boundary and part launch" "$OUT/duplicate_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/motion_test" tests/motion_test.c -lm
    run "motion, whole-step chance, FUN7 migration, song restore, ARP repeat and the 1.2 ARP modes" "$OUT/motion_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/ratchet_test" tests/ratchet_test.c -lm
    run "RATCH: x1..x4 in a step (notes, chords, drum hits), gates, chance, swing, projects, user presets, AUTOMATION RATCH rows" "$OUT/ratchet_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/step15_test" tests/step15_test.c -lm
    run "1.5 STEP: a note's LEN (TIEs on KNOB 3), a step's VEL (playback, AUTOMATION's VEL rows)" "$OUT/step15_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/detail_test" tests/detail_test.c -lm
    run "1.5.1 SEQ > DETAIL: a step's CHANCE RATCH NUDGE VEL on KNOB 1..4 (walk, entries, undo, DRUM, chords, playback, marks)" "$OUT/detail_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/bytebeat_test" tests/bytebeat_test.c -lm
    run "LOFI BYTE/FLOAT: legacy sound equivalence, formula bounds and storage" "$OUT/bytebeat_test"
    $CC -O1 -g -w -fsanitize=undefined -fno-sanitize=shift -fno-sanitize-recover=all -Ibuild/gen -Ifirmware/src -o "$OUT/hades_dsp_test" tests/hades_dsp_test.c -lm
    run "HADES isolated DSP: strict integer overflow, 12000 settings, aliased output and live control continuity" "$OUT/hades_dsp_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/lfo2_test" tests/lfo2_test.c -lm
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/lfo2_editor_test" tests/lfo2_editor_test.c -lm
    run "LFO2 editor: descriptors, values, bounds and Learn2" "$OUT/lfo2_editor_test"
    run "Independent LFO2: modulation, UI, Learn, sound and project storage" "$OUT/lfo2_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/hades_test" tests/hades_test.c -lm
    run "HADES: divider subs, filter, stability, controls, persistence and native timing" "$OUT/hades_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/drone_test" tests/drone_test.c -lm
    run "DRONE: Learn, storage, favorites, locks, DSP corners, tuning, retriggers and release" "$OUT/drone_test"
    $CC -O2 -w -DC1_HOST -pthread -Ibuild/gen -Ifirmware/src -o "$OUT/drone_c1_test" tests/drone_test.c -lm
    run "DRONE second core: four networks and mixed engines, live controls, every split bit-identical" "$OUT/drone_c1_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/drone_cells_test" tests/drone_cells_test.c -lm
    run "DRONE cells: ownership, all128 keys, local pitch/gain, snapshot inhibition, lifecycle, FULL and bounds" "$OUT/drone_cells_test"
    $CC -O2 -w -DC1_HOST -pthread -Ibuild/gen -Ifirmware/src -o "$OUT/drone_cells_c1_test" tests/drone_cells_test.c -lm
    run "DRONE cells second core: four cells tracks and mixed engines, all splits, samples and state" "$OUT/drone_cells_c1_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/sem_test" tests/sem_test.c -lm
    run "SEM engine: filter response, endpoint stability, Learn, storage and reservations" "$OUT/sem_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/jiant_sound_test" tests/jiant_sound_test.c -lm
    run "JIANT sound ports: legacy zero paths, isolated parameters, storage, locks, MIDI and combined DSP" "$OUT/jiant_sound_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/seq155_test" tests/seq155_test.c -lm
    run "1.5.5 SEQ: conditions and FILL, SONG, storage, editor; SEQ TOOLS EUCLID, DOUBLE, HALF" "$OUT/seq155_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/ui155_test" tests/ui155_test.c -lm
    run "1.5.5 fixes and UI: PAN +-100, AUTOMATION KNOB 3, SELECT PAGE, OCT- on the press (1.5.6), PROJECT ERASE, CLEAN INIT" "$OUT/ui155_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/v156_test" tests/v156_test.c -lm
    run "1.5.6: the matrix's VEL / TIME (#213), DRUM kits DEEP TIGHT PERC (#216), LEDS INV LO / HI (#212), SELECT MODW / VEL (#84)" "$OUT/v156_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/v16_test" tests/v16_test.c -lm
    run "1.6: SEQ's piano roll follows the play position (#242: STEP, DETAIL; held by the knobs, 3 s or one pass)" "$OUT/v16_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fun10_test" tests/fun10_test.c -lm
    run "1.2 (FUN10): LFO 2 SYNC / TRIG / POL, NUDGE (record, QUANTIZE, play, controls), 128 motion records, FUN10 / FUN9 / FUN8 / FUN7, user presets" "$OUT/fun10_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/midi_control_test" tests/midi_control_test.c -lm
    run "USB/TRS clock, bend, sustain, ownership and panic recovery" "$OUT/midi_control_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/midi_resync_test" tests/midi_resync_test.c -lm
    run "Custom USB/TRS bar and manual pulse resync, ARPs and panel" "$OUT/midi_resync_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/midi_drift_test" tests/midi_drift_test.c -lm
    run "External clock: five 20-minute phase runs, polymeter and ARPs" "$OUT/midi_drift_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/voice_reserve_test" tests/voice_reserve_test.c -lm
    run "Track voice reservations: isolated notes, ARPs, UNISON, DRUM-X, FX, panel and settings migration" "$OUT/voice_reserve_test"
    $CC -O1 -w -DTIMER5_SYNTH_TEST=1 -Ibuild/gen -Ifirmware/src -o "$OUT/timer5_cc_clock_test" tests/timer5_midi_test.c -lm
    run "TRS clock and all four sequencers under saturated learned CCs (64 mappings, ANALOG and DRUM-X)" "$OUT/timer5_cc_clock_test"
    $CC -O1 -w -DFELUCCA_FM4=1 -Ibuild/gen -Ifirmware/src -o "$OUT/digital_test" tests/digital_test.c -lm
    run "DIGITAL (retired, built here with FELUCCA_FM4=1): operator envelopes/levels" "$OUT/digital_test"
    $CC -O2 -w -DFELUCCA_FM4=1 -Ibuild/gen -Ifirmware/src -o "$OUT/fm4_test" tests/fm4_test.c -lm
    mkdir -p build/fm4_demo
    run "DIGITAL -> FM6: the conversion against DIGITAL (FELUCCA_FM4=1): pitch, centroid, RMS envelope; demos" \
        "$OUT/fm4_test" build/fm4_demo
    $CC -O2 -w -fsanitize=integer-divide-by-zero -fno-sanitize-recover=all -Ibuild/gen -Ifirmware/src \
        -o "$OUT/fm4_div0_test" tests/fm4_div0_test.c -lm
    run "DIGITAL -> FM6 without a divide by zero (#61): the same values as before, a 0.9 ORGAN project (UBSan)" \
        "$OUT/fm4_div0_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/theme_test" tests/theme_test.c -lm
    run "themes: contrast, text blending and font metrics" "$OUT/theme_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/text_ref_test" tests/text_ref_test.c -lm
    run "text: pens, kerning and pixels equal the reference renderer" "$OUT/text_ref_test"
    if python3 -c "import PIL" 2>/dev/null; then
        run "text spacing: glyph gaps against the font's own (16x supersampled), S M <= 0.25 px, L <= 0.5 px" \
            python3 tests/text_spacing_test.py build/gen/ui_fonts.h
    fi
    $CC -O1 -w -Ibuild/gen -o "$OUT/settings_test" tests/settings_test.c
    run "settings: PER1..PER4 migration, palette ids and preference preservation" "$OUT/settings_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/live_pattern_test" tests/live_pattern_test.c -lm
    run "live project patterns: boundary, motion/locks, replacement, stop, engine guard and LOAD UI" "$OUT/live_pattern_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/live_full_test" tests/live_full_test.c -lm
    run "FULL live load: RAM snapshot, sounds/FX, FM6, DRUM-X, fades, settings and TRS clock" "$OUT/live_full_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/step_play_test" tests/step_play_test.c -lm
    run "step recording during playback: cursor, chords, live mode, MIDI, automation and RAM save" "$OUT/step_play_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/drumx_test" tests/drumx_test.c -lm
    run "DRUM-X: fixed A/B MIDI Learn, 64 slots, project/preset, payload and audio" "$OUT/drumx_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/ui_test" tests/ui_test.c -lm
    run "UI: sounds keep steps, undo, recording, MIDI overflow, pending saves, panel recovery, drum grid, song chain, GREY gray, MONO neutral" "$OUT/ui_test"
    $CC -w -DFELUCCA_CPU2=0 -Ibuild/gen -Ifirmware/src -o "$OUT/ui_test_c1" tests/ui_test.c -lm
    run "UI, a FELUCCA_CPU2=0 build: MENU > SYSTEM without 2ND CORE, the rest as the default build" "$OUT/ui_test_c1"
    $CC -DPREEMPT -g -fwrapv -fsanitize=address,undefined -fno-sanitize=shift -fno-sanitize-recover=all -w \
        -Ibuild/gen -Ifirmware/src -o "$OUT/drumfuzz" tests/drumfuzz_test.c -lm
    run "DRUM grid stress (#211, ASan / UBSan): random grid editing, locks, undo, engines over a playing synth, the ISR anywhere" \
        "$OUT/drumfuzz" 1 6 1500
    run "DRUM grid stress (#211) with a SONG playing" "$OUT/drumfuzz" 101 3 1500 song
    FV=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c | head -n 1)   # (the splash, ABOUT: the real version)
    $CC -O1 -w "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/ui_render" tests/ui_render.c -lm
    mkdir -p build/ui_new/ppm build/ui_slot
    run "UI renders: layout lint (every screen and palette, every page, engine and column value), GREY gray, MONO neutral, alignment by ink (1 px fails), draw cost, LARGE one value size per knob (#251)" \
        "$OUT/ui_render" build/ui_new build/ui_slot
    if python3 -c "import PIL" 2>/dev/null; then python3 tests/ui_render.py build/ui_new build/ui_slot; fi
    $CC -O1 -w -DFELUCCA_FM4=1 -Ibuild/gen -Ifirmware/src -o "$OUT/ui_test_fm4" tests/ui_test.c -lm
    run "UI built with FELUCCA_FM4=1 (DIGITAL, kept in the tree): its OP pages, EDIT cycle, algorithm charts" "$OUT/ui_test_fm4"
    $CC -O1 -w -DFELUCCA_FM4=1 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/ui_render_fm4" tests/ui_render.c -lm
    mkdir -p build/ui_fm4/ppm build/ui_fm4_slot
    run "UI renders with FELUCCA_FM4=1: DIGITAL's screens (EDIT, OP ENV, the 8 algorithm charts), lint, GREY gray, MONO neutral" \
        "$OUT/ui_render_fm4" build/ui_fm4 build/ui_fm4_slot
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/audio_test" tests/audio_test.c -lm
    run "audio: overload protection, bounded fades and DMA diagnostics" "$OUT/audio_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/click_test" tests/click_test.c -lm
    run "metronome and count-in: beats sample for sample (internal and external clock), accent, level, MASTER, not in USB, count-in timing, notes onto step 1" "$OUT/click_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/persistence_test" tests/persistence_test.c -lm
    run "persistence: deferred settings, retry and failed-save rollback" "$OUT/persistence_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/backup_test" tests/backup_test.c -lm
    run "full backup: CRC before writes, stale runtime, USB reset / timeout, malformed objects, older projects" "$OUT/backup_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/fm6bank_test" tests/fm6bank_test.c -lm
    run "FM6 voice bank (1.4.1): write / read, power cuts, CRC, SLOT B1..B32, stored SLOT values as before" "$OUT/fm6bank_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/editor_test" tests/editor_test.c -lm
    run "editor: real C protocol, malformed transfers, queue recovery, MENU settings (writes build/host/menu.json)" \
        env MENU_JSON="$OUT/menu.json" "$OUT/editor_test"
    $CC -w -DFELUCCA_CPU2=0 -Ibuild/gen -Ifirmware/src -o "$OUT/editor_test_c1" tests/editor_test.c -lm
    run "editor, a FELUCCA_CPU2=0 build: MENU settings without 2ND CORE (ids 0..31)" "$OUT/editor_test_c1"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/robust_test" tests/robust_test.c -lm
    run "robustness: crafted sample slots, engine numbers, retained old projects, preset patterns, malformed requests" \
        "$OUT/robust_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/mod_test" tests/mod_test.c -lm
    mkdir -p build/mod_demo
    run "modulation matrix: off = bit-identical, the math, MIDI CC1 / CC11 / aftertouch, cost, demos" "$OUT/mod_test" build/mod_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/sound14_test" tests/sound14_test.c -lm
    mkdir -p build/sound14_demo
    run "sound items: matrix S&H / SLEW / DEPTH, ANALOG SYNC / SUB, SPREAD (bit for bit at 0, L / R, mono sum), cost, demos" \
        "$OUT/sound14_test" build/sound14_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/insert_test" tests/insert_test.c -lm
    mkdir -p build/insert_demo
    run "INSERT (1.5): OFF / MIX 0 bit for bit, each TYPE (no DC, bounded, dry / wet), MIX glide, automation, stored, cost, demos" \
        "$OUT/insert_test" build/insert_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/sound15_test" tests/sound15_test.c -lm
    mkdir -p build/sound15_demo
    run "1.5 sound items: ANALOG filter TYPE LP / BP / HP (responses, LP and other engines bit for bit), ENV SYNC (times at several tempi), cost, demos" \
        "$OUT/sound15_test" build/sound15_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fold155_test" tests/fold155_test.c -lm
    mkdir -p build/fold155_demo
    run "PHASE FOLD (1.5.5): FOLD 0 bit for bit, bounded, no DC, brighter with FOLD, alias-aware gain, cost, demo" \
        "$OUT/fold155_test" build/fold155_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/slicer_test" tests/slicer_test.c -lm
    mkdir -p build/slicer_demo
    run "SLICER: no clicks, timing, sync with the sequencer, STUT, cost, demos" "$OUT/slicer_test" build/slicer_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/swing_test" tests/swing_test.c -lm
    run "SWING: track + global at most 100, sequencer and SLICER step lengths, the SWG display" "$OUT/swing_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/perform_test" tests/perform_test.c -lm
    mkdir -p build/perform_demo build/fx_demo build/throw_demo
    run "FX layer effects: on the 1/16, stereo, too-long REPEAT, SLICER, silent keys, idle bit-identical, clicks, OCT UP / DN, FLANGER, PHASER, the key map, dotted TIME, REV / ECHO THROW, cost, demos" "$OUT/perform_test" build/perform_demo build/fx_demo build/throw_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/reverb_test" tests/reverb_test.c -lm
    mkdir -p build/reverb_demo
    run "REVERB TYPE: ROOM / SPRING bit-identical, SPRING and HALL decay / stability / level, HALL silence / stereo / ringing, model changes, cost, demos" "$OUT/reverb_test" build/fx_demo build/reverb_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/regress" tests/regress.c -lm
    run "regression: golden renders, health, voices, CPU budget (2ND CORE OFF: one core)" "$OUT/regress" tests/golden.txt tests/cpu_baseline.txt
    $CC -O2 -w -DFELUCCA_CPU2=0 -Ibuild/gen -Ifirmware/src -o "$OUT/regress_cpu2_0" tests/regress.c -lm
    run "regression, a FELUCCA_CPU2=0 build (none of the second core): the same golden renders and CPU budget" \
        "$OUT/regress_cpu2_0" tests/golden.txt tests/cpu_baseline.txt
    # the second core (2ND CORE ON: fx.c mix_block, tests/c1_host.h): a thread renders the parts handed to it, bit for
    # bit
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -DC1_HOST -pthread -o "$OUT/regress_c1" tests/regress.c -lm
    run "second core: the golden renders with a thread taking every split of the parts in turn" \
        "$OUT/regress_c1" tests/golden.txt tests/cpu_baseline.txt
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -pthread -o "$OUT/dualcore_test" tests/dualcore_test.c -lm
    run "second core: 4 parts of every preset, mixes: bit for bit on one core and on two, every split" \
        "$OUT/dualcore_test"
    run "FIXED voice reservations: Custom synth/DRUM-X mixes identical on one and two cores, every split" \
        env FIXED_VOICES=1 SCENE=Custom "$OUT/dualcore_test"
    run "LFO2: independent random and smooth modulation on both cores" env LFO2_TEST=1 SCENE="Custom HADES" "$OUT/dualcore_test"
    if [ -n "$TSAN" ]; then                     # TSAN=1: the same under ThreadSanitizer (~6 min): shared state
        TSAN_CC="cc -O1 -g -w -Ibuild/gen -Ifirmware/src -fsanitize=thread -pthread"
        $TSAN_CC -o "$OUT/dualcore_tsan" tests/dualcore_test.c -lm
        run "second core under ThreadSanitizer: 4 parts of every preset, mixes" \
            env TSAN_OPTIONS=halt_on_error=1 "$OUT/dualcore_tsan"
        ${CC%% *} -O1 -g -w -Ibuild/gen -Ifirmware/src -fsanitize=thread -pthread -DC1_HOST -o "$OUT/drone_tsan" tests/drone_test.c -lm
        run "DRONE second core under ThreadSanitizer: four networks and mixed engines, every split" \
            env TSAN_OPTIONS=halt_on_error=1 "$OUT/drone_tsan"
        ${CC%% *} -O1 -g -w -Ibuild/gen -Ifirmware/src -fsanitize=thread -pthread -DC1_HOST -o "$OUT/drone_cells_tsan" tests/drone_cells_test.c -lm
        run "DRONE cells second core under ThreadSanitizer: ownership, live controls, every split" env TSAN_OPTIONS=halt_on_error=1 "$OUT/drone_cells_tsan"
    fi
    # Felucca-Engines (firmware/engines, MIT): its own test, then its standalone host built with Felucca's generated
    # tables against its golden renders (the same hashes: build/gen/felucca_tables.h has the engines' tables)
    run "Felucca-Engines: its own tables, host and golden renders" sh firmware/engines/tests/run.sh
    $CC -O2 -w '-DFE_TABLES_H="felucca_tables.h"' -Ibuild/gen -o "$OUT/fe_host" firmware/engines/tests/fe_host.c
    "$OUT/fe_host" > "$OUT/fe_renders.txt" || fail=1
    run "Felucca-Engines with Felucca's tables: the engines' golden renders" diff -u firmware/engines/tests/golden.txt "$OUT/fe_renders.txt"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/union_test" tests/union_test.c -lm
    run "engine memory (1.5): every switch between the engines with per-part state sounds as in 1.4 (720 sequences)" \
        "$OUT/union_test" tests/union_golden.txt
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/descdump" tests/descdump.c -lm
    echo "== parameter and engine tables as JSON (for the editor mock test)"
    "$OUT/descdump" > "$OUT/desc.json" || fail=1
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/phys_test" tests/phys_test.c -lm
    mkdir -p build/phys_demo
    run "PHYS: stability C-1..G9 over the parameter corners, worst-case cost against PHASE WIRE, demos" "$OUT/phys_test" build/phys_demo
    D=${DAISYSP:-vendor/DaisySP}/Source
    if [ -d "$D/PhysicalModeling" ] && command -v c++ >/dev/null 2>&1; then
        $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -c -o "$OUT/phys_fixed.o" tests/phys_fixed.c
        c++ -O2 -std=c++14 -w -I"$D" -I"$D/Utility" -o "$OUT/phys_ref" tests/phys_ref.cpp \
            "$D/PhysicalModeling/modalvoice.cpp" "$D/PhysicalModeling/resonator.cpp" "$D/PhysicalModeling/stringvoice.cpp" \
            "$D/PhysicalModeling/KarplusString.cpp" "$D/Filters/svf.cpp" "$D/Utility/dcblock.cpp" \
            "$D/Dynamics/crossfade.cpp" "$OUT/phys_fixed.o"
        run "PHYS: the fixed-point models against DaisySP's float originals (mode frequencies, decays, Svf)" "$OUT/phys_ref"
    else
        echo "== skip PHYS reference test (no DaisySP: set DAISYSP to a checkout)"
    fi
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/drum_test" tests/drum_test.c -lm
    mkdir -p build/drum_demo
    run "DRUM: voice targets, controls, no clipping, retrigger, hat choke, the kick on a small speaker, keys, 8 lanes, cost, demos" "$OUT/drum_test" build/drum_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/noise_test" tests/noise_test.c -lm
    mkdir -p build/noise_demo
    run "NOISE: colour slopes, key-tracked filter and clock, META period, DC, clipping, retrigger, cost, demos" "$OUT/noise_test" build/noise_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fm6_test" tests/fm6_test.c -lm
    mkdir -p build/fm6_demo
    run "FM6: algorithms, envelopes, retrigger, DC, clipping, macros, patch formats, voices, cost, demos" "$OUT/fm6_test" build/fm6_demo
    # SLICE is in the standard build (firmware/src/core.h): its test always runs (after #22 by andreahaku)
    if grep -q '^#define SLC_BREAK_BPM ' build/gen/felucca_samples.h; then
        mkdir -p build/slice_demo
        python3 tests/slice_loop.py build/slice_demo/loop
        python3 tools/fm1_sample_upload.py build LOOP build/slice_demo/loop build/slice_demo/loop.wav:60 >/dev/null
        $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/slice_test" tests/slice_test.c -lm
        run "SLICE: tables, onsets, reverse, keys, modes, MAN slices and their store, demos" "$OUT/slice_test" \
            build/slice_demo/loop build/slice_demo
    else
        echo "== SLICE: build/ was made with FELUCCA_SLICE=0 (no BREAK); run ./build.sh without it first"
        fail=1
    fi
    # SAMPLE's PIANO (1.5: as 1.1.5), SLICE's PIANO (its middle C zone), 1.2's PIANO HD in a user slot
    if grep -q '^    {"PIANO", 0, 5},' build/gen/felucca_samples.h; then
        mkdir -p build/sample_demo build/piano_hd
        python3 tools/gen_samples.py --user-slot PIANO "PIANO HD" build/piano_hd/PIANO_HD >/dev/null
        $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/sample_test" tests/sample_test.c -lm
        run "SAMPLE: PIANO (5 zones as 1.1.5: splits, pitch, length, GRAIN), SLICE's PIANO, a PIANO HD slot" "$OUT/sample_test" \
            build/piano_hd/PIANO_HD build/sample_demo
    else
        echo "== skip SAMPLE's PIANO (no CC0 library: assets/samples-cc0, tools/fetch_cc0.py)"
    fi
else
    echo "== skip hostsim (run ./build.sh once)"
fi

# ASan + UBSan: the stored-data and protocol paths, and short fuzz runs (fixed seeds: the same inputs every run)
SAN="-O1 -g -fsanitize=address,undefined -fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow"
SAN="$SAN -fno-sanitize-recover=undefined -w"
san_ok() {
    printf 'int main(void){return 0;}\n' > "$OUT/san_probe.c" &&
        ${CC%% *} $SAN -o "$OUT/san_probe" "$OUT/san_probe.c" 2>/dev/null && "$OUT/san_probe"
}
if [ "${SANITIZE:-1}" = 0 ]; then
    echo "== skip the sanitizer runs (SANITIZE=0)"
elif [ ! -f build/gen/felucca_tables.h ]; then
    echo "== skip the sanitizer runs (run ./build.sh once)"
elif ! san_ok; then
    echo "== skip the sanitizer runs (the compiler has no ASan / UBSan)"
else
    mkdir -p "$OUT/asan"
    A="$OUT/asan"
    SCC="${CC%% *} $SAN -Ibuild/gen -Ifirmware/src"
    export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
    $SCC -o "$A/ldr_test" tests/ldr_test.c
    run "ASan/UBSan: update loader (other app -> this build)" "$A/ldr_test" "$OUT/old.fwsc" build/caravel.fwsc
    $SCC -DOWN_PKG=1 -o "$A/ota_test" tests/ota_test.c
    run "ASan/UBSan: M-UPGRADE entry (own loader)" "$A/ota_test" build/caravel.fwsc
    for t in editor_test project_test backup_test robust_test fun10_test fm6bank_test duplicate_test fx_edit_test jiant_sound_test drumx_test live_full_test midi_resync_test midi_drift_test voice_reserve_test sem_test lfo2_test lfo2_editor_test drone_test drone_cells_test; do
        $SCC -o "$A/$t" tests/$t.c -lm
        run "ASan/UBSan: $t" "$A/$t"
    done
    $SCC -DTIMER5_SYNTH_TEST=1 -o "$A/timer5_cc_clock_test" tests/timer5_midi_test.c -lm
    run "ASan/UBSan: TRS clock under learned CC load during audio" "$A/timer5_cc_clock_test"
    $SCC -o "$A/fuzz_ed" tests/fuzz_ed.c -lm
    run "ASan/UBSan fuzz: editor SysEx and raw USB-MIDI packets (20000, seed 7)" "$A/fuzz_ed" 20000 7
    $SCC -o "$A/fuzz_proj" tests/fuzz_proj.c -lm
    run "ASan/UBSan fuzz: project stores -> import -> restore -> render (5000, seed 13)" "$A/fuzz_proj" 5000 13
    $SCC -o "$A/fuzz_smp" tests/fuzz_smp.c -lm
    run "ASan/UBSan fuzz: user sample slot headers -> scan -> SAMPLE / GRAIN / SLICE (1000, seed 17)" "$A/fuzz_smp" 1000 17
fi

run "regression: target cost of the render loops (pi32v2 disassembly)" python3 tests/target_budget.py \
    build/felucca.dis tests/target_budget.txt

run "installer CLI (fm1_install.py) against a simulated FM-1" python3 tests/install_test.py

if command -v node >/dev/null 2>&1; then
    run "web pages: editor protocol + samples, package builder, update protocol" node web/test_web.mjs
    run "web backup: capture, validation before writes, restore order" node web/test_backup.mjs
else
    echo "== skip web tests (no node)"
fi

if ! command -v emcc >/dev/null 2>&1; then
    echo "== skip the browser emulator (no emcc: Emscripten builds web/emu)"
elif ! command -v node >/dev/null 2>&1 || [ ! -f build/gen/felucca_tables.h ]; then
    echo "== skip the browser emulator (needs node and build/gen)"
else
    run "browser emulator: the firmware to WebAssembly (web/emu/build.sh -> build/emu)" sh web/emu/build.sh
    cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o "$OUT/emu_native" web/emu/native_check.c -lm
    run "browser emulator: the same song from the native build" "$OUT/emu_native" "$OUT/emu_native.f32"
    run "browser emulator: boot, keys, MIDI, screen, LEDs, PLAY, a save kept across instances, = native, cost" \
        node web/emu/emu_test.mjs build/emu/felucca.wasm "$OUT/emu_native.f32"
fi

[ $fail -eq 0 ] && echo "ALL HOST TESTS PASSED" || { echo "HOST TESTS FAILED"; exit 1; }




