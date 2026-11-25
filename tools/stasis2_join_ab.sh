#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Private A4 HQ experiment. This does NOT enable the candidate in firmware.
# Requires initialized engines and generated build/gen/felucca_tables.h.
set -eu
cd "$(dirname "$0")/.."
test -f build/gen/felucca_tables.h || {
  echo 'Missing build/gen: first generate firmware tables with normal build tooling' >&2
  exit 2
}
mkdir -p build/stasis-v5
CFLAGS='-O2 -w -ffp-contract=off -Ibuild/gen -Ifirmware/src -DCARAVEL_REVERB_MINIMAL=1 -DCARAVEL_STASIS_EXPAND=1 -DFELUCCA_SLICE=0'
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_STASIS2=1 tests/stasis_continuity_test.c \
  -o build/stasis-v5/continuity-nojoin -lm
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_STASIS2=1 -DCARAVEL_STASIS_SOURCE_JOIN_EXPERIMENT=1 \
  -DCARAVEL_STASIS_PCM_TRACE=1 tests/stasis_continuity_test.c \
  -o build/stasis-v5/continuity-join-experiment -lm
# Third private A/B candidate: same join, reuse existing STASIS grain LUT.
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_STASIS2=1 -DCARAVEL_STASIS_SOURCE_JOIN_EXPERIMENT=1 \
  -DCARAVEL_STASIS_SOURCE_JOIN_LUT_EXPERIMENT=1 \
  -DCARAVEL_STASIS_PCM_TRACE=1 tests/stasis_continuity_test.c \
  -o build/stasis-v5/continuity-join-lut -lm

printf '%s\n' 'A4 baseline without source join (expected current HQ failure):'
base_rc=0
build/stasis-v5/continuity-nojoin || base_rc=$?
printf '%s\n' 'A4 experimental HQ source join (not the production acceptance gate):'
join_rc=0
build/stasis-v5/continuity-join-experiment || join_rc=$?
printf '%s\n' 'A4 optional shared-grain-LUT join (performance experiment):'
lut_rc=0
build/stasis-v5/continuity-join-lut || lut_rc=$?
printf 'A4 three-way: baseline exit=%u, cubic exit=%u, shared-LUT exit=%u\n' "$base_rc" "$join_rc" "$lut_rc"
if test "$base_rc" -eq 0; then
  echo 'Baseline changed unexpectedly; verify exact source, flags and previous reviewer evidence' >&2
fi
if test "$join_rc" -ne 0; then
  echo 'HQ join candidate remains invalid: inspect POST/PRE/HIT diagnostics before modifying DSP' >&2
  exit "$join_rc"
fi
if test "$lut_rc" -ne 0; then
  echo 'Shared-LUT join candidate FAILED PCM; do not enable the LUT variant' >&2
  exit "$lut_rc"
fi
echo 'Both opt-in host joins passed PCM (4 grains x 3 histories x 2 reverse x 2 scan = 48). Target ISR, sound and FM-1 still NOT RUN.'
