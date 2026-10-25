#!/usr/bin/env python3
"""Private opt-in D3 HOME/mute build. Starts with core 2 OFF; use SYSTEM > 2ND CORE + reboot."""
import os,subprocess,sys
from pathlib import Path
r=Path(__file__).resolve().parents[2]
e=dict(os.environ,FELUCCA_CPU2='1',FELUCCA_PAIR_MORPH='1',FELUCCA_HOME_PERF='1',FELUCCA_PERF_HW='1',CARAVEL_DUALCORE_D1='1')
subprocess.run([sys.executable,'tools/build.py',*sys.argv[1:]],cwd=r,env=e,check=True)

subprocess.run([sys.executable,'experiments/dualcore-d1/check_layout.py'],cwd=r,env=e,check=True)
