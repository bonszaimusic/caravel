#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Offline ELASTIC P1.4 preview builder. DOES NOT flash or publish firmware.
set -euo pipefail
cd "$(dirname "$0")/.."
for tool in emcc node cc python3; do
    command -v "$tool" >/dev/null || { echo "BLOCKED: missing $tool" >&2; exit 2; }
done
[ -f build/gen/felucca_tables.h ] || {
    echo "BLOCKED: build/gen missing. Use the documented local target/SDK build first." >&2
    exit 2
}
mkdir -p build/emu
export EMU_CFLAGS="-DCARAVEL_ELASTIC_P1=1 -DCARAVEL_ELASTIC_PLAYABLE=1 -DFELUCCA_RANDOM_MORPH=1 -DCARAVEL_STASIS_EXPAND=1 -DCARAVEL_REVERB_MINIMAL=1 -DFELUCCA_SLICE=0"
export EMU_VERSION="${EMU_VERSION:-v0.13.0-rc1}"
echo "Building private ELASTIC P1.4 WASM (no GitHub Actions)..."
sh web/emu/build.sh
echo "Compiling native parity sample from same feature flags..."
cc -O2 -ffp-contract=off -w \
  -DFELUCCA_HOME_PERF=1 -DFELUCCA_PAIR_MORPH=1 -DFELUCCA_PERF_HW=1 \
  -DCARAVEL_ACTION_BANKS=1 -DFELUCCA_CPU2=0 \
  -DCARAVEL_ELASTIC_P1=1 -DCARAVEL_ELASTIC_PLAYABLE=1 \
  -DFELUCCA_RANDOM_MORPH=1 -DCARAVEL_STASIS_EXPAND=1 -DCARAVEL_REVERB_MINIMAL=1 \
  -Ibuild/gen -Ifirmware/src web/emu/native_check.c \
  -o build/emu/native_check -lm
build/emu/native_check build/emu/native-p12.f32
node web/emu/emu_test.mjs build/emu/felucca.wasm build/emu/native-p12.f32
node web/emu/elastic_p12_smoke.mjs build/emu/felucca.wasm
node web/emu/elastic_p12_track_settings.mjs build/emu/felucca.wasm
node web/emu/elastic_p13_smoke.mjs build/emu/felucca.wasm
echo "Building focused native tests..."
cc -std=c11 -O1 -Wall -Wextra -Werror \
  tests/home_elastic_core_test.c -o build/emu/home_elastic_core_test
build/emu/home_elastic_core_test
cc -std=c11 -O1 -Wall -Wextra -Werror \
  tests/home_elastic_trajectory_test.c -o build/emu/home_elastic_trajectory_test
build/emu/home_elastic_trajectory_test
cc -std=c11 -O1 -Wall -Wextra -Werror \
  tests/home_elastic_p13_test.c -o build/emu/home_elastic_p13_test
build/emu/home_elastic_p13_test
cc -std=c11 -O1 -Wall -Wextra -Werror \
  tests/home_elastic_row_test.c -o build/emu/home_elastic_row_test
build/emu/home_elastic_row_test
cc -std=c11 -O1 -Wall -Wextra -Werror \
  tests/home_elastic_visual_test.c -o build/emu/home_elastic_visual_test
build/emu/home_elastic_visual_test
cc -O2 -w -ffp-contract=off \
  -Ibuild/gen -Ifirmware/src \
  -DCARAVEL_ELASTIC_P1=1 -DCARAVEL_ELASTIC_PLAYABLE=1 \
  -DCARAVEL_ACTION_BANKS=1 -DFELUCCA_RANDOM_MORPH=1 \
  -DFELUCCA_HOME_PERF=1 -DFELUCCA_PERF_HW=1 -DFELUCCA_SLICE=0 \
  -DCARAVEL_REVERB_MINIMAL=1 \
  experiments/action-banks/elastic_p12_test.c -o build/emu/elastic_p12_test -lm
build/emu/elastic_p12_test
# Package portable browser preview only after tests succeed.
python3 - <<'PY'
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
root=Path("build/emu")
target=Path("build/caravel-elastic-p12-emulator.zip")
readme="""# Caravel 0.13.0-rc1 — PRIVATE ELASTIC P1.4 preview
Status: experimental, no FM-1 hardware approval or firmware installer.
Unzip and open a local web server from this directory, e.g.:
  python3 -m http.server 8765
then open http://localhost:8765/ in a browser supporting AudioWorklet.
HOME twice opens ACTION; BANK forward to ELASTIC; hold BANK for SETUP.
SETUP: K1 SCOPE (CELL/ROW), K2 PULL, K3 RETURN, K4 RANGE.\nSelect T1-T4 first: each track has its OWN saved SETUP values.\nPULL ranges from 16ms to 45s, RETURN from 16ms to 5m or INF.\nIn SETUP, A4 toggles PHYSICS: K1 BOUNCE (1/16..64 BAR per cycle), K2 REFLECT\n(0..100), K3 COUPLE (0..100; OFF by default), K4 SEED (0..255).\nA4 returns to TIMES; outside SETUP, A4 still RESET. A small 4-node\npreview shows the signed seed-driven ROW links (redraws on K4).\nBOUNCE follows tempo with phase continuity; RETURN sets decay or INF.\nProject SAVE/LOAD retains per-track settings in HPM1 v5, never motion.\nIn ROW, A1 arms up to four eligible visible timbre/FX controls of the
selected engine. Pitch, engine selectors, algorithms, sample positions and
transport timing are not ELASTIC targets in this pilot. Rotate K1..K4 to
excite; A2 PULSE targets last-touched, A3 HOLD/RESUME, A4 RESET.
In CELL, A1 then turn an eligible knob to pick it. If a running MOTION/step
lock owns an armed parameter, M means its audible value takes priority.
Existing firmware/project preset values remain unchanged in this experiment.
Never use offline tests as evidence of FM-1 dual-core hardware safety.
"""
files=["index.html","worklet.js","felucca.wasm","fonts/DotGothic16-subset.woff","fonts/OFL.txt"]
for name in files:
    if not (root/name).is_file():
        raise SystemExit("missing emulator artifact: "+name)
with ZipFile(target, "w", ZIP_DEFLATED) as zip:
    for name in files:
        zip.write(root/name,name)
    zip.writestr("README.md",readme)
    if (root/"elastic-p12-screen.ppm").is_file():
        zip.write(root/"elastic-p12-screen.ppm","elastic-p12-screen.ppm")
print("PRIVATE preview:",target,target.stat().st_size,"bytes")
PY
echo "PASS: offline emulator tests; package build/caravel-elastic-p12-emulator.zip"
