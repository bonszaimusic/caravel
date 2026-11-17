#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
set -eu
cd "$(dirname "$0")/.."
out=${1:-build/ux-render}
mkdir -p "$out/ppm"
${CC:-cc} -O1 -w -include stdint.h -DFELUCCA_CPU2=0 \
  -DFELUCCA_HOME_PERF=1 -DFELUCCA_PAIR_MORPH=1 -DFELUCCA_PERF_HW=1 \
  -DCARAVEL_ACTION_BANKS=1 -DCARAVEL_UX_LAYERS=1 -DCARAVEL_UX_CAPTURE=1 \
  -DCARAVEL_STASIS_EXPAND=1 -DCARAVEL_REVERB_MINIMAL=1 \
  -Ibuild/gen -Ifirmware/src -Itests tests/ux_render_test.c -lm -o "$out/render-test"
"$out/render-test" "$out"
