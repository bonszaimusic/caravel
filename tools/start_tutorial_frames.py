#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Rebuild the START HERE tutorial screens and the overview cards (docs/tutorials/img) from the actual firmware renderer.
Run from the repository root: python3 tools/start_tutorial_frames.py   (needs a C compiler, Pillow, and build/gen from tools/build.py generate;
the Docker image of tools/docker_validate.sh has all of it: tools/tutorial_media_docker.sh)."""
import subprocess,sys
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"build/release-prep";IMG=ROOT/"docs/tutorials/img"
FRAMES=["start-home","start-preset","start-engine-picker","start-engine-picked","start-step","start-step-armed","start-step-notes","start-step-play",
        "start-save-user","start-save-project","start-save-arm","start-save-name","start-saved",
        "card-start","card-basics","card-sem","card-hades","card-drone","card-ensemble","card-seq-arp","card-filter-fx","card-midi","card-help","card-harmony"]
def main():
    OUT.mkdir(parents=True,exist_ok=True);exe=ROOT/"build/start-tutorial-frames"
    subprocess.run(["cc","-O2","-w","-ffp-contract=off","-Ibuild/gen","-Ifirmware/src","-DCARAVEL_ACTION_BANKS=1","-DFELUCCA_RANDOM_MORPH=1",
        "-DCARAVEL_REVERB_MINIMAL=1","-DCARAVEL_STASIS_EXPAND=1","-DFELUCCA_SLICE=0","-DFELUCCA_PERF_HW=1",
        "tools/start_tutorial_frames.c","-o",str(exe),"-lm"],cwd=ROOT,check=True)
    subprocess.run([str(exe)],cwd=ROOT,check=True)
    for name in FRAMES:Image.open(OUT/(name+".ppm")).save(IMG/(name+".png"),optimize=True)
    print("wrote",len(FRAMES),"screens to",IMG)
if __name__=="__main__":sys.exit(main())
