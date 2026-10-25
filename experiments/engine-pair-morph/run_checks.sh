#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/../.."
mkdir -p build/pair
cc -O1 -g -w -ffp-contract=off -Ibuild/gen -Ifirmware/src \
  experiments/engine-pair-morph/pair_test.c -lm -o build/pair/pair_test
build/pair/pair_test
cc -O1 -g0 -w -ffp-contract=off -Ibuild/gen -Ifirmware/src \
  experiments/engine-pair-morph/flex_test.c -lm -o build/pair/flex_test
build/pair/flex_test
cc -O1 -g0 -w -ffp-contract=off -Ibuild/gen -Ifirmware/src \
  experiments/engine-pair-morph/usability_test.c -lm -o build/pair/usability_test
build/pair/usability_test
node experiments/engine-pair-morph/knob_drag_test.mjs
node experiments/engine-pair-morph/audio_sweep_test.mjs
node experiments/engine-pair-morph/flex_test.mjs
node experiments/engine-pair-morph/wasm_test.mjs build/engine-pair-morph/felucca.wasm
node experiments/engine-pair-morph/motion_test.mjs
node experiments/engine-pair-morph/worklet_test.mjs
node experiments/engine-pair-morph/offline_start_test.mjs
node web/emu/emu_test.mjs build/engine-pair-morph/felucca.wasm
