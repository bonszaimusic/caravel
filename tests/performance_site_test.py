#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Website-content checks with disposable HTML fixtures, no device or deployment."""
import hashlib
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "web"))
import make_site
import performance_pages as performance
import stasis_pages


class PerformanceSiteTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.site = Path(self.temp.name) / "site"
        self.site.mkdir()
        template = (ROOT / "web/index_pkg.html").read_text()
        look = make_site.look_css(template)
        pages = {
            "index.html": make_site.landing(look, "0.6.0", True, "firmware/caravel-0.6.0-source.tar.gz"),
            "webapp/installer/index.html": '<style>/*LOOK-BEGIN*/\n' + look + '\n/*LOOK-END*/</style><script type="application/json">{"release_verified": true}</script>',
            "webapp/editor/index.html": "<p>TEST ONLY unchanged editor fixture</p>",
            "emulator/index.html": "<p>TEST ONLY unchanged emulator fixture</p>",
        }
        for name in ("tutorials", "sounds", "changelog"):
            pages[name + "/index.html"] = stasis_pages.page("TEST ONLY " + name, "<p>Fixture</p>", look, make_site.SHIP_PATH)
        for name, text in pages.items():
            path = self.site / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
        for name in ("LICENSING.md", "NOTICE", "CREDITS.md", "LICENSE", "caravel-0.6.0-source.tar.gz", "caravel-0.6.0.fwsc"):
            path = self.site / "firmware" / name
            path.parent.mkdir(exist_ok=True)
            path.write_bytes(b"TEST ONLY unchanged release inventory fixture: " + name.encode())

    def protected(self):
        paths = list((self.site / "firmware").iterdir())
        paths += [self.site / name for name in ("webapp/installer/index.html", "webapp/editor/index.html", "emulator/index.html")]
        return {p.relative_to(self.site).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}

    def test_downloads_match_and_all_release_assets_are_preserved(self):
        before = self.protected()
        performance.build(self.site)
        self.assertEqual(before, self.protected())
        self.assertEqual((self.site / "performance" / performance.PROJECT).read_bytes(),
                         (performance.ASSETS / performance.PROJECT).read_bytes())
        self.assertEqual((self.site / "performance" / performance.PACKAGE).read_bytes(),
                         (performance.ASSETS / performance.PACKAGE).read_bytes())
        self.assertIn('href="performance/"', (self.site / "index.html").read_text())
        for name in ("tutorials", "sounds"):
            self.assertIn("Download the performance setup", (self.site / name / "index.html").read_text())
        initial = {p.relative_to(self.site): p.read_bytes() for p in self.site.rglob("*") if p.is_file()}
        performance.build(self.site)
        self.assertEqual(initial, {p.relative_to(self.site): p.read_bytes() for p in self.site.rglob("*") if p.is_file()})

    def test_changed_download_is_rejected_before_site_write(self):
        source = Path(self.temp.name) / "assets"
        shutil.copytree(performance.ASSETS, source)
        (source / performance.PROJECT).write_bytes(b"TEST ONLY corrupt project")
        before = {p.relative_to(self.site): p.read_bytes() for p in self.site.rglob("*") if p.is_file()}
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            performance.build(self.site, source)
        self.assertEqual(before, {p.relative_to(self.site): p.read_bytes() for p in self.site.rglob("*") if p.is_file()})

    def test_preview_and_other_version_do_not_get_release_downloads(self):
        path = self.site / "webapp/installer/index.html"
        path.write_text(path.read_text().replace('"release_verified": true', '"release_verified": false'))
        with self.assertRaisesRegex(ValueError, "verified Caravel"):
            performance.build(self.site)
        self.assertFalse((self.site / "performance").exists())
        path.write_text(path.read_text().replace('"release_verified": false', '"release_verified": true'))
        landing = self.site / "index.html"
        landing.write_text(landing.read_text().replace("v0.6.0", "v0.5.0"))
        with self.assertRaisesRegex(ValueError, "verified Caravel"):
            performance.build(self.site)
        self.assertFalse((self.site / "performance").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
