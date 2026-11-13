#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# P1.4 source-only checks; no SDK or FM-1 required.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${CC:=cc}"
mkdir -p build/host/p14
flags=(-std=c11 -O1 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all)
"$CC" "${flags[@]}" tests/home_elastic_sync_test.c -o build/host/p14/sync-phase
build/host/p14/sync-phase
"$CC" "${flags[@]}" tests/home_elastic_trajectory_test.c -o build/host/p14/trajectory
build/host/p14/trajectory
"$CC" "${flags[@]}" tests/home_elastic_p13_test.c -o build/host/p14/physics
build/host/p14/physics
echo "ELASTIC P1.4 source-only checks passed"
