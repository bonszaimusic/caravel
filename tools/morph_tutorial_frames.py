#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Rebuild the RANDOM MORPH / MORPH JOURNEY tutorial screens (docs/tutorials/img) from the actual firmware renderer.
Run from the repository root: python3 tools/morph_tutorial_frames.py   (needs a C compiler and Pillow)."""
import subprocess,sys
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"build/release-prep";IMG=ROOT/"docs/tutorials/img"
FRAMES={"random-saved":"morph-save","random-morph":"morph-go","random-setup-set":"morph-setup",
        "journey-off":"journey-off","random-delete":"morph-delete","journey-held":"journey-held","journey-setup":"journey-setup","journey-status":"journey-status","journey-play":"journey-play","journey-resting":"journey-wait","journey-reverse":"journey-reverse"}
def main():
    OUT.mkdir(parents=True,exist_ok=True);exe=ROOT/"build/morph-tutorial-frames"
    subprocess.run(["cc","-O2","-w","-ffp-contract=off","-Ibuild/gen","-Ifirmware/src","-DCARAVEL_ACTION_BANKS=1","-DFELUCCA_RANDOM_MORPH=1",
        "-DCARAVEL_REVERB_MINIMAL=1","-DCARAVEL_STASIS_EXPAND=1","-DFELUCCA_SLICE=0","-DFELUCCA_PERF_HW=1",
        "tools/morph_tutorial_frames.c","-o",str(exe),"-lm"],cwd=ROOT,check=True)
    subprocess.run([str(exe)],cwd=ROOT,check=True)
    for src,dst in FRAMES.items():Image.open(OUT/(src+".ppm")).save(IMG/(dst+".png"),optimize=True)
    print("wrote",len(FRAMES),"screens to",IMG)
if __name__=="__main__":sys.exit(main())
