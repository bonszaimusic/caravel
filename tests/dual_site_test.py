#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Standalone dual-generation static-site isolation tests (no SDK/network)."""
import hashlib
import json
import pathlib
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "web"))
import dual_site
import frozen_classic


class DualSiteTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)

    def site(self, version):
        root = self.root / ("source-" + version)
        for p in ("webapp/installer", "webapp/editor", "emulator", "tutorials/a",
                  "performance", "firmware"):
            (root / p).mkdir(parents=True, exist_ok=True)
        payload = b"release fwsc " + version.encode()
        digest = hashlib.sha256(payload).hexdigest()
        fw = f"caravel-{version}.fwsc"
        (root / "firmware" / fw).write_bytes(payload)
        (root / "firmware/SHA256SUMS").write_text(f"{digest}  {fw}\n")
        (root / "index.html").write_text(f'<main><span class="pill">v{version}</span></main>')
        inst = {"version": version, "pkg": "../../firmware/" + fw, "sha256": digest,
                "size": len(payload), "release_verified": True}
        if version.startswith("1."):
            inst["breaking"] = True
        # This is the actual 1.x HTML/JS template, not a made-up string marker.
        # Only generated package metadata is synthetic in this test harness.
        extra = (ROOT / "tests/fixtures/installer_1x_guard.html").read_text(encoding="utf-8") if version.startswith("1.") else ""
        (root / "webapp/installer/index.html").write_text(json.dumps(inst) + extra)
        (root / "webapp/editor/index.html").write_text("editor " + version)
        (root / "emulator/index.html").write_text(f'<small class="version">v{version}</small>')
        (root / "emulator/felucca.wasm").write_bytes(b"\x00asm\x01\x00\x00\x00" + version.encode())
        (root / "tutorials/index.html").write_text("docs " + version)
        (root / "tutorials/a/index.html").write_text("deep link " + version)
        (root / "performance/data.zip").write_bytes(version.encode())
        (root / "previously-unlisted.txt").write_bytes(b"preserve old root downloads")
        return root

    def build(self, old, new, out):
        # Pin the synthetic fixture exactly; production always uses the checked-in inventory.
        manifest = self.root / "fixture-manifest.json"
        files = [{"path": p.relative_to(old).as_posix(), "size": p.stat().st_size,
                  "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
                 for p in old.rglob("*") if p.is_file() and not p.is_symlink()]
        manifest.write_text(json.dumps({"schema_version": 1, "version": "0.13.0",
                            "source_commit": frozen_classic.COMMIT, "files": files}))
        dual_site.build(old, new, out, manifest)

    def test_classic_only_keeps_legacy_links(self):
        source = self.site("0.13.0")
        out = self.root / "site"
        self.build(source, None, out)
        self.assertIn("IN DEVELOPMENT", (out / "index.html").read_text())
        self.assertIn("nicht mehr gepflegt", (out / "index.html").read_text())
        for p in source.rglob("*"):
            if p.is_file():
                name = p.relative_to(source)
                target = "release.html" if str(name) == "index.html" else name
                self.assertEqual(p.read_bytes(), (out / "classic" / target).read_bytes())
                if str(name) != "index.html":
                    self.assertEqual(p.read_bytes(), (out / name).read_bytes())
        self.assertIn("NO LONGER MAINTAINED", (out / "index.html").read_text())
        self.assertNotIn("no longer maintained", (source / "index.html").read_text())
        self.assertFalse((out / "v1").exists())
        self.assertIn("nicht mehr gepflegt", (out / "classic/index.html").read_text())
        for name in ("emulator/felucca.wasm", "webapp/installer/index.html",
                     "tutorials/a/index.html", "performance/data.zip", "firmware/caravel-0.13.0.fwsc"):
            original = (source / name).read_bytes()
            self.assertEqual(original, (out / name).read_bytes())
            self.assertEqual(original, (out / "classic" / name).read_bytes())
        self.assertIsNone(json.loads((out / "versions.json").read_text())["v1"])

    def test_two_versions_isolate_and_rewrite_modern_links(self):
        old, new = self.site("0.13.0"), self.site("1.0.0")
        url = dual_site.SITE_ORIGIN + "tutorials/"
        (new / "tutorials/index.html").write_text('<a href="' + url + '">Docs</a>')
        out = self.root / "result"
        # This test covers composition only. Real behavior is checked against
        # the complete 1.x site by check_generation_site.mjs, not a dummy HTML fixture.
        with patch.object(dual_site, "verify_generation_behavior", return_value={"result":"PASS"}):
            self.build(old, new, out)
        self.assertIn('href="v1/"', (out / "index.html").read_text())
        self.assertIn("/caravel/v1/tutorials/", (out / "v1/tutorials/index.html").read_text())
        self.assertNotIn("/caravel/v1/", (out / "classic/tutorials/index.html").read_text())
        self.assertEqual((out / "emulator/felucca.wasm").read_bytes(),
                         (out / "classic/emulator/felucca.wasm").read_bytes())
        self.assertNotEqual((out / "v1/emulator/felucca.wasm").read_bytes(),
                            (out / "classic/emulator/felucca.wasm").read_bytes())
        self.assertEqual(json.loads((out / "versions.json").read_text())["v1"]["version"], "1.0.0")

    def test_refuse_wrong_hash_preview_and_version(self):
        old = self.site("0.13.0")
        installer = old / "webapp/installer/index.html"
        raw = installer.read_text()
        for invalid in (raw.replace('"sha256": "', '"sha256": "bad'),
                        raw.replace('"release_verified": true', '"release_verified": false'),
                        raw.replace('"version": "0.13.0"', '"version": "1.0.0"')):
            installer.write_text(invalid)
            with self.assertRaises(ValueError):
                self.build(old, None, self.root / "out")
        installer.write_text(raw)
        old.joinpath("tutorials", "evil").symlink_to(old / "emulator", target_is_directory=True)
        with self.assertRaises(ValueError):
            self.build(old, None, self.root / "out")

    def test_refuse_missing_1x_safety_disclosure(self):
        old, new = self.site("0.13.0"), self.site("1.0.0")
        inst = new / "webapp/installer/index.html"
        inst.write_text(inst.read_text().replace("const brkReady = () =>", "const noGuard = () =>"))
        with self.assertRaises(ValueError):
            self.build(old, new, self.root / "out")

    def test_keywords_cannot_replace_generation_behavior(self):
        old, new = self.site("0.13.0"), self.site("1.0.0")
        with self.assertRaisesRegex(ValueError, "generation behavior"):
            self.build(old, new, self.root / "out")

    def test_v1_refuses_disabled_acknowledgements(self):
        old, modern = self.site("0.13.0"), self.site("1.0.0")
        path = modern / "webapp/installer/index.html"
        original = path.read_text()
        for invalid in (
            original.replace('"breaking": true', '"breaking": false'),
            original.replace('id="brk-understand"', 'id="deleted-understand"'),
            original.replace('id="brk-backup"', 'id="deleted-backup"'),
            original.replace('id="brk-type"', 'id="deleted-type"'),
            original.replace('!brkReady()', 'false'),
            original.replace('=== "UPDATE"', '=== "INSTALL"'),
            original.replace('await up.install(image, meta.product, step)', 'await blockedInstall(image)'),
        ):
            self.assertNotEqual(invalid, original)
            path.write_text(invalid)
            with patch.object(dual_site, "verify_generation_behavior", return_value={"result":"PASS"}):
                with self.assertRaises(ValueError):
                    self.build(old, modern, self.root / "out")
        path.write_text(original)

    def test_v1_safety_not_based_on_text_marker(self):
        old, modern = self.site("0.13.0"), self.site("1.0.0")
        path = modern / "webapp/installer/index.html"
        # Safety is supplied by guard wiring, not the literal FORMAT 2 disclosure.
        path.write_text(path.read_text().replace("FORMAT 2", "NEXT FORMAT"))
        with patch.object(dual_site, "verify_generation_behavior", return_value={"result":"PASS"}):
            self.build(old, modern, self.root / "out")
        self.assertTrue((self.root / "out/v1/index.html").is_file())

    def test_refuse_overwrite_and_overlapping_paths(self):
        old = self.site("0.13.0")
        existing = self.root / "existing"
        existing.mkdir()
        for out in (existing, old / "inside"):
            with self.assertRaises(ValueError):
                self.build(old, None, out)


if __name__ == "__main__":
    unittest.main()
