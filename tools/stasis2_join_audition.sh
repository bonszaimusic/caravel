#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# PRIVATE A/B listening preparation from the actual host STASIS FX pipeline.
# No release, target build, hardware timing claim or default-join activation.
# Requires a full checkout with the generated tables (build/gen).
set -eu
cd "$(dirname "$0")/.."
test -f build/gen/felucca_tables.h || {
  echo 'Missing build/gen: generate the tables in a full checkout first' >&2
  exit 2
}
command -v cc >/dev/null 2>&1 || { echo 'No host C compiler' >&2; exit 2; }
command -v python3 >/dev/null 2>&1 || { echo 'No Python3' >&2; exit 2; }
out='build/stasis-v5/join-audition'
mkdir -p "$out"
# Keep all A/B variants at exactly the same test stimulus gain.
input_div=${STASIS_AB_INPUT_DIV:-16}
case "$input_div" in
  8|16|32|64|128) ;;
  *) echo 'STASIS_AB_INPUT_DIV must be one of 8,16,32,64,128' >&2; exit 2 ;;
esac
# Never leave a prior run's "listen" output behind if the new raw A/B
# is rejected for clipping; all files here are generated probe artifacts.
rm -f "$out/analysis.json" "$out/"*-listen.wav
head_sha=$(git rev-parse HEAD)
printf 'STASIS audition stimulus divisor %s (all modes)\n' "$input_div"
printf 'STASIS2 join audition source HEAD %s\n' "$head_sha"
# Match the A4 host's source flags, with both builds differing ONLY in join.
CFLAGS="-O2 -w -ffp-contract=off -Ibuild/gen -Ifirmware/src -DCARAVEL_REVERB_MINIMAL=1 -DCARAVEL_STASIS_EXPAND=1 -DCARAVEL_STASIS2=1 -DFELUCCA_SLICE=0 -DSTASIS_AB_INPUT_DIV=$input_div"
# shellcheck disable=SC2086
cc $CFLAGS tests/stasis_join_audition_test.c \
  -o "$out/fx-nojoin" -lm
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_STASIS_SOURCE_JOIN_EXPERIMENT=1 \
  tests/stasis_join_audition_test.c -o "$out/fx-join" -lm
# Third candidate: existing cubic curve replaced with SAME 257-entry grain
# envelope LUT; no new PCM memory and no change in standard firmware.
# shellcheck disable=SC2086
cc $CFLAGS -DCARAVEL_STASIS_SOURCE_JOIN_EXPERIMENT=1 \
  -DCARAVEL_STASIS_SOURCE_JOIN_LUT_EXPERIMENT=1 \
  tests/stasis_join_audition_test.c -o "$out/fx-join-lut" -lm
# All WAVs are 12-second real host FX renders with the SAME source/flags.
# All source histories are deterministic: 2s LIVE, 6.14s HOLD, resume.
for entry in 'hq 0' 'long 1' 'drone 2'; do
  set -- $entry
  name=$1
  quality=$2
  "$out/fx-nojoin" "$out/$name-default.wav" "$quality"
  "$out/fx-join" "$out/$name-joined.wav" "$quality"
  "$out/fx-join-lut" "$out/$name-lut.wav" "$quality"
done
python3 tools/stasis2_join_audio_compare.py "$out"
printf '%s\n' 'PRIVATE unclipped audio audition prepared (NOT audible acceptance).'
printf '%s\n' 'Listen in headphones to the *-listen.wav pairs at matched gain.'
printf '%s\n' 'HOLD ends near 8.143s; audit the following ~0.5–1s and musical ambience.'
printf '%s\n' 'LONG and DRONE must remain WAV-identical across all 3 modes (HQ-only join).'
printf 'Source code SHA: %s\n' "$head_sha"
