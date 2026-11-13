#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Opt-in offline ownership/CPU2 gate, no hosted CI or firmware publication.
set -euo pipefail
cd "$(dirname "$0")/.."
test -f build/gen/felucca_tables.h || {
    echo "BLOCKED: build/gen/felucca_tables.h missing (prepare local SDK target tables)" >&2
    exit 2
}
CC="${CC:-cc}"
command -v "$CC" >/dev/null || { echo "BLOCKED: no C compiler" >&2; exit 2; }
mkdir -p build/host/elastic-p12-ownership
flags=(
    -Ibuild/gen -Ifirmware/src -ffp-contract=off
    -DCARAVEL_ELASTIC_P1=1 -DCARAVEL_ELASTIC_PLAYABLE=1
    -DCARAVEL_ACTION_BANKS=1 -DFELUCCA_RANDOM_MORPH=1
    -DFELUCCA_HOME_PERF=1 -DFELUCCA_PERF_HW=1
    -DFELUCCA_SLICE=0 -DCARAVEL_REVERB_MINIMAL=1 -DFELUCCA_PAIR_MORPH=1 -DCARAVEL_DUALCORE_D1=1
    -DCARAVEL_STASIS_EXPAND=1
)
echo "COMMIT $(git rev-parse HEAD)"
echo "== ELASTIC P1.2: actual MOTION lock, play priority, stable SAVE, parameter policy"
"$CC" -O2 -w "${flags[@]}" experiments/action-banks/elastic_p12_test.c \
    -o build/host/elastic-p12-ownership/elastic_p12_test -lm
build/host/elastic-p12-ownership/elastic_p12_test

echo "== ELASTIC P1.2: 4 tracks x 2 render overlays, all CPU scheduling splits"
"$CC" -O2 -w -pthread -DELASTIC_DUALCORE_TEST=1 "${flags[@]}" \
    tests/dualcore_test.c -o build/host/elastic-p12-ownership/dualcore_test -lm
SCENE="SEM DRUM-X FM6 ANALOG" build/host/elastic-p12-ownership/dualcore_test

if [[ "${ELASTIC_TSAN:-0}" == 1 ]]; then
    echo "== ELASTIC P1.2: same dualcore parity under ThreadSanitizer"
    "$CC" -O1 -g -w -fsanitize=thread -pthread -DELASTIC_DUALCORE_TEST=1 \
        "${flags[@]}" tests/dualcore_test.c \
        -o build/host/elastic-p12-ownership/dualcore_tsan -lm
    TSAN_OPTIONS=halt_on_error=1 SCENE="SEM DRUM-X FM6 ANALOG" \
        build/host/elastic-p12-ownership/dualcore_tsan
fi
echo "PASS: offline functional priority, snapshot-after-barrier and split parity"
echo "NOT PROVEN: live FM-1 interrupt deadlines, CPU1 worker vs concurrent main SAVE, acoustic clicks"
