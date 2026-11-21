#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Academy overlay for the public site (policy B: the browser files come from main, the firmware stays the release tag's).

Runs in the Pages job AFTER web/make_site.py has verified the base site from the release tag and BEFORE the generation
composition. It does nothing unless main carries web/emu/academy-release.json. Otherwise it

  1. builds the emulator in a separate copy: the tag's firmware sources and generated headers, main's web/emu files,
  2. optionally filters the tutorial data to the released stages (web/emu/academy_filter.mjs), then inlines it into index.html,
  3. checks the result (closed file list, version label, WASM header, worklet equals main's, no data of unreleased stages),
  4. replaces site/emulator and writes a receipt that names both commits.

Fail-closed: any check failure raises SystemExit and the job stops before anything is deployed.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

EMU_FILES = {"index.html", "felucca.wasm", "worklet.js", "fonts/DotGothic16-subset.woff", "fonts/OFL.txt"}
MARKER = "web/emu/academy-release.json"


def fail(message):
    raise SystemExit("academy overlay refused: " + message)


def require(condition, message):
    if not condition:
        fail(message)


def load_marker(main_tree):
    path = Path(main_tree) / MARKER
    if not path.is_file():
        return None
    data = json.loads(path.read_text(encoding="utf-8"))
    released = data.get("released")
    require(isinstance(released, list) and released and all(isinstance(n, int) and 1 <= n <= 10 for n in released)
            and len(set(released)) == len(released), "released must be a non-empty list of unique stage numbers 1..10")
    return sorted(released)


def stage_titles(main_tree):
    """Titles of all ten stages of main's data (they must not leak into the page when the stage is unreleased)."""
    script = ("import(process.argv[1]).then(m=>console.log(JSON.stringify(m.TOUR_AMBIENT_STAGES.map(s=>s.title))))"
              ".catch(e=>{console.error(e);process.exit(1)})")
    out = subprocess.check_output(["node", "-e", script, (Path(main_tree) / "web/emu/tutorial_data.mjs").resolve().as_uri()], text=True)
    titles = json.loads(out)
    require(len(titles) == 10 and all(isinstance(t, str) and t for t in titles), "tutorial data must name ten stages")
    return titles


def commit_of(tree):
    return subprocess.check_output(["git", "-C", str(tree), "rev-parse", "HEAD"], text=True).strip()


def build_emulator(tag_tree, main_tree, version, released, workdir):
    work = Path(workdir) / "academy-work"
    # a separate copy of the tag's tree (the firmware includes more than firmware/ and tests/), without git data and build output;
    # the generated headers come from the job's own build/gen, the browser files from main
    shutil.copytree(tag_tree, work, symlinks=False, ignore=shutil.ignore_patterns(".git", "build", "site"))
    (work / "build").mkdir()
    shutil.copytree(Path(tag_tree) / "build" / "gen", work / "build" / "gen")
    shutil.rmtree(work / "web" / "emu")
    shutil.copytree(Path(main_tree) / "web" / "emu", work / "web" / "emu")
    env = dict(os.environ, EMU_VERSION="v" + version.lower(), EMU_PREVIEW="0")
    if len(released) < 10:
        flt = work / "web" / "emu" / "academy_filter.mjs"
        require(flt.is_file(), "a subset of stages is released but web/emu/academy_filter.mjs is missing: the page would carry unreleased lessons")
    subprocess.run(["sh", str(work / "web" / "emu" / "build.sh")], cwd=work, env=env, check=True)
    emu = work / "build" / "emu"
    flt = work / "web" / "emu" / "academy_filter.mjs"
    if flt.is_file():
        subprocess.run(["node", str(flt), str(work / MARKER), str(emu)], cwd=work, check=True)
    subprocess.run(["node", str(work / "web" / "emu" / "academy_inline.mjs"), str(emu)], cwd=work, check=True)
    return emu


def files_of(directory):
    directory = Path(directory)
    out = set()
    for path in directory.rglob("*"):
        require(not path.is_symlink(), "symlink in emulator output: " + str(path))
        if path.is_file():
            out.add(path.relative_to(directory).as_posix())
    return out


def verify_emulator(emu, version, released, titles, main_worklet):
    emu = Path(emu)
    require(files_of(emu) == EMU_FILES, "emulator output must be exactly " + ", ".join(sorted(EMU_FILES)) + ", got " + ", ".join(sorted(files_of(emu))))
    html = (emu / "index.html").read_text(encoding="utf-8")
    label = '<small class="version">v%s</small>' % version.lower()
    require(label in html and html.count('class="version"') == 1, "emulator version label must be exactly v" + version)
    require("tutorial_data.mjs" not in html, "the data import must be inlined")
    require('id="tour-toggle"' in html, "the Academy button is missing from the page")
    require((emu / "felucca.wasm").read_bytes().startswith(b"\0asm\x01\0\0\0"), "invalid WASM header")
    require((emu / "worklet.js").read_bytes() == Path(main_worklet).read_bytes(), "worklet.js differs from main's")
    for number, title in enumerate(titles, start=1):
        if number not in released:
            require(title not in html, "the page still carries the title of unreleased stage %d: %r" % (number, title))
    for number in released:
        require(titles[number - 1] in html, "released stage %d is missing from the page" % number)


def swap_emulator(site, new_emu):
    site = Path(site)
    old = site / "emulator"
    require(old.is_dir() and not old.is_symlink(), "site/emulator is missing")
    old_html = (old / "index.html").read_text(encoding="utf-8")
    anchor = re.search(r'(<a id="matching-source" href=")([^"]*)(")', old_html)
    require(anchor, "the verified site page has no matching-source link")
    new_html = (Path(new_emu) / "index.html").read_text(encoding="utf-8")
    new_html, count = re.subn(r'(<a id="matching-source" href=")[^"]*(")', lambda m: m[1] + anchor.group(2) + m[2], new_html)
    require(count == 1, "the Academy page must contain one matching-source link")
    shutil.rmtree(old)
    shutil.copytree(new_emu, old)
    (old / "index.html").write_text(new_html, encoding="utf-8")


def write_receipt(site, tag_commit, main_commit, version, released):
    emu = Path(site) / "emulator"
    hashes = {name: hashlib.sha256((emu / name).read_bytes()).hexdigest() for name in sorted(EMU_FILES)}
    (emu / "academy-receipt.json").write_text(json.dumps({
        "kind": "academy-overlay", "version": version, "released_stages": released,
        "firmware_source_commit": tag_commit, "web_files_commit": main_commit, "sha256": hashes,
        "note": "WASM built from the release tag's firmware sources; page, worklet and released tutorial data from the web-files commit."},
        indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag-tree", default=".")
    parser.add_argument("--main-tree", required=True)
    parser.add_argument("--site", required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    released = load_marker(args.main_tree)
    if released is None:
        print("no academy-release.json on main: the emulator stays as built from the release tag")
        return
    titles = stage_titles(args.main_tree)
    with tempfile.TemporaryDirectory() as tmp:
        emu = build_emulator(args.tag_tree, args.main_tree, args.version, released, tmp)
        verify_emulator(emu, args.version, released, titles, Path(args.main_tree) / "web" / "emu" / "worklet.js")
        swap_emulator(args.site, emu)
    write_receipt(args.site, commit_of(args.tag_tree), commit_of(args.main_tree), args.version, released)
    print("Academy overlay applied: stages %s, firmware %s, web files %s" % (released, commit_of(args.tag_tree)[:8], commit_of(args.main_tree)[:8]))


if __name__ == "__main__":
    main()
