<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# External MIDI clock: resync, quantized start, EXT START

These functions work with USB or TRS clock selected as the clock source. Timing tests are host simulations; hardware timing and listening
acceptance on a real FM-1 is pending.

## Resync and quantized start

- **Hold GLO and press PLAY:** restart all four sequencers and ARPs on the next counted **4/4 bar**. While running this is a resync; while
  stopped it arms **START: NEXT BAR** and the sequencers stay stopped until the next downbeat. Press again to cancel. The screen reports RESYNC:
  NEXT BAR, then RESYNC DONE.
- **Hold GLO and tap F4** (the white key normally used for TAP): restart at the next incoming clock pulse, also when joining clock-only input
  without a previous Start. Use it for a manual downbeat when joining a running master. Repeating F4 cancels; requesting the other mode replaces
  a pending request.
- Releasing GLO leaves the request armed; the GLO map's SYNC cell shows it. Stop, clock loss or a source change cancels it, and LIVE REC and
  SONG refuse it. Held ARP chords, patterns, automation, sounds and live FX latches remain; notes release at the reset and playback phases start
  again together. A manual resync preserves the master's bar count.
- With internal clock, GLO + PLAY and F4 keep their restart and tap-tempo functions.

A bar needs an observed MIDI Start: its first following pulse is downbeat zero and each 96 pulses begin the next bar. Local PLAY cannot establish
that reference, so without one the screen says WAIT MIDI START (the manual pulse mode stays available). MIDI Continue has no song position, so
it needs a new Start before an automatic bar resync. Detected clock loss, MIDI queue overflow and source changes invalidate the reference; missing
pulses cannot be reconstructed from MIDI Clock alone.

## EXT START

**MENU > SYSTEM > EXT START** chooses **PULSE** (default) or **BAR** for plain PLAY from STOP with external clock. PLAY again cancels the wait;
PLAY while running stops. BAR needs a previously received MIDI Start; without one a message asks for that reference. Internal clock, SONG start
and live-record start keep their behaviour.

## Continuous phase and TRS clock under load

Sequencers, ARPs and the metronome keep fractional timing continuously: the clock counts fixed musical units, tempo estimates cannot move the
accumulated phase, per-track conversion and precise phase scaling avoid rounding drift at straight, triplet and slow divisions, and external ARPs
keep step overshoot. There is no periodic loop reset, 2 %-threshold correction, note skipping or pattern rewrite; each track keeps its own LEN / DIV.
Felucca's 1/16 sequencer at 120 BPM accumulated about 109 ms of phase error in a simulated 20-minute run. The regression checks five 20-minute
USB / TRS runs at 40, 97, 120 and 240 BPM, including jitter, interpolation, polymetric lengths and held ARP chords, against independent incoming
pulse counts and allows 2 ms.

TRS MIDI input is timestamped during audio rendering, so heavy controller traffic no longer waits for a render to finish before Clock, Start,
Continue and Stop are timestamped. MIDI Learn keeps its track and parameter targets.

## TRS clock with a controller on the line (0.2.1)

Symptom: with MIDI IN on the TRS jack and the clock from a sequencer (Oxi E16) the BPM display jumped and the sequence drifted as soon as a
CC was sent on the same line. The TIMER5 poll (2 kHz, also during audio renders) was already in place and stays. The host model
`tests/midi_clock_cc_test.c` sends a steady 24 ppqn stream with CCs byte by byte (31250 baud, 320 us per byte, CC every 5..12 ms, running
status, a Clock slipping into a CC, bursts of eight CCs) through the real poll, parser, queue, `events_block` and sequencer.

What it showed (before the change, 20 s each):

| input | measured tempo | displayed | sequencer position, peak to peak |
| --- | --- | --- | --- |
| 120 BPM, no CC | 120.00 | 120 | 2.7 ms |
| 120 BPM, bursts of CCs | 119.05..120.97 | 119..121 | 4.2 ms |
| 140 BPM, bursts of CCs | 138.89..141.51 | 139..142 | 3.9 ms |
| 174 BPM, no CC | 172.42..174.42 | 172..174 | 2.8 ms |
| 174 BPM, bursts of CCs | 172.42..176.47 | 172..176 | 4.8 ms |

Causes found by reading and confirmed by the model: Clock stamps have 1 ms (`fm1_ms`) and are taken at the poll, a CC byte ahead of a Clock on
the wire delays it by up to 3 byte times; the tempo was re-measured from six pulses (about 125 ms at 120 BPM, 86 ms at 174), so every 1 ms
step was a ±0.8..1.2 % tempo step and the displayed BPM, `beat_samples` (gate lengths, LFO and FX sync) and the track phases moved with it; and
the clock interpolated inside a pulse with the last single pulse gap (20 or 21 ms for 20.83), so the position ran ahead and stalled.

Now: the tempo is measured over the last four quarter-beats (24 pulses). A quarter-beat more than a quarter off the window's mean (a tempo
step, a gap) or outside 62..375 ms starts the window again. The display (`G_BPM`) changes when the measured tempo is a whole BPM off it (and on
the first reading of a window). The interpolation uses the windowed pulse period. Pulse counting, `MIDI_CLOCK_GRID` phase, resync and the USB
clock are unchanged; USB clock uses the same estimator. After the change (same model): tempo within ±0.6 BPM of the sender, displayed BPM
constant at every tempo and load, position jitter at most 2.8 ms peak to peak (no CC: 1.8 ms), no drift.

Not established: the model's jitter is small (BPM ±1..4 before the change). A swing like 109..125 BPM (with the old estimator; 117..122 with this one) is
what a byte lost from the UART's DMA count produces in the model: every later Clock is delivered with the stamp of the next byte on the
line, random while a CC is sent, and the sequence stays about a pulse late (run `midi_clock_cc_test 120 12000 0 0 0 20 6000`; the 7th
argument is the ms at which the count loses a byte). This change does not remove that. Whether the device does that is open: the open Felucca PR #264 (hugelton/Felucca) argues that
`HRXCNT` is only a fresh batch when a receive/overtime event is pending. `uart_noevent` in the CDC status counts exactly such takes; if it
grows while the BPM jumps, #264 is the next fix. `clk_gap_min_ms` / `clk_gap_max_ms` / `clk_gap_odd` show the spread the firmware really sees
(a steady 120 BPM should give 20..22 ms and an odd count of 0 or close to it; hundreds of odd gaps mean the wire or the poll, not the estimator).
