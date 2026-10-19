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
