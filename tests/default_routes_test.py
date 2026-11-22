#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Pure-stdlib tests for 1.x default routing and frozen Classic URLs."""
import pathlib
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "web"))
import default_routes


class DefaultRoutesTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.old = self.root / "old"
        self.new = self.root / "new"
        self.stage = self.root / "stage"
        self.old.mkdir()
        self.new.mkdir()
        self.stage.mkdir()

    def put(self, root, rel, content):
        dest = root / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(content.encode() if isinstance(content, str) else content)
        return dest

    def fixture(self):
        self.put(self.old, "index.html", "<html><body>Classic release</body></html>")
        self.put(self.old, "emulator/index.html", '<a href="/caravel/tutorials/">Classic lessons</a>')
        self.put(self.old, "emulator/felucca.wasm", b"\x00asm\x01\x00\x00\x00OLD")
        self.put(self.old, "webapp/installer/index.html", 'CLASSIC <a href="https://bonszaimusic.github.io/caravel/emulator/">emulator</a>')
        self.put(self.old, "webapp/editor/index.html", 'CLASSIC <a href="/caravel/tutorials/">tutorials</a>')
        self.put(self.old, "firmware/caravel-0.13.0.fwsc", b"legacy package immutable")
        self.put(self.old, "firmware/SHA256SUMS", "old digest\n")
        self.put(self.old, "tutorials/a/index.html", "Old only lesson")
        self.put(self.old, "tutorials/guide.pdf", b"old pdf")
        self.put(self.old, "performance/data.zip", b"old zip")
        self.put(self.old, "sounds/old.wav", b"old wav")
        self.put(self.old, "links.html", '<a href="/caravel/0.13/tutorials/a/">already explicit</a>')
        self.put(self.new, "index.html", "<html><main>1.1 LIVE</main></html>")
        self.put(self.new, "emulator/index.html", '1.1 WASM <a href="/caravel/v1/emulator/">v1</a>')
        self.put(self.new, "emulator/felucca.wasm", b"\x00asm\x01\x00\x00\x00NEW")
        self.put(self.new, "webapp/installer/index.html", "1.1 BREAKING SAFETY GUARD")
        self.put(self.new, "webapp/editor/index.html", "1.1 EDITOR")
        self.put(self.new, "firmware/caravel-1.1.0.fwsc", b"new package immutable")
        self.put(self.new, "firmware/SHA256SUMS", "new digest\n")
        self.put(self.new, "tutorials/index.html", "New lessons")
        self.put(self.new, "tutorials/stage-1/index.html", "stage 1")
        self.put(self.new, "emulator/academy-release.json", '{"released":4}')
        shutil.copytree(self.old, self.stage / "classic")
        self.put(self.stage, "classic/index.html", '<html><body>Classic launcher <a href="../versions/">Versions</a></body></html>')
        shutil.copytree(self.new, self.stage / "v1")

    def test_default_uses_current_site_and_both_archives(self):
        self.fixture()
        copied, redirects = default_routes.activate(self.old, self.new, self.stage)
        self.assertGreaterEqual(copied, 3)
        self.assertGreaterEqual(redirects, 1)
        self.assertIn("1.1 LIVE", (self.stage / "index.html").read_text())
        self.assertIn('href="/caravel/versions/"', (self.stage / "index.html").read_text())
        for path in ("webapp/installer/index.html", "webapp/editor/index.html", "emulator/index.html", "tutorials/index.html", "tutorials/stage-1/index.html"):
            self.assertEqual((self.stage / path).read_bytes(), (self.new / path).read_bytes())
        self.assertEqual((self.stage / "emulator/felucca.wasm").read_bytes(), (self.new / "emulator/felucca.wasm").read_bytes())
        self.assertEqual((self.stage / "v1/emulator/felucca.wasm").read_bytes(), (self.new / "emulator/felucca.wasm").read_bytes())
        self.assertEqual((self.stage / "0.13/emulator/felucca.wasm").read_bytes(), (self.old / "emulator/felucca.wasm").read_bytes())
        self.assertEqual((self.stage / "classic/emulator/felucca.wasm").read_bytes(), (self.old / "emulator/felucca.wasm").read_bytes())
        self.assertEqual((self.stage / "0.13/firmware/caravel-0.13.0.fwsc").read_bytes(), b"legacy package immutable")
        self.assertEqual((self.stage / "0.13/release.html").read_bytes(), (self.old / "index.html").read_bytes())
        self.assertIn("Classic launcher", (self.stage / "0.13/index.html").read_text())
        self.assertEqual((self.old / "emulator/index.html").read_text(), '<a href="/caravel/tutorials/">Classic lessons</a>')

    def test_absolute_old_links_remain_classic_in_archive(self):
        self.fixture()
        default_routes.activate(self.old, self.new, self.stage)
        self.assertIn('href="/caravel/classic/tutorials/"', (self.stage / "classic/emulator/index.html").read_text())
        self.assertIn('href="/caravel/0.13/tutorials/"', (self.stage / "0.13/emulator/index.html").read_text())
        self.assertIn('href="https://bonszaimusic.github.io/caravel/classic/emulator/"',
                      (self.stage / "classic/webapp/installer/index.html").read_text())
        self.assertIn('href="https://bonszaimusic.github.io/caravel/0.13/emulator/"',
                      (self.stage / "0.13/webapp/installer/index.html").read_text())
        self.assertEqual((self.stage / "0.13/links.html").read_text(),
                         (self.stage / "classic/links.html").read_text())

    def test_legacy_deep_links_redirect_with_query_and_fragment(self):
        self.fixture()
        default_routes.activate(self.old, self.new, self.stage)
        old_deep = (self.stage / "tutorials/a/index.html").read_text()
        self.assertIn('/caravel/0.13/tutorials/a/', old_deep)
        self.assertIn('location.search+location.hash', old_deep)
        self.assertIn('robots" content="noindex"', old_deep)
        self.assertEqual((self.stage / "tutorials/stage-1/index.html").read_text(), "stage 1")
        self.assertEqual((self.stage / "performance/data.zip").read_bytes(), b"old zip")
        self.assertEqual((self.stage / "tutorials/guide.pdf").read_bytes(), b"old pdf")
        self.assertEqual((self.stage / "sounds/old.wav").read_bytes(), b"old wav")
        self.assertIn('/caravel/0.13/links.html', (self.stage / "links.html").read_text())

    def test_root_firmware_is_modern_and_old_named_download_survives(self):
        self.fixture()
        default_routes.activate(self.old, self.new, self.stage)
        self.assertEqual((self.stage / "firmware/SHA256SUMS").read_text(), "new digest\n")
        self.assertEqual((self.stage / "firmware/caravel-1.1.0.fwsc").read_bytes(), b"new package immutable")
        self.assertEqual((self.stage / "firmware/caravel-0.13.0.fwsc").read_bytes(), b"legacy package immutable")
        self.assertEqual((self.stage / "0.13/firmware/SHA256SUMS").read_text(), "old digest\n")

    def test_version_chooser_anchors_to_root_not_versions(self):
        page = '<html><head><title>Versions</title></head><body><a href="v1/">1.x</a><a href="classic/">Old</a></body></html>'
        self.assertIn('<base href="/caravel/">', default_routes.version_chooser(page))
        with self.assertRaisesRegex(ValueError, 'head'):
            default_routes.version_chooser('broken')

    def test_refuse_reserved_input_paths(self):
        self.fixture()
        self.put(self.new, "0.13/index.html", "evil namespace collision")
        with self.assertRaisesRegex(ValueError, "namespace"):
            default_routes.activate(self.old, self.new, self.stage)
        self.assertFalse((self.stage / "0.13").exists())

    def test_refuse_missing_namespaced_copy(self):
        self.fixture()
        shutil.rmtree(self.stage / "v1")
        with self.assertRaisesRegex(ValueError, "missing complete"):
            default_routes.activate(self.old, self.new, self.stage)

    def test_refuse_invalid_new_landing(self):
        self.fixture()
        self.put(self.new, "index.html", "no closing tags")
        with self.assertRaisesRegex(ValueError, "no </main>"):
            default_routes.activate(self.old, self.new, self.stage)

    def test_explicit_namespaced_links_with_query_remain_explicit(self):
        self.assertEqual(default_routes._remap("/caravel/versions.json?format=2", "classic"),
                         "/caravel/versions.json?format=2")
        self.assertEqual(default_routes._remap("/caravel/0.13/tutorials/#song", "classic"),
                         "/caravel/0.13/tutorials/#song")
        self.assertEqual(default_routes._remap("/caravel/emulator/?stage=1", "classic"),
                         "/caravel/classic/emulator/?stage=1")

    def test_root_binary_and_modern_source_unmodified(self):
        self.fixture()
        before = {(p.relative_to(self.old).as_posix()): p.read_bytes() for p in self.old.rglob('*') if p.is_file()}
        modern = {(p.relative_to(self.new).as_posix()): p.read_bytes() for p in self.new.rglob('*') if p.is_file()}
        default_routes.activate(self.old, self.new, self.stage)
        self.assertEqual(before, {(p.relative_to(self.old).as_posix()): p.read_bytes() for p in self.old.rglob('*') if p.is_file()})
        self.assertEqual(modern, {(p.relative_to(self.new).as_posix()): p.read_bytes() for p in self.new.rglob('*') if p.is_file()})


if __name__ == "__main__":
    unittest.main()
