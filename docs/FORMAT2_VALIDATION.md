<!-- SPDX-License-Identifier: GPL-3.0-only -->
# FORMAT 2 validation

FORMAT 2 is the supported 1.0 basis. The production
build has six engines and a complete HPM2 v6 project. Disabling FORMAT 2 or SLIM
is rejected by `firmware/src/core.h:66`. This branch does not rebuild the old
firmware as a supported build profile.

`tools/validate_all.py:1` is the official entry point. It delegates to the explicit
registry in `tools/validate_format2.py:1`. `tests/run_tests.sh:1` invokes the same
eight shards. `.github/workflows/validate-format2.yml:1` has a manual validation
workflow and no release/publish job. `tools/docker_validate.sh` runs its actual
prepare/host steps locally; use a normal clone because a worktree's `.git` file
may point outside the container.

```sh
python3 tools/validate_all.py inventory
python3 tools/validate_all.py ui_storage
tools/docker_validate.sh host fast     # the gate of each iteration
tools/docker_validate.sh host all      # the full gate of a release candidate
python3 tools/validate_all.py report
```

### Two gates, and what takes the time

`host all` ran about 85 minutes on an Apple-silicon Mac (measured for 1.2.0: preparation 24 s, the seven ordinary shards 78 to 209 s each, the
sanitizer shard 3,933 s). Two things were slow: each shard compiled its tests one after the other, and the CHORD EXT fuzzer ran its 846 runs six at a time.

- A shard now plans first, compiles all its `r.c()` tests `VALIDATE_JOBS` at a time (default: the CPUs, at most 8) and then runs them in their old
  order, so a test's run still sees the state the previous one left. `VALIDATE_JOBS=1` is the old, fully sequential behaviour. Each shard prints its
  slowest steps at the end; the per-step seconds are in `build/validation/<shard>.json`.
- `tools/chordext_fuzz.py` runs `CHORDEXT_FUZZ_WORKERS` runs at a time (default: the CPUs, at most 12), compiles its two variants together and takes
  `CHORDEXT_FUZZ_SEEDS` seeds per profile. `tools/chordext_mutations.py` runs the three controls and the eleven mutants together (`CHORDEXT_MUT_WORKERS`).
- **`host fast`** sets `CHORDEXT_FUZZ_SMOKE=1`: 30 seeds per profile instead of 141 (every profile and both CPU2 variants still run). It is the gate of each
  iteration. **`host all`** keeps all 141 and is the gate of a release candidate, run once on the tree that will be released.
- Target preparation still uses the unchanged `linux/amd64` JieLi image. Host shards and the ACTION/WASM job use the Docker server's native
  architecture (`Dockerfile.native`: gcc/clang, Python, Node 22.11.0, Emscripten 3.1.64, pinned DaisySP). The prepared `build/` tree is shared through a
  Docker volume mounted **read-only** by each shard. All shards use the single source snapshot captured during preparation. Each container gets its own `/work`, fixtures, test outputs and baseline copies. No shard writes
  into another shard's paths. Results are merged only after the containers exit. The full gates run `report` automatically.
- `VALIDATE_SHARDS=3 tools/docker_validate.sh host fast` enables three concurrent isolated shards (default 1). The runner divides Docker's CPU count
  and a conservative 2 GiB/compiler memory allowance across active shards; requested compile/fuzz/mutation worker counts are capped accordingly.
  With parallel shards, the long CPU/TSAN shard starts first and reserves at most two CPU/compiler slots. Ordinary shards, then the sanitizer/fuzz shard,
  share the remaining budget. An atomic control file returns the reserved fuzz workers as soon as CPU/TSAN exits, including during an active fuzz run.
  The fuzzer preserves input/result order and propagates every worker exception. With one shard slot everything stays sequential.
  Parallel compiles have unique log paths even when the test name repeats. Test execution remains sequential inside each shard.
- Native `cpu_dual` runs all three TSAN fixtures. It uses a narrowly extended Docker seccomp profile, described in
  [docker_validate/SECCOMP.md](../tools/docker_validate/SECCOMP.md), to allow TSAN's startup personality call. TSAN failure is fatal.
- Every run has a new directory under `build/docker-validate/`, with `timing.json`, per-container logs, workflow step timings, and merged
  `artifacts/validation/`. Stale earlier results cannot turn a failed run green. `host all` rejects smoke mode or a seed override below 141.
  There is no automatic change-based selection of fuzz coverage.

### Timing budgets and before/after comparison

```sh
VALIDATE_SHARDS=3 tools/docker_validate.sh host fast
VALIDATE_SHARDS=3 tools/docker_validate.sh host all
python3 tools/validation_timing.py build/docker-validate/host-all-RUN
python3 tools/validation_timing.py AFTER_RUN --before BEFORE_RUN
python3 tools/docker_validate/test_runner.py
python3 tools/docker_validate/test_timing.py
python3 tools/docker_validate/test_local_host.py
python3 tools/docker_validate/test_fuzz_workers.py
```

The full local gates check their budgets automatically: fast <=600 s, all <=1200 s (wall time including preparation and report, excluding first-time
image builds). The timing tool prints a Markdown table and the slowest test steps. It fails on incomplete/failed gates, missing or failed fuzz/mutation
results, or excess time. `--before` also compares ordered check names, counts, results, inventory, and new omissions. Compile-step times overlap and must
not be added to infer wall time. Diagnostic single-shard runs have no default time budget. `VALIDATE_HOST_PLATFORM=linux/amd64` is an explicit emulated
baseline on ARM64; it retains the historical TSAN omission and is not the native acceptance gate. There is no automatic emulated fallback.

`VALIDATE_TARGET_IMAGE` and `VALIDATE_HOST_IMAGE` can select already-built local images; the normal defaults derive tags from the Dockerfile content.
`tools/docker_validate.sh image` builds both images ahead of a timed run. Docker Desktop must be running. Image builds use network downloads; gate
execution itself does not depend on GitHub Actions or account billing.

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

On Apple Silicon, the native container runs ThreadSanitizer; the explicitly selected
x86 emulation baseline records its unsupported TSAN limitation. Linux also lacks the Mac kernel instruction counter: CPU-budget evidence is reported
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
