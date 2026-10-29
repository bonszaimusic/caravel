#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""0.5.0 feature gate, in addition to the inherited host suite. No device I/O."""
from pathlib import Path
import os,subprocess,shlex
r=Path(__file__).resolve().parents[1];out=r/'build/release-prep';out.mkdir(parents=True,exist_ok=True);(r/'build/home-perf').mkdir(parents=True,exist_ok=True)
cases=['experiments/home-perf/automation_test.c','experiments/home-perf/home_d3_test.c','experiments/home-perf/home_test.c','experiments/home-perf/hardware_test.c','experiments/home-perf/backup_test.c','experiments/home-perf/diag_test.c','experiments/home-perf/usability_test.c','experiments/home-perf/macro_audio_test.c','experiments/home-perf/cell_pair_test.c','tests/track_mute_test.c','tests/delay_ext_test.c']
for case in cases:
 name=Path(case).stem;exe=out/name
 subprocess.run([*shlex.split(os.environ.get('CC','cc')),'-O2','-w','-ffp-contract=off','-Ibuild/gen','-Ifirmware/src',case,'-lm','-o',str(exe)],cwd=r,check=True)
 with (out/(name+'.log')).open('w') as f:subprocess.run([str(exe)],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True)
 print(name,'PASS',flush=True)
subprocess.run(['python3','experiments/dualcore-d1/run_tests.py'],cwd=r,check=True)
subprocess.run(['python3','experiments/dualcore-d1/check_layout.py'],cwd=r,check=True)
