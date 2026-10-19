# DRONE — coupled network

**Scope:** the TOPO0..2 instrument.
For the opt-in TOPO3..5 cell modes, see the [cell guide](engines/drone-cells.md).


Original DSP inspired by the idea of a coupled, slowly moving drone instrument.
Not a LYRA clone, circuit simulation, sonic equivalence claim, or port of any
manufacturer's source. No clone source was used.

## Status and use

**Experimental.** The cell extension is described in the [cell guide](engines/drone-cells.md).
DRONE is appended at engine ID **16**, after HADES. Existing engine IDs and
project/settings layouts are unchanged. Firmware and web-editor descriptors,
factory categories and the existing PER7 favorites capacity include DRONE.

Select **DRONE / STILL AIR**, play and hold a low note (for example C3), and
raise COUPLE slowly. The four performance knobs control **COUPLE, CHAR, DRIFT,
TONE**. Try PAIRS, CHAIN and RING on the first EDIT page. Notes transpose the
whole instrument. Long ADSR settings need a held note or sequencer ties; the
engine does not latch itself, but the track's [LATCH](latch.md) (CHORD page) keeps the keys' and MIDI notes held for it.
Begin listening at a low monitor level.
The existing delay/reverb can be added, but factory presets are dry so the
network can be evaluated without effects masking its behavior.

**Verified:** target firmware build and memory checks, native renders,
parameter/voice/storage/editor tests, regression hashes and CPU2 host parity.
**Not verified:** listening quality, LYRA sonic similarity, physical FM-1 CPU
headroom/timing, or device acceptance under sustained MIDI/USB/FX load. Host
thread parity and sanitizers are not substitutes for hardware validation.

## Instrument and signal path

`ENG_DRONE` is named `DRONE`, with `drone_note_on` and `drone_render` callbacks.
It has `.poly = 1`: one tracked host voice drives four audible generators, not
four independently playable voices. A note transposes the **entire network**;
there are no independently tuned LYRA-style touch generators. POLY mode can
steal/retrigger this single voice; MONO/LEGATO follows the existing host policy.
UNISON is still capped at one tracked voice. All four generators share the
host's ADSR, velocity, amplitude modulation, pan/spread side, and note gate.
There is no individual oscillator envelope, touch gate, engine latch, or autonomous
sound after the normal voice envelope/panic has stopped rendering it. The track's LATCH ([latch.md](latch.md)) works
above the engine: it only withholds the note-off, so the engine still sees a held note.

Each generator uses interpolated `sine_i`, optionally blended toward the odd
soft-character polynomial `1.5*x - 0.5*x^3`. All four are always averaged at
equal weight, including chain nodes; coupling does not make nodes inaudible.
Their previous-sample character outputs feed bounded phase modulation:

- **PAIRS:** 0 receives 1, 1 receives 0; 2 receives 3, 3 receives 2.
- **CHAIN:** 0 has no incoming connection; 1 receives 0, 2 receives 1,
  3 receives 2.
- **RING:** 0 receives 3, 1 receives 0, 2 receives 1, 3 receives 2.

All nodes read the same previous-sample snapshot, and publish only after all
four have evaluated. Changing evaluation order must not change the network.
Topology changes crossfade input routes through PAIRS -> CHAIN -> RING; a
PAIRS-to-RING jump deliberately traverses CHAIN. Feedback never writes into
the accumulated carrier phase or frequency: PM is bounded to less than a
quarter turn. There is no delay effect, delay line, allocator, shared RNG,
mutable global DSP state, or self-sustaining envelope bypass.

After averaging: first-order DC rejection (approximately 14 Hz at 44.1 kHz),
one-pole tone low-pass, `soft_knee(..., 16000)`, then exactly the HADES/SEM
`voice_amp(sample * 2, m, i) * 2` output convention. The DC rejection and tone
filter are shared by the network; the tone multiply uses a wide intermediate
because DC rejection can transiently exceed the original waveform range.

## Exact parameter mapping

All numeric controls are 0..127; topology is 0..2 in this original instrument
(TOPO 3..5 are the cell modes; see the [cell guide](engines/drone-cells.md)).

