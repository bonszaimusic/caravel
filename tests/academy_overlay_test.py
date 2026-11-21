#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Tests of the Academy overlay checks (no emulator build, no network)."""
import json
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "web"))
import academy_overlay as ao

TITLES = ["Strings", "CELL AIR", "Envelope", "Motion", "Motif", "Harmony", "Space", "DEEP SUB", "Save", "Live"]


def page(version="1.1.0", titles=("Strings",), link="https://example.test/src", button=True, imports=False):
    body = " ".join(titles)
    return ('<small class="version">v%s</small><a id="matching-source" href="%s">src</a>%s%s<script>%s</script>'
            % (version, link, '<button id="tour-toggle"></button>' if button else "", '<x>import "./tutorial_data.mjs"</x>' if imports else "", body))


class OverlayTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.addCleanup(self.tmp.cleanup); self.root = pathlib.Path(self.tmp.name)

    def emulator(self, name, html, extra=(), drop=()):
        d = self.root / name
        for f in ao.EMU_FILES:
            (d / f).parent.mkdir(parents=True, exist_ok=True)
            (d / f).write_bytes(b"\0asm\x01\0\0\0" if f == "felucca.wasm" else b"w" if f == "worklet.js" else b"f")
        (d / "index.html").write_text(html, encoding="utf-8")
        for f in extra: (d / f).write_text("x")
        for f in drop: (d / f).unlink()
        return d

    def worklet(self):
        w = self.root / "worklet.js"; w.write_bytes(b"w"); return w

    def test_marker(self):
        main = self.root / "main"; (main / "web/emu").mkdir(parents=True)
        self.assertIsNone(ao.load_marker(main))
        (main / ao.MARKER).write_text(json.dumps({"released": [1]})); self.assertEqual(ao.load_marker(main), [1])
        for bad in ({"released": []}, {"released": [11]}, {"released": [1, 1]}, {"released": "1"}, {}):
            (main / ao.MARKER).write_text(json.dumps(bad))
            with self.assertRaises(SystemExit): ao.load_marker(main)

    def test_good_page_passes(self):
        ao.verify_emulator(self.emulator("e", page()), "1.1.0", [1], TITLES, self.worklet())

    def test_unreleased_title_in_page_is_refused(self):
        with self.assertRaises(SystemExit) as c:
            ao.verify_emulator(self.emulator("e", page(titles=("Strings", "CELL AIR"))), "1.1.0", [1], TITLES, self.worklet())
        self.assertIn("unreleased stage 2", str(c.exception))

    def test_missing_released_stage_is_refused(self):
        with self.assertRaises(SystemExit): ao.verify_emulator(self.emulator("e", page(titles=("x",))), "1.1.0", [1], TITLES, self.worklet())

    def test_other_refusals(self):
        w = self.worklet()
        cases = {"version": page(version="1.0.0"), "import": page(imports=True), "button": page(button=False)}
        for name, html in cases.items():
            with self.assertRaises(SystemExit, msg=name): ao.verify_emulator(self.emulator(name, html), "1.1.0", [1], TITLES, w)
        with self.assertRaises(SystemExit): ao.verify_emulator(self.emulator("extra", page(), extra=["tutorial_data.mjs"]), "1.1.0", [1], TITLES, w)
        with self.assertRaises(SystemExit): ao.verify_emulator(self.emulator("drop", page(), drop=["worklet.js"]), "1.1.0", [1], TITLES, w)
        other = self.root / "other.js"; other.write_bytes(b"different")
        with self.assertRaises(SystemExit): ao.verify_emulator(self.emulator("wl", page()), "1.1.0", [1], TITLES, other)

    def test_swap_keeps_the_verified_source_link_and_writes_a_receipt(self):
        site = self.root / "site"; old = self.emulator("old", page(link="../firmware/caravel-1.1.0-source.tar.gz"))
        (site).mkdir(); import shutil; shutil.copytree(old, site / "emulator")
        new = self.emulator("new", page(titles=("Strings",), link="https://placeholder"))
        ao.swap_emulator(site, new)
        html = (site / "emulator/index.html").read_text()
        self.assertIn('href="../firmware/caravel-1.1.0-source.tar.gz"', html); self.assertNotIn("placeholder", html)
        ao.write_receipt(site, "a" * 40, "b" * 40, "1.1.0", [1])
        receipt = json.loads((site / "emulator/academy-receipt.json").read_text())
        self.assertEqual((receipt["firmware_source_commit"], receipt["web_files_commit"], receipt["released_stages"]), ("a" * 40, "b" * 40, [1]))
        self.assertEqual(set(receipt["sha256"]), ao.EMU_FILES)


if __name__ == "__main__":
    unittest.main()
