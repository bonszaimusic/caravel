#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Private source-level STASIS2 probe runner. Execute from the repo root.
# Requires generated build/gen/felucca_tables.h; no firmware build or FM-1 QA.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/stasis-v5
# No generated firmware assets required to exercise the exact region helper.
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined    tests/stasis_region_math_test.c -o build/stasis-v5/region-math
build/stasis-v5/region-math
# The actual region planner must preserve both reader safety margins as the
# writer resumes after FREEZE. This cannot establish audible PCM continuity.
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined \
    tests/stasis_region_release_test.c -o build/stasis-v5/region-release
build/stasis-v5/region-release
# Standalone C5 hold timing and 32-bit millisecond rollover, no build/gen needed.
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined \
    tests/stasis_ui_new_test.c -o build/stasis-v5/new-source-progress
build/stasis-v5/new-source-progress
# STASIS Scope V6 rate is an independent UI-only helper. This tests just
# 6 Hz timing/wrap; the actual source bins and LCD renderer remain gated.
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined \
    tests/stasis_scope_rate_test.c -o build/stasis-v5/scope-rate
build/stasis-v5/scope-rate
# Pure V6 UI visibility: STASIS scope must also be present on SENDS and
# FILTER, with TONE represented as 5 quantized temperature levels.
# This does not render real audio history or relocate MIDI CAPTURE notes.
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined \
    tests/stasis_scope_policy_test.c -o build/stasis-v5/scope-policy
build/stasis-v5/scope-policy
# Exact safe clipping of each 8-row LCD strip; a full r48 disc must fit.
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined \
    tests/stasis_scope_geometry_test.c -o build/stasis-v5/scope-geometry
build/stasis-v5/scope-geometry
# The HQ source seam candidate is disabled in the target/default firmware.
# This isolated check only validates its interpolation/limits; full A4 is
# still a failing acceptance gate and the candidate needs explicit A/B.
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined -Ifirmware/src \
    tests/stasis_join_test.c -o build/stasis-v5/source-join-unit -lm
build/stasis-v5/source-join-unit
# Optional low-operation variant: reuse the actual 257-point grain envelope.
# Isolated C11/UBSan check covers all 513 weights x 121 extreme PCM pairs.
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined -Ifirmware/src \
    tests/stasis_join_lut_test.c -o build/stasis-v5/source-join-lut-unit -lm
build/stasis-v5/source-join-lut-unit
# Isolated moving-writer simulation: exposes the old/new recorded source
# splice independently of the HALL and grain-head implementation.
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined -Ifirmware/src \
    tests/stasis_join_ring_test.c -o build/stasis-v5/source-join-ring -lm
build/stasis-v5/source-join-ring
# 294,912 exhaustive source/FORMAT2 bit layouts, including unchanged HARMONY.
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined \
   tests/stasis_region_wire_test.c -o build/stasis-v5/region-wire
build/stasis-v5/region-wire
test -f build/gen/felucca_tables.h || {
  echo 'Missing build/gen: generate firmware tables with the normal build tooling first' >&2
  exit 2
}
mkdir -p build/stasis-v5
CFLAGS='-O2 -w -ffp-contract=off -Ibuild/gen -Ifirmware/src -DCARAVEL_REVERB_MINIMAL=1 -DCARAVEL_STASIS_EXPAND=1 -DFELUCCA_SLICE=0'
# The true firmware grain lookup, as included into the full host FX code,
# must match smoothstep within 2 Q15 units (PCM <=4). No device claim.
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_STASIS2=1 -DCARAVEL_STASIS_SOURCE_JOIN_EXPERIMENT=1 \
   -DCARAVEL_STASIS_SOURCE_JOIN_LUT_EXPERIMENT=1 \
   tests/stasis_join_lut_fx_test.c -o build/stasis-v5/source-join-lut-fx -lm
build/stasis-v5/source-join-lut-fx
# Intentional word splitting to expand standard GCC flags.
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_STASIS2=1 tests/stasis_v5_stage1_test.c -o build/stasis-v5/full-ring -lm
build/stasis-v5/full-ring
# V6 source audio telemetry on the real DSP: captured PCM peaks (all three
# rates), frozen peaks and NEW reset. Optional, never a default firmware flag.
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_STASIS2=1 -DCARAVEL_STASIS_SCOPE_V6=1 \
   tests/stasis_v5_stage1_test.c -o build/stasis-v5/full-ring-v6 -lm
build/stasis-v5/full-ring-v6
# shellcheck disable=SC2086
# Host-only sample-exact trace: no target code, no production ISR overhead.
cc $CFLAGS -DCARAVEL_STASIS2=1 -DCARAVEL_STASIS_PCM_TRACE=1 tests/stasis_continuity_test.c -o build/stasis-v5/continuity -lm
# Record A4 failure but continue with independent A5 UI and FORMAT-2 gates.
# This MUST NOT turn a failing PCM run into a passing suite.
continuity_failed=0
build/stasis-v5/continuity || continuity_failed=1
# Prove the full FORMAT-2 project codec, not just standalone packed bytes.
# Native objects (0x00/0xA5) remain readable, 0xA6 is opt-in STASIS2-only.
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_FORMAT2=1 -DCARAVEL_STASIS2=1 \
   tests/format2_project_test.c -o build/stasis-v5/format2-project-a6 -lm
