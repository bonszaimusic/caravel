<!-- SPDX-License-Identifier: GPL-3.0-only -->
# FORMAT 2 validation

FORMAT 2 is the supported 1.0 basis. The production
build has six engines and a complete HPM2 v6 project. Disabling FORMAT 2 or SLIM
is rejected by `firmware/src/core.h:66`. This branch does not rebuild the old
firmware as a supported build profile.

`tools/validate_all.py:1` is the official entry point. It delegates to the explicit
registry in `tools/validate_format2.py:1`. `tests/run_tests.sh:1` invokes the same
eight shards. `.github/workflows/validate-format2.yml:1` has a manual validation
workflow and no release/publish job. `tools/docker_validate.sh:47` runs its actual
prepare/host steps locally; use a normal clone because a worktree's `.git` file
may point outside the container.

```sh
python3 tools/validate_all.py inventory
python3 tools/validate_all.py ui_storage
tools/docker_validate.sh host all
python3 tools/validate_all.py report
```

The inventory rejects any root test absent from both the registry and the exact
historical-fixture manifest (`tests/format2_legacy_tests.json:1`). That manifest
names removed-engine/old-format fixtures and their replacement coverage. It is
not a claim that all historical assertions still apply to FORMAT 2. Live-source,
CAPTURE/MIDI and first-boot tests are registered, including ASan/UBSan. The web
shard runs framebuffer color, frame-strip stability and four-loop density checks.
Tutorial sources, local links and the generated index are checked there too.

`tests/regress.c:1` keeps the retained preset, ANALOG mode/send and slicer hashes
from the unchanged `tests/golden.txt:1`. Exact retired scene names are listed in
`tests/format2_retired_renders.txt:1`; a missing current golden fails. CPU and
target tolerances are unchanged. The host-registry verifies that the golden,
CPU, target and historical union baseline files did not change during a shard.
The static target checker removes functions belonging to physically deleted
engines from its required-function set (`tests/target_budget.py:46`).

The host render fixtures retain the historical ROOM reference for comparable
hashes. Production STASIS and complete HOME/PAIR/ELASTIC are exercised separately
by the STASIS suites and FORMAT 2 project/editor/backup tests. Headroom and
hardware deadlines require actual FM-1 measurements; static loop weights are
not cycle measurements.

Strict ASan/UBSan covers native and complete project codecs, settings, bank and
backup validation, retained UI/MIDI paths, 20,000 USB/editor frames and 5,000
mutated project inputs. `tests/format2_backup_test.c:140` injects every
Erase/Program failure until a complete commit succeeds, then checks the durable
copy after losing retry RAM. `tests/format2_engine_memory_test.c:1` compares all
335 destination-preset/predecessor combinations after zero versus arbitrary
previous engine memory and checks adjacent-track isolation.

The former DRUM stimulus for the speaker EQ is frozen as PCM, with its hash,
license and provenance in `tests/fixtures/SPEAKER_STIMULUS.md:1`. EQ/headroom
assertions remain in `tests/speaker_test.c:65`; the removed DSP is not compiled.
The frozen pre-FORMAT-2 WASM is only an old-data/rollback test fixture; provenance
is in `tests/fixtures/FORMAT1_EMULATOR.md:1`.

On Apple Silicon, x86 ThreadSanitizer cannot run under Docker emulation. The
runner records that limitation rather than claiming a race-detector pass. Linux
also lacks the Mac kernel instruction counter: CPU-budget evidence is reported
separately. The historical preset data subtracts its original idle reference of
286 instructions/sample. FORMAT 2's control-only idle is calibrated separately in
`tests/cpu_format2_idle.txt` (366 on Apple clang 21 arm64), keeping the same +25%
tolerance and the original engine-net-work references. This is a host regression
budget, not a measured FM-1 cycle or interrupt deadline. No audio golden is reset.

CHORD EXT freezes separate FORMAT 2 OFF, SCALE and pre-live HARM references. Its
live-source fuzz runs profiles A, B and D, each 141 seeds × 4000 steps, with CPU2
compiled out and in, ASan/UBSan and CAPTURE enabled. The emulator does not run a
second-core worker thread; actual DSP thread parity is checked by the dual-core
fixtures. Mutation controls must compile successfully and all M1–M11 must fail
their behavioral tests. A failed compile is not counted as detection.
