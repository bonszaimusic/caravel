# Performance support and tests

The folder name is retained for source compatibility. Current user documentation
is in [docs/tutorials](../../docs/tutorials/home-and-stasis.md). Build the production profile
with build.sh; run tools/check_050.py for the feature regression gate.

## ELASTIC P1.1 (developer experiment, default OFF)

`CARAVEL_ELASTIC_P1=1` adds a seventh HOME ACTION bank, `ELASTIC`, for
temporary spring modulation of **one continuous per-track parameter**. It is a
control-rate performance overlay: the stored parameter, project, preset and
flash are never written. The bank is absent from the default build.

- **Target**: any HOME cell of the selected track whose descriptor is
  continuous and side-effect free: `F_PCT`, `F_BIPCT`, `F_TIME`, `F_LFOHZ`,
  `F_CUTOFF`, `F_DB`, `F_SEMI`, `F_BPM`, `F_OCT`. Enums/on/off/note/step
  parameters, `F_INT` slots (e.g. FM6 PTCH, SAMPLE SET, PAN/FILT), globals,
  LFO2/DX/Macro/Pair and FM6 are not P1 targets.
- **Choose**: turn the desired K1–K4 knob once, then A1; without a previous
  turn TONE (`P_E7`) is preferred, then the first safe cell. Arming selects the
  target row.
- **Play**: A1 arms (sequencer stopped), the knob excites the spring; A2 adds
  a small upward nudge, A3 toggles HOLD/RESUME, A4 RESET ends at the stored
  base value. A manual or MIDI write to the same parameter, transport start,
  a track/engine change, a sound/project load or a lost target cancels P1.
- **Spring**: a slow, underdamped pendulum returns in both directions. The
  default settles in ~3.6 s; SETUP `FALL` 0..100 ranges ~1.2 s to ~6.9 s.
  `BOUNCE` changes the swing period, `RESPONSE` the impulse, `RANGE` the
  amplitude. All four are volatile and never saved.
- **Audio**: `hp_begin`/`hp_end` publish one aligned 32-bit value per track
  (valid, engine, parameter id, signed delta); each audio core applies it to
  its own render frame and restores the parameter after the block. The applied
  parameter id is saved in the frame, so a UI-side cancel cannot restore the
  wrong parameter.

Build the target with the flag (ACTION banks are on by default in the firmware
amalgamation; host tests define them explicitly):

```
CARAVEL_ELASTIC_P1=1 python3 tools/build.py --binary-version 0.12.0
```

Local checks:

```
gcc -O2 -w -ffp-contract=off -Ibuild/gen -Ifirmware/src \
    -DCARAVEL_ELASTIC_P1=1 -DCARAVEL_ACTION_BANKS=1 -DFELUCCA_RANDOM_MORPH=1 \
    -DFELUCCA_HOME_PERF=1 -DFELUCCA_PERF_HW=1 -DFELUCCA_SLICE=0 \
    -DCARAVEL_REVERB_MINIMAL=1 experiments/action-banks/elastic_p1_test.c \
    -o build/elastic_p1_test -lm
build/elastic_p1_test
```

`tests/run_tests.sh` runs the same integration check (53 checks); the P0
kernel, overlay and controller-shim tests are registered above it. No hardware,
browser/WASM, TSAN or listening claim follows from these host checks.
