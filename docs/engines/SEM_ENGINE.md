# SEM — capabilities and controls

SEM is an Oberheim SEM-inspired virtual analogue engine by bonszaimusic (Caravel). It is
an original interpretation, not a circuit-identical emulation; ANALOG remains available.
The name Oberheim and its products belong to their owners.

This document describes what the engine offers and how to use it.

## Selecting SEM

In BUILD, hold EDIT to choose **SEM** and a preset. In PLAY, tap EDIT for SOUND and browse its EDIT groups. See the [1.0 tutorial](../tutorials/engine-sem.md). Internal engine numbers are not the stored FORMAT 2 engine index.

## Sound and controls

- Two oscillators with independent waveform selection and detuning.
- Low-pass / notch / high-pass blending, plus fixed band-pass and high-pass modes.
- Pulse-width modulation and per-voice vintage variation.

| Page | Controls |
| --- | --- |
| HOME | CUT, RES, STATE, VINT |
| EDIT 1 / OSC | OSC1, OSC2, DTN, MIX |
| EDIT 2 / SEM FLT | CUT, RES, STATE, VINT |
| SEM MODE | TYPE, LFO SHP, SPREAD |

### Oscillators and PWM

OSC1 and OSC2 independently select SAW, PULSE (50%), TRI or PWM. DTN detunes
OSC2 by 0–50 cents. MIX crossfades the oscillators. For PWM, SHP from the LFO,
envelope or modulation matrix moves pulse width. Start with a slow triangle LFO
and a modest LFO SHP amount.

### Filter

TYPE **STATE** enables the 12-dB/octave low-pass → notch → high-pass blend:

- STATE 0: low-pass.
- Raw STATE 64 (about 50%): notch.
- STATE 127 (100%): high-pass.

TYPE **BP** selects band-pass; **HP** selects fixed high-pass. STATE is inactive
in those two modes. Resonance does not reach self-oscillation. STATE changes are
smoothed to avoid abrupt jumps.

### Vintage variation

VINT adds small pitch/filter calibration differences between voices and slow
local tuning drift. At 0, these variations are disabled.

## Presets and track facilities

Eight factory presets: **SEM BRASS, WARM PAD, OB STRING, ROUND BAS, NOTCH PAD,
SEM LEAD, HIGH AIR, SOFT KEY**.

ENV, LFO, MOD, ARP, FX and AUTO/FIXED voice allocation are the existing track
facilities. The eight engine controls support MIDI Learn, sequencer automation
and locks. TYPE supports MIDI Learn and saving, but is not a sequencer motion
target. SPREAD and LFO SHP use the existing track controls.

## Saving and compatibility

SEM sounds and projects require firmware supporting engine 14. Older firmware
rejects that engine; back up before downgrading. Existing engine, parameter and
preset IDs are retained.

SEM favorites live in dedicated maps of the PER7 settings. PER7 settings cannot be imported
by older firmware.

## Validation and remaining limits

The host suite covers SEM controls, filter behavior, MIDI Learn, saving, automation,
mixed voice allocation and one/two-core checks. All 117 golden renders of the other
engines are unchanged.

This is not hardware acceptance. Check sustained eight-note chords and a
four-track mix with DRUM-X, sends/inserts, rapid learned CC sweeps, live A–D
loads, USB audio, external clock, note release and power-cycle persistence on
the FM-1. Record the firmware SHA, audible glitches and measured deadlines.
Host timings are not FM-1 CPU percentages.
