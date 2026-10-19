<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# MORPH and PHASE feedback

Two sound parameters adapted from **JIANT** by juanjiant-bit (GPL-3.0-only), on top of Felucca's engines. Hardware acceptance on a real FM-1 is pending.

**MORPH** blends the oscillator waveform continuously: ANALOG SAW / SQR / TRI / SIN, PHASE's eight waves and LOFI's five legacy waves (not its
BYTE and FLOAT formulas), and the DRUM-X patches (see [DRUM-X](drum-x.md)). It is on the **MORE** page (KNOB 2) of the EDIT family on those engines.

**PHS FB** (PHASE feedback) is PHASE's independent feedback amount, on MORE (KNOB 3) on PHASE only; PHASE FOLD keeps its slot E8.

Both default to zero, so existing sounds are unchanged. They support step locks, automation, MIDI Learn and are MOD destinations (MORPH, PHS FB).

## Storage

Sounds use a 113-parameter layout: MORPH is parameter 103, PHS FB 104, E1..E8 105..112; no FM operator parameter is reused. Existing projects and
user presets load by parameter count; projects saved with the newer layout need Caravel or newer firmware.
