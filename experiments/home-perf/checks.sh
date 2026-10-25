#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build/home-perf
python3 tests/target_budget_test.py
cc -O2 -w experiments/home-perf/diag_time_test.c -o build/home-perf/diag_time_test
build/home-perf/diag_time_test
cc -O1 -g -w -ffp-contract=off -Ibuild/gen -Ifirmware/src experiments/home-perf/diag_test.c -lm -o build/home-perf/diag_test
build/home-perf/diag_test
cc -O1 -g -w -ffp-contract=off -Ibuild/gen -Ifirmware/src experiments/home-perf/home_test.c -lm -o build/home-perf/home_test
cc -O1 -g -w -ffp-contract=off -Ibuild/gen -Ifirmware/src experiments/home-perf/usability_test.c -lm -o build/home-perf/usability_test
build/home-perf/usability_test
build/home-perf/home_test
cc -O1 -g -w -ffp-contract=off -Ibuild/gen -Ifirmware/src experiments/home-perf/hardware_test.c -lm -o build/home-perf/hardware_test
build/home-perf/hardware_test
cc -O1 -g -w -ffp-contract=off -Ibuild/gen -Ifirmware/src experiments/home-perf/backup_test.c -lm -o build/home-perf/backup_test
build/home-perf/backup_test
python3 experiments/home-perf/backup_cli_test.py build/home-perf/backup_test
cc -O1 -g -w -DFELUCCA_HOME_PERF=1 -ffp-contract=off -Ibuild/gen -Ifirmware/src experiments/engine-pair-morph/pair_test.c -lm -o build/home-perf/pair_test
build/home-perf/pair_test
cc -O1 -g -w -ffp-contract=off -Ibuild/gen -Ifirmware/src experiments/home-perf/macro_audio_test.c -lm -o build/home-perf/macro_audio_test
build/home-perf/macro_audio_test
cc -O1 -g -w -ffp-contract=off -Ibuild/gen -Ifirmware/src experiments/home-perf/cell_pair_test.c -lm -o build/home-perf/cell_pair_test
build/home-perf/cell_pair_test
cc -O1 -g -w -ffp-contract=off -Ibuild/gen -Ifirmware/src experiments/home-perf/pair_endpoint_test.c -lm -o build/home-perf/pair_endpoint_test
build/home-perf/pair_endpoint_test
node experiments/home-perf/cell_pair_wasm_test.mjs
node experiments/home-perf/home_wasm_test.mjs
node experiments/home-perf/offline_test.mjs
node web/emu/emu_test.mjs build/home-perf/felucca.wasm
node experiments/engine-pair-morph/knob_drag_test.mjs
node experiments/engine-pair-morph/flex_test.mjs
node experiments/engine-pair-morph/audio_sweep_test.mjs
node experiments/engine-pair-morph/wasm_test.mjs build/home-perf/felucca.wasm
