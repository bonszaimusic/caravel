#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# ELASTIC P1.3: offline local-AGENT acceptance. No GitHub Actions, flash or merge.
set -euo pipefail
cd "$(dirname "$0")/.."
echo "P13-HEAD: $(git rev-parse HEAD)"
echo "== Standalone P1.3 physics, 1..1000 oscillations, bounce walls and coupling"
mkdir -p build/host/p13
"${CC:-cc}" -std=c11 -O1 -Wall -Wextra -Werror \
  tests/home_elastic_p13_test.c -o build/host/p13/home_elastic_p13_test
build/host/p13/home_elastic_p13_test
"${CC:-cc}" -std=c11 -O1 -Wall -Wextra -Werror \
  tests/home_elastic_trajectory_test.c -o build/host/p13/home_elastic_trajectory_test
build/host/p13/home_elastic_trajectory_test
echo "== Real P1.3 HOME, PLAY/MOTION/SAVE ownership and optional 43-scene split test"
bash tools/test_elastic_p12_ownership.sh
if [[ "${P13_TSAN:-0}" == 1 ]]; then
  ELASTIC_TSAN=1 bash tools/test_elastic_p12_ownership.sh
fi
echo "== Exact WASM UI, HPM1 v4 project SAVE/LOAD, native parity and screens"
bash tools/build_elastic_p12_emulator.sh
echo "== Full host suite"
bash tools/docker_validate.sh host all
echo "== All target layouts: inspect EACH profile log for BUILD FAILED"
mkdir -p build/host/p13
if ! EXTRA_ENV="CARAVEL_ELASTIC_P1=1 CARAVEL_ELASTIC_PLAYABLE=1" \
    bash tools/docker_layouts.sh 2>&1 | tee build/host/p13/layouts.log; then
  echo "BLOCKED: layout wrapper failed" >&2;exit 1
fi
if grep -Eiq 'BUILD FAILED|\bFAILED\b|\bERROR\b' build/host/p13/layouts.log; then
  echo "BLOCKED: individual layout reports failure despite wrapper exit 0" >&2
  exit 1
fi
echo "RUN COMPLETED (not final approval): verify default/onecore/noram"
echo "RAM >= 4096 B free in EACH profile; app slot positive; measure new FLASH delta."
echo "Persist: v4 SAVE/LOAD, v1-v3 migration, RANDOM snapshot, autosave & power cut."
echo "Hardware still OPEN: FM-1 audio clicks, CPU2/SAVE, 300s/1000 cycles."
