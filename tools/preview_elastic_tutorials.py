#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Render a local, unpublished preview of the 0.13.0 tutorial section.

No firmware build, hardware, audio synthesis, GitHub Actions or public Pages
deployment is involved. Existing 0.6.0 STASIS and Random/Journey tutorials
stay available in the preview. Concept SVGs are marked as illustrations,
not as 0.13.0 firmware screenshots.

  python3 tools/preview_elastic_tutorials.py
  python3 -m http.server 8790 -d build/elastic-tutorial-preview
  # then open http://localhost:8790/tutorials/
"""
from pathlib import Path
import argparse
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "web"))

import make_site  # noqa: E402
import stasis_pages  # noqa: E402
import tutorial_pages  # noqa: E402


def build(target):
    data = tutorial_pages.check_source(ROOT)
    expected = tutorial_pages.readme_text(data)
    if (ROOT / "docs/tutorials/README.md").read_text(encoding="utf-8") != expected:
        raise ValueError("Outdated generated README; run web/tutorial_pages.py --write-readme")
    target.mkdir(parents=True, exist_ok=True)
    look = make_site.look_css((ROOT / "web/index_pkg.html").read_text(encoding="utf-8"))
    tutorial_pages.add_tutorials(target, look, make_site.SHIP_PATH, "0.13.0-rc1", ROOT)
    stasis_pages.add_morph_tutorial(target, look, make_site.SHIP_PATH, ROOT, "0.13.0-rc1")
    stasis_pages.add_lessons_page(target, look, make_site.SHIP_PATH, ROOT, "0.13.0-rc1")
    # Only the tutorial section is generated. No installer/emulator/release assets.
    landing = ('<!doctype html><html lang="en"><meta charset="utf-8">'
               '<meta name="viewport" content="width=device-width, initial-scale=1">'
               '<title>Caravel tutorials — local preview</title>'
               '<p>Local 0.13.0 ELASTIC tutorial preview, not a firmware release.</p>'
               '<p><a href="tutorials/">Open tutorial overview</a></p></html>')
    (target / "index.html").write_text(landing, encoding="utf-8")
    tutorial_pages.check_site(target, ROOT)
    print("Local tutorial preview:", target / "tutorials/index.html")
    print("Not published. ELASTIC screenshots/audio still require a validated renderer build.")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output", type=Path,
                    default=ROOT / "build/elastic-tutorial-preview",
                    help="Destination directory for a local, unpublished HTML preview")
    args = ap.parse_args()
    build(args.output.resolve())