| Slot | Label | Default | Mapping |
| --- | --- | ---: | --- |
| `P_E0` | SPREAD | 0 | Let `u = value/127`. Ratios are `1`, `1 + u/4`, `1 + u/2`, `1 + u`. Opens unison continuously to `1 : 5/4 : 3/2 : 2`; intermediate values are linear frequency ratios, not equal-tempered semitone steps. |
| `P_E1` | DETUNE | 24 | Relative tuning factors `1 + fine/65536`, with integer `fine = value * {-3,-1,1,3} * 252 / 127`. At maximum the outside generators are approximately -20.1/+19.9 cents; inside generators about one third as far. |
| `P_E2` | COUPLE | 12 | PM gain `clamp(value*256 + m->shape - 64*256, 0, 32512)` in Q15. PM phase offset is the resulting Q15 source times 32768 phase units (less than +/-0.25 cycle). |
| `P_E3` | TOPO | 0 | `0=PAIRS`, `1=CHAIN`, `2=RING`; continuous route crossfade during changes. (`3..5` are the cell modes; this table describes `0..2`.) |
| `P_E4` | CHAR | 18 | Q15 sine-to-soft-cubic blend, `value*258`. No hard clipping or wavefolder. |
| `P_E5` | DRIFT | 32 | Q15 triangle drift amount `value*258`; frequency deviation up to approximately +/-1/256 (+/-6.8 cents), independently phased/rated for each generator. |
| `P_E6` | RATE | 20 | Base drift phase increment `1461 + smoothed(value*258)*11/20`; generator multipliers `{8,9,10,11}/8`. At 44.1 kHz approximately 0.015..0.200 Hz for generator 0, up to 0.275 Hz for generator 3. The full range stays slow. |
| `P_E7` | TONE | 76 | Q8 target `clamp(value*256 + m->cutoff, 0, 32512)`; one-pole coefficient `128 + smoothed_target*30000/32512` in Q15. Dark-to-open, nonresonant; not the host's calibrated cutoff-frequency scale. |

The four performance knobs select `{P_E2, P_E4, P_E5, P_E7}`.
Shape's neutral host value is `64*256`; only its offset modulates coupling.
Cutoff modulation moves tone. Both are clamped before smoothing. Pitch uses
`m->inc` directly, retaining the host's fine tuning, bend, glide and modulation
rather than rebuilding an increment from a rounded MIDI note.

Six timbral/route controls and the four final carrier increments use per-sample
integer exponential smoothing (nominal 1024-sample / 23 ms time constant at
44.1 kHz, exact integer endpoints). Thus interval/detune edits and note changes
are smoothed through frequency, and rapid pitch modulation is intentionally
low-passed. First-ever rendering installs pitch immediately rather than
sweeping up from zero. Topology, character, tone, coupling, drift amount and
drift rate do not jump on a live edit. Targets are sampled per render call.

Drift is deterministic and bounded, with no cumulative tuning error. Triangle
corners are continuous but not differentiable. Its phase pauses while the host
is not rendering the voice; it is not a wall-clock/free-running LFO.

Five original presets (the cell modes add further presets, indices 5..15) use long host ADSR values with high sustain (ADSR units are the
existing host table indices, not seconds), no sends, and suggested PAD pattern
5: STILL AIR, OPEN FIFTH (a four-note just-ratio network, not only a fifth),
SLOW CHAIN, DARK RING, BEAT FIELD. Presets do not auto-latch a note.

## Memory, CPU2, reset and retrigger contract

`drone_part_t` contains only 32-bit members: 104 bytes per part in the original
instrument (252 bytes with the append-only cell state), drawn from
`ENG_MEM((uint32_t)(t - trk))`, not a new global buffer. `.mem` advertises
`sizeof(drone_part_t)`; `ENG_MEM_FITS("DRONE", sizeof(drone_part_t))` enforces
fit in the existing 12888-byte per-part region without enlarging it. Each part
has its own phases, previous outputs, smoothed controls, filter and drift.
Static tables are read-only. This follows CPU2's one-render-owner-per-part
model; it does not make concurrent rendering of the same part safe.

