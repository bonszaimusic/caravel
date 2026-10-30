#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Add current help links to a generated installer; never touch firmware files."""
import argparse
import hashlib
from pathlib import Path

FAQ_URL = "https://github.com/bonszaimusic/caravel/blob/main/docs/faq.md"
LINKS = (
    ('<a href="https://github.com/bonszaimusic/caravel" target="_blank" rel="noopener">DOCS</a>',
     f'<a href="{FAQ_URL}" target="_blank" rel="noopener">FAQ</a>\n    '),
    ('      <p id="status"></p>',
     f'\n      <p class="small" lang="en">Connection or update problem? <a href="{FAQ_URL}" target="_blank" rel="noopener" style="color:inherit">Troubleshooting &amp; FAQ</a></p>'),
    ('        <p class="small" data-t="back"></p>',
     f'\n        <p class="small" lang="en"><a href="{FAQ_URL}#stock-return-error--5" target="_blank" rel="noopener" style="color:inherit">Update Failed -5? Read the stock-return FAQ.</a></p>'),
)


def add_links(text):
    # Templates in a later release may already contain all three links.
    counts = [text.count(link) for _, link in LINKS]
    if counts == [1, 1, 1] and text.count(FAQ_URL) == 3:
        return text
    if any(counts) or FAQ_URL in text:
        raise ValueError("Partial or duplicate FAQ links in installer")
    for marker, _ in LINKS:
        if text.count(marker) != 1:
            raise ValueError("Installer FAQ insertion marker missing or duplicated")
    for i, (marker, link) in enumerate(LINKS):
        text = text.replace(marker, link + marker if i == 0 else marker + link, 1)
    return text


def firmware_hashes(site):
    return {p.relative_to(site).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (site / "firmware").rglob("*") if p.is_file()}


def apply(site):
    site = Path(site)
    path = site / "webapp/installer/index.html"
    if path.is_symlink() or not path.is_file() or not (site / "firmware").is_dir():
        raise ValueError("Expected a generated site with an installer and firmware directory")
    before = firmware_hashes(site)
    updated = add_links(path.read_text(encoding="utf-8"))
    path.write_text(updated, encoding="utf-8")
    if firmware_hashes(site) != before:
        raise ValueError("Firmware artifacts changed while adding FAQ links")
    print("Installer FAQ links ready; firmware artifact hashes unchanged")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", type=Path, required=True)
    apply(parser.parse_args().site)
