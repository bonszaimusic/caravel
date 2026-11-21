# SPDX-License-Identifier: GPL-3.0-only
"""Offline negative tests for Pages W2 overlay; no network, Actions or hardware."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

APPLY = Path(__file__).resolve().parents[1] / 'web/hotfix_w2/apply.py'
spec = importlib.util.spec_from_file_location('hotfix_w2', APPLY)
H = importlib.util.module_from_spec(spec)
spec.loader.exec_module(H)


class OverlayTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name).resolve(strict=True)
        self.site = self.root / 'site'
        self.source = self.root / 'public'
        self.tag = self.root / 'tag'
        self.release = self.root / 'release'
        self.site.mkdir(); self.source.mkdir(); self.tag.mkdir(); self.release.mkdir()
        for path in ('v1/webapp/installer', 'v1/firmware', 'emulator'):
            (self.site / path).mkdir(parents=True, exist_ok=True)
        (self.tag / 'web').mkdir()
        (self.source / 'web/hotfix_w2').mkdir(parents=True)
        self.old_pkg = b'approved package, fixture only'
        self.sha = H.digest(self.old_pkg)
        self.meta = {'version': H.VERSION, 'source': '../../firmware/caravel-1.1.0-source.tar.gz',
                     'release_verified': True, 'breaking': True,
                     'sha256': self.sha, 'size': len(self.old_pkg),
                     'pkg': '../../firmware/' + H.FW_NAME, 'product': 'FM-1_999'}
        (self.release / H.FW_NAME).write_bytes(self.old_pkg)
        (self.site / 'v1/firmware' / H.FW_NAME).write_bytes(self.old_pkg)
        source = b'exact published source archive, test fixture'
        (self.release / 'caravel-1.1.0-source.tar.gz').write_bytes(source)
        (self.site / 'v1/firmware/caravel-1.1.0-source.tar.gz').write_bytes(source)
        self.baseline = ('<main>Caravel</main><a id="matching-source" '
                         'href="../../firmware/caravel-1.1.0-source.tar.gz">Firmware source</a>'
                         '<script>const meta = '+json.dumps(self.meta)+';</script>')
        (self.site / H.PATH).write_text(self.baseline)
        (self.site / 'emulator' / 'index.html').write_text('original Academy stays')
        (self.site / 'index.html').write_text('original landing stays')
        template = ('<main><h1>Caravel</h1><p>Enter UBOOT (no flash write)</p>'
                    '<p>WL80UBOOT1.00 / WL82 UBOOT1.00</p></main>'
                    '<a id="matching-source" href="OLD">Firmware source</a>'
                    '<script>/*LIB*/\nconst meta = /*META*/;\nconst v="@VERSION@";</script>')
        self.templates = {'index_pkg.html': template.encode(),
                          'fm1ota.js': b'export const UBOOT = 18;\n'}
        self.original_blobs = H.APPROVED_BLOBS
        H.APPROVED_BLOBS = {n: H.blobsha(s) for n, s in self.templates.items()}
        self.addCleanup(lambda: setattr(H, 'APPROVED_BLOBS', self.original_blobs))
        for name, data in self.templates.items():
            (self.source / 'web/hotfix_w2' / name).write_bytes(data)
        for name in ('generation.js', 'fm1pkg.js', 'fm1backup.js'):
            (self.tag / 'web' / name).write_text('export const ' + name.replace('.', '_') + ' = 1;\n')
        self.rb = self.root / 'rollback'
        self.receipt = self.root / 'receipt.json'
        self.override = mock.patch.multiple(H, FW_SHA=self.sha, FW_SIZE=len(self.old_pkg))
        self.override.start()
        self.addCleanup(self.override.stop)

    def apply(self):
        return H.overlay(self.site, self.source, self.tag, self.release, self.rb,
                         self.receipt, 'a'*40)

    def test_round_trip_only_installer_updated(self):
        before = H.inventory(self.site)
        self.apply()
        after = H.inventory(self.site)
        self.assertEqual([k for k in after if after[k] != before[k]], [H.PATH])
        self.assertEqual(before, H.inventory(self.rb))
        self.assertFalse((self.site / 'SITE_HOTFIX_MANIFEST.json').exists())
        self.assertEqual(json.loads(self.receipt.read_text())['changed'], [H.PATH])
        self.assertIn('Matching W2 web source', (self.site / H.PATH).read_text())
        self.assertIn('../../firmware/caravel-1.1.0-source.tar.gz', (self.site / H.PATH).read_text())

    def test_reject_modified_firmware(self):
        (self.site / 'v1/firmware' / H.FW_NAME).write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError, 'published firmware was modified'):
            self.apply()
        self.assertFalse(self.rb.exists())

    def test_reject_modified_source_archive(self):
        (self.site / 'v1/firmware/caravel-1.1.0-source.tar.gz').write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError, 'source archive was modified'):
            self.apply()
        self.assertFalse(self.rb.exists())

    def test_reject_unapproved_source_blob(self):
        (self.source / 'web/hotfix_w2/fm1ota.js').write_bytes(b'not approved')
        with self.assertRaisesRegex(ValueError, 'unreviewed web source'):
            self.apply()
        self.assertFalse(self.rb.exists())

    def test_reject_missing_release_source_link(self):
        (self.site / H.PATH).write_text(self.baseline.replace('../../firmware/caravel-1.1.0-source.tar.gz', '../wrong-source.tar.gz'))
        with self.assertRaisesRegex(ValueError, 'missing matching release source archive link'):
            self.apply()

    def test_reject_existing_rollback_without_overwrite(self):
        self.rb.mkdir()
        with self.assertRaisesRegex(ValueError, 'already exist'):
            self.apply()

    def test_reject_symlink_site(self):
        (self.site / 'emulator/link').symlink_to(self.release, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'symlink or special'):
            self.apply()

    def test_reject_missing_rev_and_no_private_metadata_in_webroot(self):
        H.APPROVED_BLOBS['index_pkg.html'] = 'f'*40
        with self.assertRaises(ValueError):
            self.apply()
        self.assertFalse((self.site / 'SITE_HOTFIX_MANIFEST.json').exists())


if __name__ == '__main__':
    unittest.main(verbosity=2)
