#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""The tutorials: registry, Markdown renderer, images, the README banner and the generated overview, with disposable fixtures. No device, no deployment."""
import json
from pathlib import Path
import re
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "web"))
import make_site
import tutorial_pages as tp

TUT = ROOT / "docs/tutorials"


class TutorialPagesTest(unittest.TestCase):
    def setUp(self):
        self.data = tp.load_topics(ROOT)
        self.look = make_site.look_css((ROOT / "web/index_pkg.html").read_text())

    def test_registry_has_every_topic_of_the_agreed_structure(self):
        ids = [t["id"] for t in self.data["topics"]]
        self.assertEqual(ids, ["start-here", "basics", "sem", "hades", "drone", "ensemble", "seq-arp", "home", "harmony", "filter-fx", "midi-sync", "help"])
        self.assertEqual([t["num"] for t in self.data["topics"]], ["1", "2", "3.1", "3.2", "3.3", "3.4", "4", "5", "6", "7", "8", "9"])
        for t in self.data["topics"]:
            self.assertTrue(t["tagline"].endswith(".") and t["minutes"] > 0)

    def test_ready_pages_have_the_agreed_parts_and_only_known_images_and_links(self):
        self.assertEqual(tp.check_source(ROOT)["version"], self.data["version"])
        for t in self.data["topics"]:
            if t["status"] != "ready":
                continue
            title, lead, sixty, sections = tp.parse_page((TUT / t["md"]).read_text(encoding="utf-8"), self.data)
            self.assertTrue(title and lead and sixty.startswith('<aside class="sixty"><h2>In 60 seconds</h2>'), t["id"])
            self.assertNotIn("In 60 seconds</strong>", sixty)          # the title is the box's heading, not repeated in its text
            self.assertGreaterEqual(len([n for n, *_ in sections if n]), 3, t["id"])
            self.assertTrue(all(t["status"] == "ready" for t in self.data["topics"]))

    def test_screens_are_firmware_frames_and_drawings_are_self_contained(self):
        for path in sorted((TUT / "img").glob("start-*.png")) + sorted((TUT / "img").glob("card-*.png")):
            raw = path.read_bytes()
            self.assertEqual((raw[:8], struct.unpack(">II", raw[16:24])), (b"\x89PNG\r\n\x1a\n", (240, 240)), path.name)
        for path in sorted((TUT / "img").glob("device-*.svg")):
            svg = path.read_text(encoding="utf-8")
            self.assertTrue(svg.startswith("<svg") and "<script" not in svg and "http://" not in svg.replace("http://www.w3.org/2000/svg", ""), path.name)
            self.assertNotIn('href="http', svg)
            # no run of base64 that the publication check would read as a key (AKIA/ASIA plus 16 characters, tokens)
            self.assertIsNone(re.search(r"(?:AKIA|ASIA)[0-9A-Z]{16}|gh[pousr]_[A-Za-z0-9]{20,}|sk-[A-Za-z0-9_-]{20,}|xox[baprs]-[A-Za-z0-9-]{20,}", svg, re.IGNORECASE), path.name)
        banner = (TUT / "img" / self.data["banner"]).read_bytes()
        self.assertEqual(struct.unpack(">II", banner[16:24]), (1600, 440))

    def test_every_screen_a_page_uses_is_a_240_square_firmware_frame(self):
        seen = set()
        for t in self.data["topics"]:
            if t["status"] == "ready":
                seen |= set(re.findall(r"\]\(img/([^)\s]+\.png)\)", (TUT / t["md"]).read_text(encoding="utf-8")))
        self.assertGreaterEqual(len(seen), 40)
        for name in sorted(seen):
            raw = (TUT / "img" / name).read_bytes()
            self.assertEqual((raw[:8], struct.unpack(">II", raw[16:24])), (b"\x89PNG\r\n\x1a\n", (240, 240)), name)

    def test_sound_examples_are_labelled_native_renders_and_match_their_manifest(self):
        import hashlib
        manifest = json.loads((ROOT / "docs/audio/tutorials.json").read_text())
        self.assertIn("not a recording of an FM-1", manifest["origin"])
        by_file = {c["file"]: c for c in manifest["clips"]}
        used = tp.used_audio(self.data, ROOT)
        self.assertEqual(sorted(by_file), used)                                   # every clip is used, every used clip is in the manifest
        self.assertGreaterEqual(len(used), 16)
        for name, clip in by_file.items():
            path = ROOT / "docs/audio/tutorials" / name
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), clip["sha256"], name)
            self.assertTrue(path.read_bytes()[:3] in (b"ID3", b"\xff\xfb", b"\xff\xf3", b"\xff\xf2"), name)
            self.assertTrue(0.005 < clip["rms"] and clip["decoded_peak"] < 1.0 and 5 < clip["seconds"] < 15, name)
        for t in self.data["topics"]:                                              # a page says what kind of audio it plays
            text = (TUT / t["md"]).read_text(encoding="utf-8")
            if "[Listen:" in text:
                self.assertTrue("native DSP render" in text, t["id"])
                self.assertTrue(re.search(r"(\*\*)?not(\*\*)? a recording of an FM-1", text) is not None, t["id"])

    def test_markdown_conventions(self):
        text = ("# T\n\nLead.\n\n> **In 60 seconds**\n>\n> 1. one\n> 2. two\n\n## 1. Step\n\nPress `SEQ` and read [start](start-here.md), [faq](../faq.md#x).\n\n"
                "> [!TIP]\n> Careful tip.\n\n![Diagram: the front](img/device-controls.svg)\n\n![A screen](img/start-home.png)\n\n## Plain\n\n- a\n- b\n")
        title, lead, sixty, sections = tp.parse_page(text, self.data)
        self.assertEqual(title, "T")
        self.assertIn("<ol><li>one</li><li>two</li></ol>", sixty)
        num, heading, body, figs = sections[0]
        self.assertEqual((num, heading), ("1", "Step"))
        self.assertIn("<code>SEQ</code>", body)
        self.assertIn('href="../start-here/"', body)                                   # a ready topic: its site page
        self.assertIn('href="https://github.com/bonszaimusic/caravel/blob/main/docs/faq.md#x"', body)   # any other file: GitHub
        self.assertIn('<aside class="callout tip"><strong>Tip</strong>', body)
        self.assertEqual([k for k, _ in figs], ["device", "screen"])
        self.assertIn('src="../img/start-home.png" width="240" height="240"', figs[1][1])
        self.assertIsNone(sections[1][0])

    def test_site_pages_overview_and_pill(self):
        with tempfile.TemporaryDirectory() as temp:
            site = Path(temp)
            tp.add_tutorials(site, self.look, make_site.SHIP_PATH, "0.12.0", ROOT)
            overview = (site / "tutorials/index.html").read_text()
            self.assertIn('<span class="pill">v0.12.0</span>', overview)
            self.assertEqual(overview.count("<article class=\"card"), len(self.data["topics"]))
            self.assertNotIn("IN PREPARATION", overview)
            for t in self.data["topics"]:
                self.assertIn(t["title"], overview)
            self.assertIn('href="start-here/"', overview)
            self.assertIn("img/device-hero.svg", overview)
            page = (site / "tutorials/start-here/index.html").read_text()
            self.assertIn('<span class="pill">v0.12.0</span>', page)
            self.assertIn('id="step-1"', page)
            self.assertIn("../firmware/caravel-0.12.0-source.tar.gz", page)
            self.assertEqual(page, (tp.topic_page(self.data["topics"][0], self.data, self.look, make_site.SHIP_PATH, "0.12.0", ROOT)))
            for name in tp.used_images(self.data, ROOT):
                self.assertTrue((site / "tutorials/img" / name).is_file(), name)
            for name in tp.used_audio(self.data, ROOT):
                self.assertTrue((site / "tutorials/audio" / name).is_file(), name)
            sem = (site / "tutorials/sem/index.html").read_text()
            self.assertIn('<audio controls preload="none" src="../audio/sem-warm-pad.mp3"', sem)
            self.assertIn("native DSP render, not a device recording", sem)

    def test_generated_overview_readme_is_current_and_linked_at_the_top_of_the_readme(self):
        self.assertEqual((TUT / "README.md").read_text(encoding="utf-8"), tp.readme_text(self.data))
        top = "\n".join((ROOT / "README.md").read_text(encoding="utf-8").splitlines()[:8])
        self.assertIn('href="https://bonszaimusic.github.io/caravel/tutorials/"><img src="docs/tutorials/img/tutorials-banner.png"', top)
        self.assertIn("docs/tutorials/start-here.md", top)
        for url in re.findall(r'(?:src|href)="(docs/[^"]+)"', (ROOT / "README.md").read_text(encoding="utf-8")):
            self.assertTrue((ROOT / url).is_file(), url)


if __name__ == "__main__":
    unittest.main(verbosity=2)
