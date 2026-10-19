#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run at the worktree root with existing build/gen. OUT can be a temp directory.
# CC may contain flags. SANITIZE=0 skips ASan/UBSan; TSAN=1 opts into thread checks.
# STRICT_UBSAN=1 also runs DSP-only with signed-overflow checking enabled.
set -eu
: "${CC:=cc}"
: "${OUT:=build/host/drone_cells}"
[ -f build/gen/felucca_tables.h ] || { echo "run ./build.sh first (build/gen missing)"; exit 1; }
mkdir -p "$OUT"
fail=0
run() { echo "== $1"; shift; "$@" || { rc=$?; echo "FAILED (exit $rc)"; fail=1; }; }
$CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/drone_cells_test" tests/drone_cells_test.c -lm
run "DRONE cells native" "$OUT/drone_cells_test"
$CC -O2 -w -DC1_HOST -pthread -Ibuild/gen -Ifirmware/src -o "$OUT/drone_cells_c1_test" tests/drone_cells_test.c -lm
run "DRONE cells CPU2: four cells tracks + mixed tracks, 16 masks, output/peaks/state parity" "$OUT/drone_cells_c1_test"
if [ "${SANITIZE:-1}" != 0 ]; then
    # Same exclusions as run_tests.sh: intentional DSP wraps and flat-flash XIP
    # pointer rebasing are outside the host C model. Never suppress other checks.
    $CC -O1 -g -w -fsanitize=address,undefined \
        -fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow \
        -fno-sanitize-recover=undefined -Ibuild/gen -Ifirmware/src \
        -o "$OUT/drone_cells_san" tests/drone_cells_test.c -lm
    run "DRONE cells ASan/UBSan (standard suite exclusions)" \
        env ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$OUT/drone_cells_san"
fi
if [ "${STRICT_UBSAN:-0}" = 1 ]; then
    # Shift checking alone is excluded for existing fixed-point helpers; unlike
    # the standard suite this retains signed-overflow, bounds and pointer checks.
    $CC -O1 -g -w -fsanitize=undefined -fno-sanitize=shift -fno-sanitize-recover=all \
        -Ibuild/gen -Ifirmware/src -o "$OUT/drone_cells_strict" tests/drone_cells_test.c -lm
    run "DRONE cells strict UBSan, DSP-only" "$OUT/drone_cells_strict" --dsp-only
fi
if [ "${TSAN:-0}" = 1 ]; then
    $CC -O1 -g -w -DC1_HOST -pthread -fsanitize=thread -Ibuild/gen -Ifirmware/src \
        -o "$OUT/drone_cells_tsan" tests/drone_cells_test.c -lm
    run "DRONE cells CPU2 ThreadSanitizer" env TSAN_OPTIONS=halt_on_error=1 "$OUT/drone_cells_tsan"
fi
[ "$fail" = 0 ] && echo "DRONE CELLS TESTS PASSED" || exit 1
