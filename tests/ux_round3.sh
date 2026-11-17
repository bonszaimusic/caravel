#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
# Native capture regression probes. Needs generated build/gen from ./build.sh.
set -eu
cd "$(dirname "$0")/.."
out=${1:-build/ux-round3}
mkdir -p "$out"
${PYTHON:-python3} - "$out" <<'PYBUILD'
import os, shlex, subprocess, sys
sys.path.insert(0,'tools')
from validate_format2 import PROFILE
exe=sys.argv[1]+'/capture-test'
subprocess.run([*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-O1','-w',
    '-Wno-error=implicit-function-declaration','-ffp-contract=off',
    '-Ibuild/gen','-Ifirmware/src','-Itests',*PROFILE,
    'tests/ux_capture_test.c','-lm','-o',exe],check=True)
subprocess.run([exe],check=True)
PYBUILD