`.keep = 0` is deliberate: none of the host `voice_t.ph`/`voice_t.s` slots
contains DRONE DSP state. The host's partial keep restoration therefore cannot
restore only half the network. `voice_start` sets active/stage before calling
`note_on`, preserves amplitude on sounding retriggers, and may restore phases
and selected voice slots afterward. Neither active/stage nor `env_out` is a
reliable fresh-network discriminator here: AMP modulation can make `env_out`
zero on a sounding voice. Instead a pool-local `ready` field is initialized
only after a zeroed-memory claim; sounding retriggers never reset this state.

- **Fresh:** zeroed region (power-on or another memory-owning engine has claimed
  the part) seeds deterministic, unequal carrier/drift phases, clears previous
  outputs and filters, initializes controls, and installs pitch on first render.
- **Warm:** repeated notes, sounding retriggers, steals, completed releases,
  silence and panic retain the network while DRONE still owns the pool. No
  hidden rendering occurs during silence; the next envelope resumes the paused
  state, with smoothed movement toward the new note/controls.
- **Switches:** the inspected `eng_mem_claim` clears a new owner's advertised
  bytes before note-on. Switching via a memoryless engine does **not** change
  the owner; returning can therefore be warm. This prototype intentionally
  follows that existing contract rather than promising a reset on every switch.
