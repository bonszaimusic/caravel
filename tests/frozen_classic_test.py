#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Offline checks of the immutable Classic byte transport and inventory."""
import functools
import hashlib
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import sys
import tempfile
import threading
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "web"))
import frozen_classic as classic


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


class FrozenTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.source = self.root / "source"
        (self.source / "nested").mkdir(parents=True)
        (self.source / "index.html").write_bytes(b"unchanged Classic\r\n")
        (self.source / "nested/download.bin").write_bytes(bytes(range(256)))
        self.manifest = self.root / "manifest.json"
        self.data = {"schema_version": 1, "version": "0.13.0", "source_commit": classic.COMMIT,
                     "files": [{"path": p.relative_to(self.source).as_posix(),
                                "size": p.stat().st_size,
                                "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
                               for p in self.source.rglob("*") if p.is_file()]}
        self.save()

    def save(self):
        self.manifest.write_text(json.dumps(self.data))

    def server(self):
        server = ThreadingHTTPServer(("127.0.0.1", 0),
                 functools.partial(QuietHandler, directory=str(self.source)))
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(server.server_close)
        self.addCleanup(thread.join)
        self.addCleanup(server.shutdown)
        return f"http://127.0.0.1:{server.server_port}/"

    def test_exact_copy_and_no_overwrite(self):
        out = self.root / "out"
        classic.fetch(out, self.manifest, self.server())
        classic.verify(out, self.manifest)
        for p in self.source.rglob("*"):
            if p.is_file():
                self.assertEqual(p.read_bytes(), (out / p.relative_to(self.source)).read_bytes())
        with self.assertRaises(ValueError):
            classic.fetch(out, self.manifest, self.server())

    def test_changed_download_refused_atomically(self):
        (self.source / "index.html").write_bytes(b"different published bytes")
        out = self.root / "out"
        with self.assertRaisesRegex(ValueError, "bytes differ"):
            classic.fetch(out, self.manifest, self.server())
        self.assertFalse(out.exists())

    def test_missing_extra_changed_and_linked_files_refused(self):
        extra = self.source / "extra"
        extra.write_bytes(b"not pinned")
        with self.assertRaises(ValueError):
            classic.verify(self.source, self.manifest)
        extra.unlink()
        original = (self.source / "index.html").read_bytes()
        (self.source / "index.html").write_bytes(b"x" * len(original))
        with self.assertRaisesRegex(ValueError, "bytes differ"):
            classic.verify(self.source, self.manifest)
        (self.source / "index.html").unlink()
        with self.assertRaises(ValueError):
            classic.verify(self.source, self.manifest)
        (self.source / "index.html").symlink_to(self.source / "nested/download.bin")
        with self.assertRaises(ValueError):
            classic.verify(self.source, self.manifest)

    def test_untrusted_inventory_paths_refused(self):
        row = self.data["files"][0].copy()
        for name in ("../outside", "/absolute", "a/../b", "a\\b", "./a", "a//b", ""):
            self.data["files"][0] = {**row, "path": name}
            self.save()
            with self.assertRaises(ValueError, msg=name):
                classic.inventory(self.manifest)
        self.data["files"] = [row, row]
        self.save()
        with self.assertRaises(ValueError):
            classic.inventory(self.manifest)

    def test_wrong_generation_and_remote_http_refused(self):
        with self.assertRaises(ValueError):
            classic.fetch(self.root / "out", self.manifest, "http://example.org/")
        self.data["version"] = "0.13.1"
        self.save()
        with self.assertRaises(ValueError):
            classic.inventory(self.manifest)

    def test_pinned_html_requires_dynamic_and_transitive_modules(self):
        # A valid file hash alone must not hide an incomplete published snapshot.
        def pin(path, payload):
            path.write_bytes(payload)
            name = path.relative_to(self.source).as_posix()
            self.data["files"] = [r for r in self.data["files"] if r["path"] != name]
            self.data["files"].append({"path": name, "size": len(payload),
                "sha256": hashlib.sha256(payload).hexdigest()})
            self.save()
        pin(self.source / "index.html", b'<script>import("https://bonszaimusic.github.io/caravel/nested/module.js")</script>')
        with self.assertRaisesRegex(ValueError, "runtime dependency"):
            classic.verify(self.source, self.manifest)
        pin(self.source / "nested/module.js", b'export {value} from "./leaf.js";')
        with self.assertRaisesRegex(ValueError, "runtime dependency"):
            classic.verify(self.source, self.manifest)
        pin(self.source / "nested/leaf.js", b'export const value = 1;')
        classic.verify(self.source, self.manifest)

        pin(self.source / "nested/leaf.js", b'import("/caravel/%2e%2e/outside.js")')
        with self.assertRaisesRegex(ValueError, "runtime dependency"):
            classic.verify(self.source, self.manifest)


if __name__ == "__main__":
    unittest.main()
