#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Reproduce 1.0 tutorial PNGs from the actual firmware WASM framebuffer.
Usage: python3 tools/tutorial_screens.py [build/emu/felucca.wasm]
"""
import json,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
wasm=sys.argv[1] if len(sys.argv)>1 else 'build/emu/felucca.wasm'
scenes=json.loads((root/'docs/tutorials/screens-1.0.json').read_text())
for name,commands in scenes.items():
 with tempfile.NamedTemporaryFile(mode='w',suffix='.json') as f:
  json.dump(commands,f);f.flush()
  subprocess.run(['node','web/emu/shots.mjs',wasm,'docs/tutorials/img',f.name],cwd=root,check=True)