- **Panic:** the engine has no independent output callback/tail or note latch (the track's LATCH, [latch.md](latch.md), ends on a panic).
  It relies on the normal host voice/envelope/panic path. Retaining internal
  samples does not itself add output when the host does not render it.

## Limitations and parent integration checklist

No oversampling or dedicated anti-aliasing is provided. PM/character generate
sidebands; high notes and strong coupling can alias despite the tone filter.
Each carrier increment is capped at `0x60000000` (0.375 cycles/sample,
16.54 kHz at 44.1 kHz), including drift. At the cap intervals and transposition
are no longer exact. This is a drone-oriented prototype, not a full-bandwidth
FM engine. Carrier tuning is correct only within that explicit ceiling; PM
itself can alter perceived pitch and introduce DC/asymmetric spectra.

The one-voice budget accounts for one host voice, but it performs four sine
lookups and four soft-character evaluations per sample, plus smoothing,
routing and filters. CPU2 safety is a state-ownership argument, not a measured
speed claim. The per-sample wide tone multiply and per-block wide pitch
multiplies require target profiling. Output normalization follows host
conventions but perceived loudness and worst-case headroom remain unmeasured.

## Build and test evidence — 2026-10-17

Same worktree, submodule and sample assets for baseline and preview. The sample
build contains five sets / eleven zones, 126244 bytes of ADPCM. No existing
engine or sample set was removed to fit DRONE.

| Resource | Base build | DRONE preview | Limit |
| --- | ---: | ---: | ---: |
| App image (bytes) | 559060 | 561344 | 581564 |
| RAM `.data + .bss` (bytes) | 65436 | 65436 | 73728 |
| Pool (bytes) | 331656 | 331656 | 368640 |

The build-to-build image increase is 2284 bytes (also includes preview version
metadata); 20220 bytes remain in the app slot. The 104-byte state per active
DRONE part reuses the existing engine pool rather than enlarging it. The table
is not a claim that stacks, dynamic execution or audio deadlines were measured.
Build logs: `build/baseline-build.log`, `build/drone-build.log` (local artifacts).

Build: `sh build.sh --release 1.6.1-drone-preview-1` with dependencies from
`BUILDING.md`. Package: `build/caravel-1.6.1-drone-preview-1.fwsc`; no device was
flashed. The local build used a venv for Pillow/fontTools and Docker/JieLi.
The original temporary SDK directory disappeared between builds; the three
packaging SDK files were recovered into local ignored `build/sdk` from the
successfully built preview package and verified against all three reference
SHA256 values in `tools/build.py`. Set `AC79_SDK` to a valid SDK or that local
minimal packaging directory to reproduce this build; no SDK files are committed.

Validation performed (selected suites, not the complete `run_tests.sh`):

- `tests/run_drone_tests.sh`: all eight MIDI Learn targets, locks, project and
  user-preset roundtrips, favorites isolation; 2304 parameter/pitch/topology
  cases with memory canaries; deterministic output, exact AMP-zero silence,
  chunk equivalence, per-part isolation, warm retriggers, pitch bins,
  release/panic and overlapping notes in every host voice mode.
- Existing SEM, HADES, independent LFO2/editor, UI (CPU2 compiled in/out), editor,
  projects, voice reservations and robust-input suites passed.
- Regression: 122 renders, zero changed/gone hashes, zero health or routing
  failures or crashes, default / CPU2 compiled out / threaded CPU2 host.
  Five DRONE golden and native CPU entries appended; existing entries retained.
- Engine-memory regression: 720 existing-engine sequences passed for each of
  CPU2 compiled in/out. DRONE isolation is covered separately above.
- Dedicated threaded parity: 48 comparisons passed for four DRONE parts and
  a DRONE/HADES/SEM/FM6 mix, with sends/inserts/live control changes.
- ASan/UBSan with the existing suite DSP exclusions passed; dedicated threaded
  parity also passed ThreadSanitizer. An earlier strict integration UBSan run
  reported negative signed shifts in existing `fx.c` DC-block code; no report
  originated in DRONE. This is not a blanket strict-UB-clean claim.
- Fresh descriptor generation and `node web/test_web.mjs` passed.
- `tests/target_budget.py build/felucca.dis tests/target_budget.txt` passed,
  including the new DRONE target loop-cost baseline of 828 weighted instructions.
  This static metric is not target cycles or CPU utilization.

Native CPU regression measured 837..852 host instructions/sample including the
mixer (idle 327), or about 510..525 incremental instructions for one four-generator
network. The scheduler uses a provisional conservative cost hint of 800 per
network; it must not divide this by eight. Host counts do not establish device
CPU reserve. Regression peak was 18386, absolute DC at most 7.8, voices freed
within 4.61 seconds of note-off, and final tails were exactly silent under that
specific regression setup. These are native-render observations, not hardware
measurements or guarantees for every sound/FX combination.

Listening renders: `tests/run_drone_tests.sh --demo` writes
`build/drone_demo/0.wav` through `4.wav`, in preset order. Each holds C3 for
20 seconds then allows eight seconds of release. The first eight seconds keep
the factory settings; the remainder sweeps coupling and changes topology.

## Remaining acceptance work

1. Audition presets and sustained parameter moves; tune loudness, cancellation,
   excessive brightness/aliasing and musical coupling ranges by ear.
2. Measure FM-1 timing with one and four networks, other expensive engines,
   delay/reverb/inserts, saturated CC/clock and USB audio; CPU2 OFF and ON.
   Include long drones, voice stealing, sound/project changes, mute and panic.
3. Decide after those measurements whether eight generators or an internal
   delay are worthwhile. Neither is implemented or promised by this preview.

Older firmware does not know engine ID 16. Back up projects/settings before
hardware experiments; a project using DRONE must not be assumed compatible
with firmware that predates the engine. No automatic installation or release.

## Browser preview — 2026-10-17

The matching emulator was built with `EMU_VERSION=v1.6.1-drone-preview-1 sh
web/emu/build.sh` (Emscripten on PATH). `EMU_VERSION` changes only the preview
label; it does not edit the firmware source version. Assets are in `build/emu/`.

Serve that directory locally, for example from this worktree:

```sh
python3 -m http.server 8791 --bind 127.0.0.1 --directory build/emu
```

Open `http://127.0.0.1:8791/`, click **Switch on**, and use PRESETS to browse to
DRONE (knobs support dragging vertically and the mouse wheel). Hold a piano
key while adjusting the controls. The preview is local; no device installation
or public deployment is required. Browser-saved state is local to its origin.
Stop the server with Ctrl+C. Do not open `index.html` via `file://`, because
WebAssembly/AudioWorklet loading needs a local HTTP origin.

`node web/emu/emu_test.mjs build/emu/felucca.wasm` passed, including five new
DRONE preset tests (six-second held MIDI note, eight-second release, nonzero
bounded audio then silence). Final wasm size: 842385 bytes. These tests ran in
Node, not a manually auditioned browser session. Browser audio load/latency is
not evidence of physical FM-1 performance.
