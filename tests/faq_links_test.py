#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Offline checks for FAQ links on an existing verified-release installer."""
import importlib.util
from pathlib import Path
import re
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("faq_links", ROOT / "web/faq_links.py")
FAQ = importlib.util.module_from_spec(spec)
spec.loader.exec_module(FAQ)


class FAQLinksTest(unittest.TestCase):
    def setUp(self):
        self.current = (ROOT / "web/index_pkg.html").read_text()
        self.old = self.current
        for _, link in FAQ.LINKS:
            self.assertEqual(self.old.count(link), 1)
            self.old = self.old.replace(link, "", 1)

    def test_older_installer_gets_exact_current_links_without_script_changes(self):
        result = FAQ.add_links(self.old)
        self.assertEqual(result, self.current)
        script = lambda text: re.findall(r'<script.*?</script>', text, re.S)
        self.assertEqual(script(result), script(self.old))

    def test_already_linked_release_is_idempotent(self):
        self.assertEqual(FAQ.add_links(self.current), self.current)

    def test_060_tutorial_navigation_keeps_faq_insertion_working(self):
        marker = FAQ.LINKS[0][0]
        old = self.old.replace(marker, '<a href="../../tutorials/">TUTORIALS</a>' + marker)
        result = FAQ.add_links(old)
        self.assertEqual(result.count(FAQ.FAQ_URL), 3)
        self.assertIn('href="../../tutorials/"', result)

    def test_partial_links_and_changed_markers_fail_before_site_write(self):
        for text in [self.old.replace('<p id="status"></p>', ""),
                     self.old + FAQ.LINKS[0][1], self.current + FAQ.LINKS[0][1]]:
            with self.subTest(text=text[-60:]), self.assertRaises(ValueError):
                FAQ.add_links(text)

    def test_site_overlay_preserves_firmware_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            site = Path(directory)
            installer = site / "webapp/installer/index.html"
            installer.parent.mkdir(parents=True)
            installer.write_text(self.old)
            (site / "firmware").mkdir()
            package = site / "firmware/caravel.fwsc"
            package.write_bytes(b"TEST ONLY unchanged firmware artifact")
            before = FAQ.firmware_hashes(site)
            FAQ.apply(site)
            self.assertEqual(FAQ.firmware_hashes(site), before)
            self.assertEqual(installer.read_text(), self.current)
            self.assertEqual(list(site.rglob('*.fwsc')), [package])

    def test_faq_section_links_have_targets(self):
        text = (ROOT / "docs/faq.md").read_text()
        headings = {re.sub(r'[^\w\- ]', '', line.lower()).replace(' ', '-')
                    for line in re.findall(r'^## (.+)$', text, re.M)}
        for target in re.findall(r'\]\(#([^)]*)\)', text):
            self.assertIn(target, headings)
        self.assertIn('stock-return-error--5', headings)


if __name__ == '__main__':
    unittest.main(verbosity=2)
