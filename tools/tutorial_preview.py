#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""A disposable local preview of the tutorial pages: the real generators (web/performance_pages.py, web/tutorial_pages.py, web/stasis_pages.py)
run on a minimal stand-in site, the way tests/performance_site_test.py builds its fixtures. NOT a release site: no installer, firmware or emulator.

  python3 tools/tutorial_preview.py OUTDIR [--version 0.12.0]
  cd OUTDIR && python3 -m http.server 8000      then open http://localhost:8000/tutorials/
"""
import argparse
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "web"))
sys.path.insert(0, str(ROOT / "tools"))
import make_site
import performance_pages
import stasis_pages


def build(out, version="0.12.0"):
    out = Path(out)
    if out.exists() and any(out.iterdir()):
        raise SystemExit("OUTDIR must be new or empty")
    out.mkdir(parents=True, exist_ok=True)
    template = (ROOT / "web/index_pkg.html").read_text()
    look = make_site.look_css(template)
    source = f"firmware/caravel-{version}-source.tar.gz"
    pages = {"index.html": make_site.landing(look, version, True, source),
             "webapp/installer/index.html": '<style>/*LOOK-BEGIN*/\n' + look + '\n/*LOOK-END*/</style><script type="application/json">{"release_verified": true}</script><p>Preview stand-in for the installer.</p>',
             "webapp/editor/index.html": "<p>Preview stand-in for the editor.</p>", "emulator/index.html": "<p>Preview stand-in for the emulator.</p>"}
    for name in ("tutorials", "sounds", "changelog"):
        pages[name + "/index.html"] = stasis_pages.page("Preview " + name, "<p>Fixture</p>", look, make_site.SHIP_PATH, version=version)
    for name, text in pages.items():
        path = out / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    for name in ("LICENSING.md", "NOTICE", "CREDITS.md", "LICENSE", f"caravel-{version}-source.tar.gz", f"caravel-{version}.fwsc"):
        (out / "firmware").mkdir(exist_ok=True)
        (out / "firmware" / name).write_bytes(b"preview stand-in: " + name.encode())
    performance_pages.build(out)
    print("preview site:", out)
    print("serve it:  cd", out, "&& python3 -m http.server 8000   ->  http://localhost:8000/tutorials/")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", type=Path)
    ap.add_argument("--version", default="0.12.0")
    args = ap.parse_args()
    build(args.out, args.version)
