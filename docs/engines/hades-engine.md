# HADES — capabilities and controls

HADES is a Hades RE-inspired digital bass engine by bonszaimusic (Caravel). It is an
original interpretation, not a circuit-identical emulation, Dreadbox firmware port or
endorsed product. Dreadbox and Hades belong to their owners.

This document describes its available features and operation.

## Sound and controls

- Main saw/pulse oscillator with pulse-width modulation.
- Two phase-locked suboscillators, one and two octaves below the played note.
- Resonant three-pole low-pass filter.
- Post-filter soft drive and a BODY bass shelf.

| Page | Parameter | Meaning |
| --- | --- | --- |
| OSC SUB | WAVE | OFF / SAW / PULSE; OFF leaves the suboscillators available |
| OSC SUB | PW | Pulse width; existing SHP modulation adds PWM |
| OSC SUB | SUB1 | Level one octave below the played note |
| OSC SUB | SUB2 | Level two octaves below the played note |
| FLT DRV | CUT | Low-pass cutoff; existing FLT modulation applies |
| FLT DRV | RES | Filter resonance, bounded below self-oscillation |
| FLT DRV | DRV | Post-filter soft drive |
| FLT DRV | BODY | Low-frequency shelf gain |

HOME exposes **CUT / RES / DRV / BODY**. All eight engine controls are editable,
MIDI-learnable, automatable and stored in projects/user presets. The eight
factory presets are dry, so you can evaluate the engine without track FX.

## Playing and modulation

MONO/LEGATO are suggested for bass. POLY uses Felucca's normal shared voice
budget. The engine uses the existing track ADSR, filter-envelope amount, glide,
LFOs, modulation matrix and effects. MIDI Learn follows normal Felucca routing,
not the original hardware's MIDI restrictions.

Useful starting points:

- Set WAVE to OFF to hear only the suboscillators.
- Mix SUB1/SUB2 with SAW or PULSE for a layered bass.
- Route SHP modulation to pulse width, or FLT modulation to cutoff.
- Evaluate DRV and BODY with effects off before adding track FX.

## Limits

- No circuit-identical nonlinear OTA model or measured hardware matching.
- No self-oscillation in the modeled filter.
- No separate emulation of the hardware's filter AD/RS and amp AR envelopes;
  the existing Felucca envelope facilities remain in use.
- No audio-rate patching.
- BODY and DRV do not claim exact matching to the hardware controls.

## Saving and compatibility

HADES uses engine ID 15, after SEM at 14. Existing engine IDs and the eight
engine parameter slots retain their values. HADES sounds require firmware
supporting that engine.

The HADES integration introduced **PER7 settings**, with dedicated favorite
maps for engines 14–21. PER1–6 settings import and PER4–6 backup restoration
remain supported. Older firmware cannot import PER7: back up before downgrading.


## Validation and remaining limits

The host suite includes HADES checks of filter/divider behavior,
parameter extremes, MIDI Learn, automation, saving, favorites migration,
strict DSP sanitization and browser-emulator audio. All 117 current golden
renders of the other engines are unchanged.

FM-1 acceptance with multiple tracks and FX remains necessary. Host timings and
static instruction budgets are not device CPU percentages or a real-time
deadline guarantee.
