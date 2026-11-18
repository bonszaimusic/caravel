#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
# Native CAPTURE quantisation regression; requires existing build/gen.
set -eu
cd "$(dirname "$0")/.."
out=${1:-build/capture-post-quant}
mkdir -p "$out"
${PYTHON:-python3} - "$out" <<'PYBUILD'
import os,shlex,subprocess,sys
sys.path.insert(0,'tools')
from validate_format2 import PROFILE
exe=sys.argv[1]+'/capture-quant-test'
cmd=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-O1','-w',
     '-Wno-error=implicit-function-declaration','-ffp-contract=off',
     '-Ibuild/gen','-Ifirmware/src','-Itests',*PROFILE,
     'tests/capture_post_quant_test.c','-lm','-o',exe]
print('RUN:', ' '.join(cmd),flush=True)
subprocess.run(cmd,check=True)
subprocess.run([exe],check=True)
PYBUILD
