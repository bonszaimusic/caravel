#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Felucca in the browser: the firmware's sources as the host tests build them (web/emu/felucca_web.c), compiled
# to WebAssembly with Emscripten, plus the page. After X0X's web/emu/build.sh (charlesvestal/fm1-x0x, GPL-3.0).
#   web/emu/build.sh   ->  build/emu/{index.html, worklet.js, felucca.wasm, fonts/}
# Run after one ./build.sh (build/gen: the tables, the fonts, the CC0 samples).
# -ffp-contract=off: no fused multiply-adds, as the host build. EMU_OPT replaces -O2, EMU_CFLAGS adds flags
# (EMU_CFLAGS=-msimd128: let the compiler vectorise; every browser with AudioWorklet has wasm SIMD).
# FORMAT 2, six engines and BUILD / PLAY use the target profile below.
# CAPTURE is enabled by default; EMU_CFLAGS can explicitly override it with
# -DCARAVEL_UX_CAPTURE=0 for a comparison build.
# The page shows the version (FELUCCA_VERSION) marked "(preview)" before its release; EMU_PREVIEW=0 drops the mark.
set -e
cd "$(dirname "$0")/../.."
[ -f build/gen/felucca_tables.h ] || { echo "emu: run ./build.sh once first (build/gen)"; exit 1; }
command -v emcc >/dev/null 2>&1 || { echo "emu: no emcc (Emscripten)"; exit 1; }
VERSION=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c | head -n 1)
# Optional experimental version label; does not change the firmware source.
if [ -n "${EMU_VERSION:-}" ]; then VERSION="${EMU_VERSION}"; fi
mkdir -p build/emu/fonts
# shellcheck disable=SC2086
emcc -DCARAVEL_UX_CAPTURE=1 -DCARAVEL_FORMAT2=1 -DCARAVEL_SLIM=1 -DCARAVEL_UX_LAYERS=1 -DCARAVEL_UX_EVOLVE=1 -DCARAVEL_ELASTIC_P1=1 -DCARAVEL_ELASTIC_PLAYABLE=1 -DFELUCCA_RANDOM_MORPH=1 -DCARAVEL_REVERB_MINIMAL=1 -DCARAVEL_STASIS_EXPAND=1 -DFELUCCA_HOME_PERF=1 -DFELUCCA_PAIR_MORPH=1 -DFELUCCA_PERF_HW=1 -DCARAVEL_ACTION_BANKS=1 -DFELUCCA_CPU2=0 ${EMU_OPT:--O2} -ffp-contract=off -std=gnu99 -w ${EMU_CFLAGS:-} \
    "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o build/emu/felucca.wasm web/emu/felucca_web.c
[ "${EMU_PREVIEW:-1}" = 0 ] && PREVIEW='' || PREVIEW=' (preview)'
sed -e "s/@VERSION@/$VERSION/g" -e "s/@PREVIEW@/$PREVIEW/g" web/emu/index.html > build/emu/index.html
cp web/emu/worklet.js build/emu/
# The Academy data in this repository is the public bundle (released stages only, see academy-release.json);
# the build only copies it next to the page. The overlay step of the Pages job inlines it.
cp web/emu/tutorial_data.mjs build/emu/
cp web/emu/fonts/DotGothic16-subset.woff web/emu/fonts/OFL.txt build/emu/fonts/
echo "emu: build/emu ($(wc -c < build/emu/felucca.wasm | tr -d ' ') B wasm, $VERSION)"
