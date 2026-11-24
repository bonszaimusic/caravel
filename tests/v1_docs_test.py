#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Pinned-source, preservation, scope and local-navigation checks; no SDK/build."""
import hashlib
from html.parser import HTMLParser
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from urllib.parse import unquote, urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "web"))
import v1_docs


class References(HTMLParser):
    def __init__(self):
        super().__init__(); self.urls = []
    def handle_starttag(self, tag, attrs):
        self.urls.extend(value for key, value in attrs if key in ("href", "src"))


class V1DocsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name); self.repo = self.root / "repo"; self.repo.mkdir()
        self.site = self.root / "site"; self.site.mkdir()
        subprocess.run(["git", "init", "-q", str(self.repo)], check=True)
        self.env = {**os.environ, "GIT_AUTHOR_NAME": "Caravel", "GIT_COMMITTER_NAME": "Caravel",
                    "GIT_AUTHOR_EMAIL": "", "GIT_COMMITTER_EMAIL": ""}
        for name in v1_docs.PAGES:
            content = "# Candidate guide\n\nCaravel **1.0.0 candidate**\n\n## Controls\n\n- PLAY\n- BUILD\n"
            if name == "docs/tutorials/start-here.md":
                content += '\n![PLAY](img/one-sound.png)\n\n[Basics](basics.md) · [Historical reference](../faq.md)\n'
            path = self.repo / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_text(content)
        for diagram in v1_docs.DIAGRAMS:
            for ext in (".svg", ".png"):
                (self.repo / (v1_docs.ART + diagram + ext)).write_bytes((diagram + ext).encode())
        path = self.repo / "docs/tutorials/img/one-sound.png"; path.parent.mkdir(); path.write_bytes(b"original-renderer-bytes")
        (self.repo / "docs/tutorials/old-classic.md").write_text("CLASSIC MUST NOT BE COPIED")
        self.commit()
        landing = '<!doctype html><main><p>' + v1_docs.OLD_EDITOR_BLURB + '</p></main>'
        for name, data in {"index.html": landing.encode(), "firmware/app.bin": b"firmware",
                           "firmware/caravel-1.0.0-source.tar.gz": b"source archive",
                           "webapp/editor/index.html": b"editor", "webapp/installer/index.html": b"installer",
                           "emulator/index.html": b"emulator", "emulator/felucca.wasm": b"\0asm"}.items():
            path = self.site / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data)
        self.record()

    def commit(self):
        subprocess.run(["git", "-C", str(self.repo), "add", "."], check=True)
        subprocess.run(["git", "-C", str(self.repo), "-c", "commit.gpgsign=false", "commit", "-qm", "fixture"], env=self.env, check=True)
        self.sha = subprocess.check_output(["git", "-C", str(self.repo), "rev-parse", "HEAD"], text=True).strip()

    def record(self):
        (self.site / "firmware/release-build.json").write_text(json.dumps({"version": "1.0.0", "source": {"commits": {".": self.sha}}}))

    def build(self, name="out"):
        out = self.root / name
        receipt = v1_docs.build(self.site, self.repo, self.sha, out)
        return out, receipt

    def test_exact_sources_scope_and_existing_site_preserved(self):
        before = v1_docs.tree(self.site); out, receipt = self.build()
        self.assertEqual(before, v1_docs.tree(self.site))
        self.assertEqual(receipt["original_site_sha256"], before)
        self.assertEqual(receipt["original_site_files_unchanged"], len(before) - 1)
        for name, expected in before.items():
            if name != "index.html":
                self.assertEqual(hashlib.sha256((out / name).read_bytes()).hexdigest(), expected)
        self.assertEqual(receipt["landing_index_sha256"]["before"], before["index.html"])
        self.assertEqual(receipt["landing_index_sha256"]["after"], hashlib.sha256((out / "index.html").read_bytes()).hexdigest())
        self.assertNotEqual(receipt["landing_index_sha256"]["before"], receipt["landing_index_sha256"]["after"])
        for source, row in receipt["sources"].items():
            data = subprocess.check_output(["git", "-C", str(self.repo), "show", f"{self.sha}:{source}"])
            self.assertEqual(data, (out / row["copy"]).read_bytes())
        self.assertEqual(len([name for name in receipt["sources"] if name.startswith(v1_docs.ART) and name.endswith((".png", ".svg"))]), 14)
        for slug in ("engine-analog", "engine-phys", "capture"):
            self.assertIn(f"docs/tutorials/{slug}.md", receipt["sources"])
            self.assertTrue((out / "tutorials" / slug / "index.html").is_file())
        self.assertEqual(len(v1_docs.GUIDES), 20)
        self.assertFalse(any("old-classic" in str(path) for path in out.rglob("*")))
        self.assertNotIn("CLASSIC MUST NOT BE COPIED", "".join(path.read_text() for path in (out / "tutorials").rglob("*.html")))

    def test_deterministic_and_ignores_uncommitted_source(self):
        first, _ = self.build("first")
        (self.repo / "docs/tutorials/start-here.md").write_text("UNCOMMITTED MARKER")
        second, _ = self.build("second")
        self.assertEqual(v1_docs.tree(first), v1_docs.tree(second))
        self.assertNotIn("UNCOMMITTED", (second / "tutorials/start-here/index.html").read_text())

    def test_published_documentation_keeps_source_and_hardware_limits(self):
        out = self.root / "published"
        receipt = v1_docs.build(self.site, self.repo, self.sha, out, published=True)
        self.assertEqual(receipt["kind"], "published-1.x-documentation")
        landing = (out / "index.html").read_text()
        self.assertIn("CARAVEL 1.0.0", landing)
        self.assertNotIn("PRIVATE CANDIDATE", landing)
        content = (out / "tutorials/index.html").read_text()
        self.assertNotIn("not published", content)
        self.assertIn("hardware acceptance is still open", content)
        self.assertIn(self.sha, content)
        # Academy list: every stage link must be a real relative link, never source text left in the HTML
        self.assertNotIn("relative(", content)
        for n in (1, 2, 3, 4, 5):
            self.assertIn('href="../emulator/index.html#stage-%d"' % n, content)
        self.assertEqual((out / "firmware/app.bin").read_bytes(), b"firmware")

    def test_navigation_status_and_source_receipt(self):
        out, _ = self.build()
        landing = (out / "index.html").read_text(); parser = References(); parser.feed(landing)
        self.assertIn("tutorials/", parser.urls)
        self.assertIn("candidate — not published", landing)
        self.assertIn("hardware acceptance is still open", landing)
        self.assertIn(v1_docs.NEW_EDITOR_BLURB, landing)
        self.assertNotIn(v1_docs.OLD_EDITOR_BLURB, landing)
        for page in (out / "tutorials").rglob("*.html"):
            content = page.read_text()
            self.assertIn("candidate — not published", content)
            self.assertIn("hardware acceptance is still open", content)
            self.assertIn(self.sha, content)
            parser = References(); parser.feed(content)
            for url in parser.urls:
                parts = urlsplit(url)
                if parts.scheme or url.startswith("#"):
                    continue
                self.assertFalse(url.startswith("/"), url)
                target = (page.parent / unquote(parts.path)).resolve()
                self.assertTrue(target.is_relative_to(out.resolve()), url)
                self.assertTrue(target.is_file(), (page, url))
        self.assertIn("source reference:", (out / "tutorials/start-here/index.html").read_text())
        self.assertIn("Documentation draft", (out / "tutorials/audio-engine-diagrams/index.html").read_text())

    def test_mismatch_existing_and_overlap_refused(self):
        for sha, out in [("a" * 40, self.root / "bad"), (self.sha, self.site / "nested"), (self.sha, self.site)]:
            with self.assertRaises(ValueError):
                v1_docs.build(self.site, self.repo, sha, out)
        self.build()
        with self.assertRaises(ValueError):
            self.build()

    def test_unselected_image_and_symlink_refused(self):
        (self.repo / "docs/tutorials/start-here.md").write_text("# Candidate\n\n![Old](img/classic.png)\n")
        self.commit(); self.record()
        with self.assertRaisesRegex(ValueError, "noncandidate"):
            self.build()
        self.assertFalse((self.root / "out").exists())
        (self.site / "unsafe").symlink_to(self.site / "index.html")
        with self.assertRaisesRegex(ValueError, "linked"):
            self.build()

    def test_existing_tutorials_and_git_symlink_refused(self):
        existing = self.site / "tutorials"; existing.mkdir()
        (existing / "index.html").write_text("existing guide must survive")
        with self.assertRaisesRegex(ValueError, "existing tutorials"):
            self.build()
        self.assertEqual((existing / "index.html").read_text(), "existing guide must survive")
        (existing / "index.html").unlink(); existing.rmdir()
        source = self.repo / "docs/tutorials/start-here.md"
        source.unlink(); source.symlink_to("basics.md")
        self.commit(); self.record()
        with self.assertRaisesRegex(ValueError, "nonregular"):
            self.build()

    def test_changed_landing_marker_refused(self):
        path = self.site / "index.html"
        path.write_text(path.read_text().replace(v1_docs.OLD_EDITOR_BLURB, "changed description"))
        with self.assertRaisesRegex(ValueError, "landing markers"):
            self.build()
        self.assertFalse((self.root / "out").exists())


if __name__ == "__main__":
    unittest.main()