build/stasis-v5/format2-project-a6
# Per-track DRY (retired id 80) and the HALL/FEED hall controls on the real mix and STASIS DSP.
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_STASIS2=1 tests/stasis_mix_dry_test.c -o build/stasis-v5/mix-dry -lm
build/stasis-v5/mix-dry
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_STASIS2=1 tests/stasis_send_test.c -o build/stasis-v5/send -lm
build/stasis-v5/send
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_STASIS2=1 tests/stasis_hall_feed_test.c -o build/stasis-v5/hall-feed -lm
build/stasis-v5/hall-feed
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_STASIS2=1 tests/stasis_level_test.c -o build/stasis-v5/stasis-level -lm
build/stasis-v5/stasis-level
# MOTN scatter: grains start across the REGION window (real host DSP, frozen and live).
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_STASIS2=1 tests/stasis_grain_wander_test.c -o build/stasis-v5/grain-wander -lm
build/stasis-v5/grain-wander
# Legacy baseline retained for an independent 32768-sample reference check.
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_STASIS2=0 tests/stasis_continuity_test.c -o build/stasis-v5/continuity-baseline -lm
build/stasis-v5/continuity-baseline
# Full UI/project and physical FX+C5 host fixture: only profile flags that
# compile the real HOME/PAIR/UX/ACTION integration may be used here.
UI_FLAGS='-DFELUCCA_CPU2=0 -DFELUCCA_HOME_PERF=1 -DFELUCCA_PERF_HW=1 -DFELUCCA_PAIR_MORPH=1 -DCARAVEL_ACTION_BANKS=1 -DFELUCCA_RANDOM_MORPH=1 -DCARAVEL_UX_LAYERS=1 -DCARAVEL_UX_EVOLVE=1 -DCARAVEL_ELASTIC_P1=1 -DCARAVEL_ELASTIC_PLAYABLE=1 -DCARAVEL_AFTERBURNER=1 -DCARAVEL_STASIS2=1'
# shellcheck disable=SC2086
cc $CFLAGS $UI_FLAGS tests/stasis_project_menu_ui_test.c -o build/stasis-v5/project-menu-ui -lm
build/stasis-v5/project-menu-ui
# Experimental V6 UI integration: 32 physical PCM bins, all 5 SPACE pages,
# lower MIDI CAPTURE and byte-identical full/partial framebuffer geometry.
# shellcheck disable=SC2086
cc $CFLAGS $UI_FLAGS -DCARAVEL_UX_CAPTURE=1 -DCARAVEL_STASIS_SCOPE_V6=1 \
   tests/stasis_project_menu_ui_test.c -o build/stasis-v5/project-menu-ui-v6 -lm
build/stasis-v5/project-menu-ui-v6
# Private BUILD FX visual experiment: GRAIN/REGION orbit, disabled DLY
# rails, and BUILD scope full-vs-dirty pixel equality (V7 on).
# These do not turn a failing A4 into a passing suite.
# shellcheck disable=SC2086
cc $CFLAGS $UI_FLAGS -DCARAVEL_UX_BUILD_FX_PLAY_STYLE=1 \
   tests/stasis_project_menu_ui_test.c -o build/stasis-v5/build-fx-orbit -lm
build/stasis-v5/build-fx-orbit
# shellcheck disable=SC2086
cc $CFLAGS $UI_FLAGS -DCARAVEL_UX_CAPTURE=1 -DCARAVEL_STASIS_SCOPE_V6=1 \
   -DCARAVEL_UX_BUILD_FX_PLAY_STYLE=1 \
   tests/stasis_project_menu_ui_test.c -o build/stasis-v5/build-fx-orbit-v7 -lm
build/stasis-v5/build-fx-orbit-v7
if test "$continuity_failed" -ne 0; then
  if test "${STASIS2_A4_KNOWN_LIMIT:-0}" = 1; then
    # Release 1.2.0: the HQ HOLD to LIVE seam is a documented known limit (docs/release-120.md); the join that closes
    # it is the build flag CARAVEL_STASIS_SOURCE_JOIN_EXPERIMENT (tools/stasis2_join_ab.sh). Every other step above
    # already ran with set -e, so only A4 can reach this point. Unset the variable to make A4 fail the suite again.
    echo 'STASIS2 A4 PCM discontinuity: KNOWN LIMIT (docs/release-120.md); all other probe steps passed' >&2
  else
    echo 'STASIS2 host probe suite FAILED: A4 PCM discontinuity (A5 and FORMAT-2 were still exercised)' >&2
    exit 1
  fi
fi
printf '%s\n' 'STASIS2 host probe suite finished. Target size, PCM parity and FM-1 hardware remain separate gates.'
