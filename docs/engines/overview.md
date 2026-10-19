<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Engines at a glance

Caravel has sixteen engines, each with factory presets and eight engine controls (EDIT 1 / EDIT 2). Pick one in PRESETS or with
EDIT held and KNOB 1. The "good for" column is a starting suggestion, not a rule: every engine goes through the same track
facilities (ADSR, filter envelope, ARP, LFOs, MOD matrix, FX), so most of them can do more than the label says.

| Engine | What it is | Good for |
| --- | --- | --- |
| **ANALOG** | Two band-limited oscillators (saw, square, triangle, sine, PWM) with hard sync, a sub oscillator, noise, drive and a low-, band- or high-pass filter | The all-rounder: classic leads, basses, pads, plucks |
| **SEM** | Oberheim SEM-inspired: two oscillators, a state-variable filter that blends low-pass, notch and high-pass, per-voice vintage drift | Warm pads, brass, strings, soft keys, notch sweeps ([SEM](SEM_ENGINE.md)) |
| **HADES** | Hades-inspired bass: saw/pulse plus two phase-locked sub oscillators, resonant 3-pole low-pass, drive and a bass shelf | Basses and sub layers, dark leads ([HADES](hades-engine.md)) |
| **DRONE** | Four coupled sine generators in one voice: pairs, chain or ring coupling, slow drift, cell modes. Experimental | Evolving drones and beds, bells, slow textures ([DRONE](../drone-engine.md), [cells](drone-cells.md)) |
| **PHASE** | Phase distortion (a bent cosine, controlled by DCW), a second line mixed or ring-modulated, sub, wavefolder, MORPH and feedback | Bright, glassy and organ-like tones, bells, wire, brass ([MORPH and feedback](morph-and-feedback.md)) |
| **LOFI** | A chip voice: pulse, triangle, saw, shift-register noise, a 32 x 4-bit wave memory, bit depth, sample hold, pitch sweep, arpeggio; BYTE and FLOAT formula waves | Chiptune, 8-bit arps and leads, bytebeat experiments ([BYTE and FLOAT](lofi-byte-float.md)) |
| **TRIO** | Three oscillators with ring modulation, hard sync and combined waves into a 12 dB/oct multimode filter | Fat basses, sync and ring-mod leads, bell-like metal |
| **WHEEL** | A tonewheel-style organ: nine drawbars, 16 registrations, percussion, key click, drive and a rotary effect | Organ parts from jazz to rock |
| **FM6** | Classic six-operator FM (the msfa/Dexed core), 32 algorithms, patches from the web editor or factory slots, macros on the device | Electric pianos, bells, FM basses and pads |
| **PHYS** | Physical models: MODAL (bells, bars, plates), STRNG (plucked and bowed strings), MEMB (drum heads), SYMP (strings with sympathetic strings) | Mallets, plucks, hand drums, metallic textures |
| **VOICE** | A formant oscillator (after Klatt, 1980): sung vowels from the keyboard, with breath and whisper | Choirs, vocal leads, talking basses |
| **SAMPLE** | Built-in CC0 sample sets (PIANO, FLUTE, SAX) and three user slots (USR1..3, 80 KiB each, loaded with the web editor or `tools/fm1_sample_upload.py`) | Realistic instrument parts and your own sounds |
| **GRAIN** | A granular engine over the SAMPLE material and the user slots | Clouds, freezes, shimmer, glitch textures |
| **SLICE** | A sample slicer played from the keys and the sequencer: 4/8/16/32 equal slices, AUTO onsets or manual slices | Breakbeat chopping, stutters, sliced loops |
| **DRUM** | A drum kit of eight lanes (KICK, SNARE, CLAP, HAT CL, HAT OP, TOM, RIM, BELL) on the General MIDI map, several kits, and **DRUM-X** with A/B patches and MORPH | Drums and percussion, the grid, GM patterns ([DRUM-X](drum-x.md)) |
| **NOISE** | White, pink and brown noise, random impulses and shift-register noise through a resonant filter, with drift and a bit crusher | Wind, rain, risers, hats, effects and arcade sounds |

## Notes

- **Presets:** SEM, HADES and DRONE presets are listed in their guides. The factory presets of HADES and DRONE are dry (no
  FX sends), so you hear the engine itself first.
- **Voices:** all tracks share eight voices; DRONE uses one voice for four generators. MENU > VOICES reserves voices per track
  ([Voice reservations](../voice-reservations.md)).
- **MORE page** (EDIT family): SPREAD, MORPH and PHS FB for the engines that have them.
- Which engine came from where: ANALOG, LOFI, TRIO, WHEEL and NOISE are the MIT-licensed
  [Felucca-Engines](https://github.com/hugelton/Felucca-Engines); FM6 uses msfa (Dexed), PHYS uses DaisySP and Rings, VOICE follows
  Klatt's formant synthesizer. See [CREDITS](../../CREDITS.md).
- Tips for getting good sounds out of SEM, HADES and DRONE: [Tips and tricks](tips.md).
