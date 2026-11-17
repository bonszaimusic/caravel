#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The frozen comparison must reject a deliberately changed input velocity."""
import os,subprocess,sys
p=subprocess.run([sys.argv[1],'check'],env={**os.environ,'CE_DIFF_PERTURB':'1'},stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
assert p.returncode!=0 and 'midi_channels DIFFERS' in p.stdout,p.stdout
print('CHORD EXT differential negative control: changed MIDI velocity detected')
