#!/bin/sh
# Run from this worktree root; build/gen must already be generated.
set -eu
mkdir -p build/host
"${CC:-cc}" -O2 -w -Ibuild/gen -Ifirmware/src tests/drone_test.c -lm -o build/host/drone_test
if [ "${1:-}" = --demo ]; then mkdir -p build/drone_demo; fi
exec build/host/drone_test "$@"
