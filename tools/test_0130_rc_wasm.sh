#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Test the actual RC ACTION bundle, including seven-bank navigation and v5.
set -euo pipefail
cd "$(dirname "$0")/.."
flags=(-O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src
  -DFELUCCA_HOME_PERF=1 -DFELUCCA_PAIR_MORPH=1 -DFELUCCA_PERF_HW=1
  -DCARAVEL_ACTION_BANKS=1 -DFELUCCA_CPU2=0 -DFELUCCA_SLICE=0
  -DFELUCCA_RANDOM_MORPH=1 -DCARAVEL_STASIS_EXPAND=1
  -DCARAVEL_REVERB_MINIMAL=1 -DCARAVEL_ELASTIC_P1=1 -DCARAVEL_ELASTIC_PLAYABLE=1)
cc "${flags[@]}" web/emu/native_check.c -o build/home-perf/native-rc -lm
build/home-perf/native-rc build/home-perf/native-rc.f32
node web/emu/emu_test.mjs build/home-perf/felucca.wasm build/home-perf/native-rc.f32
for test in elastic_p12_smoke elastic_p12_track_settings elastic_p13_smoke; do
  node "web/emu/$test.mjs" build/home-perf/felucca.wasm
done
for test in action_wasm_test action_setup_wasm_test action_snapshots_wasm_test \
  stasis_granular_wasm_test morph_safety_wasm_test stasis_antialias_audio_wasm_test; do
  node "experiments/action-banks/$test.mjs"
done
node experiments/home-perf/home_wasm_test.mjs
