#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# One local build profile for the private 0.13.0 RC. Never publishes or flashes.
set -euo pipefail
cd "$(dirname "$0")/.."
# Stable 0.13.0 by default; CARAVEL_RC_VERSION=0.13.0-rc1 builds the rc1-labelled package.
export CARAVEL_RC_VERSION="${CARAVEL_RC_VERSION:-0.13.0}"
export FELUCCA_HOME_PERF=1 FELUCCA_PAIR_MORPH=1 FELUCCA_PERF_HW=1
export CARAVEL_ACTION_BANKS=1 FELUCCA_RANDOM_MORPH=1 CARAVEL_STASIS_EXPAND=1
export CARAVEL_ELASTIC_P1=1 CARAVEL_ELASTIC_PLAYABLE=1
export FELUCCA_SLICE=0 CARAVEL_REVERB_MINIMAL=1
case "${1:-help}" in
  generate)
    python3 -c 'import tools.build as b; b.generate()'
    ;;
  layouts)
    # Each invocation regenerates headers. The default build runs last so the
    # original host suite receives the actual matching app/loader/package.
    for profile in onecore noram default; do
      out="build/rc0130/$profile"; mkdir -p "$out"
      case "$profile" in
        onecore) settings=(FELUCCA_CPU2=0 CARAVEL_AUDIO_RAM=1) ;;
        noram) settings=(FELUCCA_CPU2=1 CARAVEL_AUDIO_RAM=0) ;;
        default) settings=(FELUCCA_CPU2=1 CARAVEL_AUDIO_RAM=1) ;;
      esac
      env "${settings[@]}" python3 tools/build.py --binary-version "$CARAVEL_RC_VERSION" 2>&1 | tee "$out/build.log"
      cp build/felucca.bin build/felucca.dis build/felucca.elf "$out/"
      cp build/loader/ota.bin "build/caravel-$CARAVEL_RC_VERSION.fwsc" build/gen/felucca_samples.h "$out/"
    done
    cp "build/caravel-$CARAVEL_RC_VERSION.fwsc" build/caravel.fwsc
    ;;
  package)
    # --release assembles and verifies a LOCAL source/package archive from a
    # clean committed checkout; it has no GitHub-release side effects.
    sh build.sh --release "$CARAVEL_RC_VERSION"
    ;;
  host)
    # The reference host tests intentionally choose their own legacy/full
    # compile flags. Do not inject the reduced target profile into them.
    export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
    export TSAN="${TSAN:-1}" SANITIZE="${SANITIZE:-1}"
    CC="${CC:-cc} -include stdint.h" sh tests/run_tests.sh
    ;;
  elastic)
    bash tools/test_elastic_p14_host.sh
    python3 -B tests/sample_zone_policy_test.py
    bash tools/test_elastic_p12_ownership.sh
    ;;
  emulator)
    export ACTION_EMU_VERSION="v$CARAVEL_RC_VERSION"
    export ACTION_EMU_CFLAGS='-DCARAVEL_ACTION_BANKS=1 -DFELUCCA_RANDOM_MORPH=1 -DCARAVEL_REVERB_MINIMAL=1 -DCARAVEL_STASIS_EXPAND=1 -DFELUCCA_SLICE=0 -DCARAVEL_ELASTIC_P1=1 -DCARAVEL_ELASTIC_PLAYABLE=1'
    python3 experiments/home-perf/build.py
    bash tools/test_0130_rc_wasm.sh
    cp build/home-perf/Caravel-ACTION-BANKS-0.6.0-Android.html "build/Caravel-$CARAVEL_RC_VERSION-Emulator.html"
    ;;
  *) echo 'Usage: bash tools/rc0130.sh {generate|layouts|package|host|elastic|emulator}'; exit 2 ;;
esac
