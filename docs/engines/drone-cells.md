<!-- SPDX-License-Identifier: GPL-3.0-only -->
# DRONE cell modes

**Experimental, opt-in; not a release or a LYRA clone.** Four coupled generators
still use one host voice. The five original DRONE presets and TOPO values 0..2
retain their previous sound and whole-network transposition behavior.

## Try it

Choose **DRONE** with any cell preset (CELL AIR, CELL CHN, CELL RNG, GLASS CHOIR, ...) or just select DRONE, which now starts in a cell mode. Start at a low monitor
level. Hold a low anchor note, then briefly add other notes while keeping the
anchor held. Those gestures retune and excite individual cells, not the entire
network. COUPLE changes PM and the new slow neighbor suppression together.
Lower COUPLE to reduce both effects; allow several seconds for activity to settle.

- A fresh phrase seeds all four cell pitches from the first note and SPREAD.
- New notes rotate through cells 1..3. Cell 0 keeps the anchor pitch. A repeat
  re-excites its existing owner if that owner has not been replaced.
- A new gesture has a local velocity-dependent excitation. Releasing its note
  lets that cell decay while the remaining held cells continue.
- The last held note-off releases the normal shared ADSR; the engine has no latch or
  autonomous background sound (the track's LATCH, [latch.md](../latch.md), simply never sends the note-offs). Panic and engine changes retain host authority.
- Replaced note owners cannot release their replacement. Repeated note-on/on/off
  is one held key, matching the host policy. The internal held mask covers MIDI
  notes 0..127, not only the eight entries retained by the host mono stack.

**TOPO** appends three modes without changing the original values:

| Value | Name | Behavior |
| --- | --- | --- |
| 0 | PAIRS | Original whole-network instrument |
| 1 | CHAIN | Original whole-network instrument |
| 2 | RING | Original whole-network instrument |
| 3 | CELLPAIR | Selective cells, paired coupling |
| 4 | CELLCHN | Selective cells, chain coupling |
| 5 | CELLRING | Selective cells, ring coupling |

## The eleven cell presets

Indices 5..15, all cell modes, dry, with their own slow ADSR. The parameter sets were chosen
by a measured search so that all eleven are at least **8 dB apart** in long-term spectrum
(24 third-octave bands of one held C1, levels relative to the total, floor -60 dB, 30-60 s;
before the retune the closest pair was 1.6 dB). That is a measurement of spectral difference, not a
listening verdict; temporal character (beating, pulsing) is part of the design but not of
that metric. **Since 0.3.2 the parameter sets are the same but the sound changed** (see below), and
the closest pair is now IRON BELLS / RUST at **4.2 dB** (SUB FOG / SWARM was the closest with 8.27 dB);
the test `dc_preset_separation` therefore asks for 4 dB, a deliberate re-baseline, not a re-tune.
Pairs below 6 dB are CELL AIR / SWARM, CELL RNG / IRON BELLS, CELL RNG / RUST and SWARM / MOON PULSE:
the presets want a listening pass. Names describe the measured tendency:

| # | Preset | Topology | Tendency |
| ---: | --- | --- | --- |
| 5 | CELL AIR | CELLPAIR | Open and calm: wide SPREAD (octave), strong DETUNE beating, almost no CHAR, very slow drift. |
| 6 | CELL CHN | CELLCHN | Pure and dark: no modulation (COUPLE 0), muffled, slowly beating sines in a chain. |
| 7 | CELL RNG | CELLRING | Dark FM-rough ring: high COUPLE, low TONE, moderate CHAR. |
| 8 | GLASS CHOIR | CELLPAIR | Bright shimmer: unison SPREAD with DETUNE, COUPLE 91, TONE fully open, lively movement. |
| 9 | TIDE | CELLRING | Very slow beating tide: strong DETUNE, low COUPLE, very low flux. |
| 10 | IRON BELLS | CELLRING | Metallic bells: maximum COUPLE, octave SPREAD, bright. |
| 11 | SUB FOG | CELLCHN | Very dark rumble: TONE 15, unison cells, strong drift. |
| 12 | SWARM | CELLPAIR | Restless, saturated and bright: high CHAR, strong DRIFT, TONE open. |
| 13 | STATIC | CELLPAIR | Harsh bright wire-like noise tone: COUPLE 127, CHAR 90, TONE 120. |
| 14 | RUST | CELLRING | Rough and gritty: high CHAR, ring PM, bright TONE. |
| 15 | MOON PULSE | CELLRING | Pulsing dark FM pad: unison cells, COUPLE 89, fast RATE. |

New DRONE sounds (engine selected without a preset) now start in **CELLPAIR** with
SPREAD 64, DETUNE 30, COUPLE 40, CHAR 30, DRIFT 60, RATE 40, TONE 76. The original five
presets (0..4) and TOPO 0..2 are unchanged.

**DRIFT now also breathes the cells** (cell modes only): each cell's PM depth varies
between 0.4x and 1x of COUPLE and its loudness between 0.3x and 1x, following the slow
deterministic drift triangles (a neighbor's phase for loudness). DRIFT 0 leaves both
exactly constant; RATE sets the speed. A new phrase also starts its drift triangles at
a different place, so repeated phrases are not identical. All of this is bounded by the
same quarter-turn PM limit and adds no state, delay or divide. It is slow variation, not
autonomous re-triggering. (0.3.2 adds a further, separate movement, BREATH, below.)

## 0.3.2: wave shape, roughness, breathing, no whistle

**Unreleased; host-tested only. The sound has not been heard on a real FM-1**, and the choice of the
settings comes from a host prototype and listening to its renders, not from the device. No new
parameter and no format change: the cell modes (TOPO 3..5) of the existing presets and projects simply
sound different; TOPO 0..2 and the five original presets are bit-identical (golden renders unchanged).

- **CHAR is also the wave shape (cell modes).** CHAR 0 is a pure sine, as before. Turning it up the
  generator goes sine, then triangle (CHAR about 69), then a soft, rounded square (about 0.65 of the way at CHAR 89,
  the value of GLASS CHOIR, at most 0.8 at CHAR 127; "SHARP"). The shape is computed from the
  phase-modulated angle of the cell, so PM still colours it; the former soft-cubic CHAR blend follows on top. A little
  level makeup keeps the loudness about the same. Measured on one uncoupled cell: the third harmonic is
  -76 dB below the fundamental at CHAR 0, -50 dB at 30, -28 dB at 89, -25 dB at 127 (`dc_sharp`).
- **Roughness (ROUGH).** How far a cell sits from the anchor cell 0 in pitch decides how it behaves: the
  interval (from the real frequencies of the cells, SPREAD and DETUNE included, folded into one octave) is looked up in a
  roughness table (unison and fifth 0, minor second and major seventh 1, tritone 0.8, the thirds and
  sixth in between). A rough interval raises the cell's coupling to 1.6x COUPLE and its CHAR to 1.4x, a pure one lowers
  them to 0.9x and 0.8x; cell 0 is unchanged. The roughness glides over about 23 ms. So a chord of pure intervals stays calm,
  a cluster grows teeth. With SPREAD 0 and no DETUNE all intervals are unisons: pure.
- **Breathing (BREATH).** Each cell's loudness also moves on a slow triangle of its own, 0.17, 0.23, 0.31 and
  0.43 Hz, up to 25 % deep at DRIFT 127 and proportional to DRIFT below; DRIFT 0 is exactly neutral (as the
  existing breathing above). The four rates do not line up, so the movement does not repeat quickly.
  Note that GLASS CHOIR has DRIFT 53, so it breathes about 10 %, where the prototype used a fixed 25 %.
- **Whistle fixed.** The cell loop has a one-sample delay; with CHAR and COUPLE up its loop gain
  reached x1.5 near fs/4 (11 kHz), a steady whistle in GLASS CHOIR and STATIC. A one-pole low-pass (1/2 per sample) in the
  feedback path removes it: the strongest 100 Hz band at 10.2-11.8 kHz is -58 dB (GLASS CHOIR) and -50 dB (STATIC)
  below the whole spectrum, and was -18 and -11 dB without it (`dc_whistle`). The side effect is
  that the cells are duller at high COUPLE: the test's brightness ratio to the legacy RING at COUPLE 127
  went from 0.75 to 0.54, so its lower bound is now 0.5 (it was 0.6).

How it was checked, and what that does not say. The same 66 s phrase as the host prototype's
render (GLASS CHOIR, held anchor and added notes, `holds`, and flurries, `swarm`) was rendered with this
firmware code and compared with the prototype's chosen variant (ROUGH, low-pass, BREATH, SHARP 650) in six bands against
the total energy, 5 s windows: the mean band levels differ by at most 0.3 dB below 3 kHz, 0.7 dB at 3-6 kHz,
2.4 dB at 6-12 kHz and 1.9 dB above 12 kHz (single windows up to 4.7 dB). It is brighter at the top than the prototype by that much.
That is a comparison of two host renders, not a listening test and not a device measurement. The extra work in the cell renderer is
about +24 % of its static loop cost on the target (1471 to 1824 in `tests/target_budget.py`; the slow quantities are updated every
8th sample); the real CPU load of an FM-1 has not been measured.

## Controls and deliberate limits

The eight stored parameter IDs remain unchanged. Performance knobs are still
**COUPLE / CHAR / DRIFT / TONE**. SPREAD seeds the initial cell intervals at a
fresh phrase; editing it mid-phrase does not rearrange existing cell pitches.
Explicitly retuned cells use their played pitch instead of a seeded ratio.
DETUNE and the other controls remain live. TUNE, MIDI bend and pitch-modulation
offsets affect the local pitches. Shared host GLIDE is not used to transpose the
network after a gesture; local retuning has the existing ~23 ms smoothing.

Each cell has a smooth excitation/decay state and an activity follower. Held
excitation settles toward 75% of its velocity-derived initial level. A routed
neighbor activity can suppress its target by **less than 50%**. The cell amplitude
weights its audible output; its modulation signal follows the same amplitude with
4x makeup (saturating at unity from 25 % amplitude), so a sounding cell
modulates its neighbors about as strongly as a legacy generator and a silent cell
still modulates nothing. This is a bounded
inhibition experiment, not physical energy simulation, autonomous re-excitation,
emergent rhythm, or a promise of chaotic motion. No extra oscillators or internal
delay were added. PM and slow inhibition share COUPLE; the current UI does not
provide an independent P2-depth control.

Live switching between legacy/cell modes preserves phases, with smoothing on
entry and a short output bridge on exit. A live entry seeds from a note that is actually held (tracked from real
note-on/off events in both families); it cannot reconstruct a previous
independent-cell history. The host voice may still name an already released key
while another is held, so it is never used to infer held keys.
High-register PM can still alias. A one-voice allocation does not imply a cheap
render: four generators and the cell updates execute inside that voice.

## Storage and safety

Engine ID remains **16**, with unchanged settings/project layout. Presets 0..4
are unchanged; the cell presets append at 5..15. MIDI Learn, locks, user presets,
project/FULL loads and matching editor descriptors cover the new values.

Older firmware does not understand TOPO3..5 or the three new presets and cannot
reproduce these sounds. Back up sounds/projects before testing or downgrading.
Do not silently reinterpret old presets as cell-mode sounds.

## Local browser and listening comparison

Build with `sh web/emu/build.sh` (the page shows the firmware version) after a firmware
build, then serve `build/emu/` over loopback HTTP as described in the
[original DRONE reference](../drone-engine.md). The matching local browser can
play all eight presets; no physical device installation is needed.

For reproducible dry A/B renders:

```sh
mkdir -p build/drone_cells_demo build/host
cc -O2 -w -Ibuild/gen -Ifirmware/src tests/drone_cells_demo.c -lm -o build/host/drone_cells_demo
build/host/drone_cells_demo
```

`legacy.wav` and `cells.wav` use identical DARK RING parameters except TOPO2 vs
TOPO5, the same C2 anchor for 120 seconds, additional note gestures and coupling
edits, then eight seconds of release. They compare the complete behaviors, not
an isolated P2 effect. Listen dry first. RMS-normalized local `*-matched.wav`
files (target RMS 0.12 full scale, peak cap 0.8; made from the dry renders by a
separate local step, not by the demo program) are listening aids, not perceptual loudness certification. No subjective
sound-quality or physical FM-1 result has been established by the render tests.

