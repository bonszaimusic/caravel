#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Offline site/release correspondence tests; ALL firmware/SDK bytes are synthetic.

Reuse the release suite's disposable fixture, never canonical Git history or build
outputs. No network, GitHub workflow execution, device access or deployment.
"""
import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from html.parser import HTMLParser
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import release_artifacts as R


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


RT = load("release_fixture", ROOT / "tests/release_artifacts_test.py")
SITE = load("site_builder", ROOT / "web/make_site.py")
VERSION = "0.1.0"


def workflow_blocks():
    lines = (ROOT / ".github/workflows/pages.yml").read_text().splitlines()
    blocks, i, name = {}, 0, None
    while i < len(lines):
        if lines[i].strip().startswith("- name: "):
            name = lines[i].strip().removeprefix("- name: ")
        if lines[i].strip() == "run: |":
            indent = len(lines[i]) - len(lines[i].lstrip()) + 2
            i += 1
            block = []
            while i < len(lines) and (not lines[i].strip() or len(lines[i]) - len(lines[i].lstrip()) >= indent):
                block.append(lines[i][indent:] if lines[i].strip() else "")
                i += 1
            blocks[name] = "\n".join(block) + "\n"
        else:
            i += 1
    return blocks


def workflow_python(name):
    block = workflow_blocks()[name]
    match = re.fullmatch(r"(?:#[^\n]*\n)*python3 - <<'PY'\n(.*)\nPY\n", block, re.S)
    if not match:
        raise AssertionError("Expected one embedded Python script: " + name)
    return compile(match[1], "pages.yml: " + name, "exec")


class Links(HTMLParser):
    def __init__(self):
        super().__init__()
        self.targets = []

    def handle_starttag(self, tag, attrs):
        self.targets.extend(value for key, value in attrs if key in ("href", "src") and value)


class SiteReleaseTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        RT.ReleaseArtifactsTest.setUpClass()

    @classmethod
    def tearDownClass(cls):
        RT.ReleaseArtifactsTest.tearDownClass()

    def setUp(self):
        self.fixture = RT.ReleaseArtifactsTest(methodName="runTest")
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root = self.fixture.root
        paths = ["index_pkg.html", "editor.html", "fm1pkg.js", "fm1ota.js", "fm1backup.js",
                 "fukiai.ttf", "FUKIAI-LICENSE.txt", "emu/index.html", "emu/worklet.js",
                 "emu/fonts/DotGothic16-subset.woff", "emu/fonts/OFL.txt"]
        for name in paths:
            RT.put(self.root, "web/" + name, (ROOT / "web" / name).read_bytes())
        RT.commit(self.root)
        R.record_build(self.root, VERSION, R.source_snapshot(self.root), self.fixture.sdk)
        self.release = R.release(self.root, VERSION, sdk=self.fixture.sdk)
        self.package = self.release / "caravel-0.1.0.fwsc"
        self.out = self.root.parent / "site"
        self.emulator = self.root.parent / "emulator"
        self.emulator.mkdir()
        for name in SITE.EMULATOR_FILES:
            if name == "felucca.wasm":
                RT.put(self.emulator, name, b"\0asm\x01\0\0\0TEST ONLY synthetic module")
            else:
                data = (self.root / "web/emu" / name).read_bytes()
                if name == "index.html":
                    data = data.replace(b"@VERSION@", b"v0.1.0").replace(b"@PREVIEW@", b"")
                RT.put(self.emulator, name, data)
        patcher = mock.patch.object(SITE, "HERE", self.root / "web")
        patcher.start()
        self.addCleanup(patcher.stop)

    def build(self, **kwargs):
        return SITE.main(self.package, VERSION, self.out, release_dir=self.release,
                         emulator=self.emulator, **kwargs)

    def rehash(self):
        data = {p.name: p.read_bytes() for p in self.release.iterdir() if p.name != "SHA256SUMS"}
        (self.release / "SHA256SUMS").write_bytes(R.checksum_bytes(data))

    def test_release_site_carries_matching_source_and_all_notices(self):
        self.build()
        for filename in (*R.DOCS, "ATTRIBUTION.txt", "release-build.json", "SHA256SUMS",
                         "caravel-0.1.0-source.tar.gz", *R.LICENSES):
            self.assertEqual((self.out / "firmware" / filename).read_bytes(), (self.release / filename).read_bytes())
        for filename in R.LICENSES:
            self.assertEqual((self.out / "firmware/LICENSES" / filename).read_bytes(), (self.release / filename).read_bytes())
        inst = (self.out / "webapp/installer/index.html").read_text()
        emu = (self.out / "emulator/index.html").read_text()
        self.assertIn('href="../../firmware/caravel-0.1.0-source.tar.gz"', inst)
        self.assertIn('href="../firmware/caravel-0.1.0-source.tar.gz"', emu)
        self.assertIn('"release_verified": true', inst)
        self.assertIn('"sha256": "' + hashlib.sha256(self.package.read_bytes()).hexdigest() + '"', inst)
        self.assertIn('"size": ' + str(self.package.stat().st_size), inst)
        self.assertIn('validateCaravelPackage(pkg, meta)', inst)
        self.assertIn('image = verified.image;', inst)
        for path in self.out.rglob("*.html"):
            parser = Links()
            parser.feed(path.read_text())
            for dest in parser.targets:
                url = urlsplit(dest)
                if url.scheme or url.netloc or not url.path:
                    continue
                self.assertTrue((path.parent / unquote(url.path)).exists(), (path.relative_to(self.out), dest))

    def test_notice_change_even_with_recomputed_checksums_is_rejected(self):
        (self.release / "CREDITS.md").write_text("TEST ONLY tampered notice")
        self.rehash()
        with self.assertRaisesRegex(R.ReleaseError, "notice/source"):
            self.build()
        self.assertFalse(self.out.exists())

    def test_wrong_source_commit_in_record_is_rejected(self):
        path = self.release / "release-build.json"
        record = json.loads(path.read_text())
        record["source"]["commits"]["."] = "0" * 40
        path.write_bytes(R.json_bytes(record))
        self.rehash()
        with self.assertRaisesRegex(R.ReleaseError, "record mismatch"):
            self.build()

    def test_missing_matching_source_is_rejected(self):
        (self.release / "caravel-0.1.0-source.tar.gz").unlink()
        self.rehash()
        with self.assertRaisesRegex(R.ReleaseError, "inventory mismatch"):
            self.build()

    def test_modified_source_archive_is_rejected(self):
        (self.release / "caravel-0.1.0-source.tar.gz").write_bytes(b"TEST ONLY invalid archive")
        self.rehash()
        with self.assertRaises(Exception):
            self.build()
        self.assertFalse(self.out.exists())

    def test_wrong_version_is_rejected(self):
        # Different versions now share Caravel's fixed FM-1_999 identity.
        # The verified release directory still must match the requested
        # version; a 0.1.0 download cannot stand in for 0.2.0.
        with self.assertRaisesRegex(R.ReleaseError, "missing required file.*caravel-0\\.2\\.0\\.fwsc"):
            SITE.main(self.package, "0.2.0", self.out, release_dir=self.release)
        self.assertFalse(self.out.exists())

    def test_package_outside_verified_directory_is_rejected(self):
        elsewhere = self.root.parent / "copied-package.fwsc"
        shutil.copyfile(self.package, elsewhere)
        with self.assertRaisesRegex(R.ReleaseError, "verified release directory"):
            SITE.main(elsewhere, VERSION, self.out, release_dir=self.release)

    def test_manifest_path_escape_is_rejected_before_writes(self):
        (self.release / "SHA256SUMS").write_text("0" * 64 + "  ../outside\n")
        with self.assertRaises(R.ReleaseError):
            self.build()
        self.assertFalse(self.out.exists())

    def test_preview_must_be_explicit_and_is_labelled(self):
        with self.assertRaisesRegex(R.ReleaseError, "provide --release-dir"):
            SITE.main(self.package, VERSION, self.out)
        SITE.main(self.package, VERSION, self.out, preview=True)
        text = (self.out / "webapp/installer/index.html").read_text()
        self.assertIn("not release-verified", text)
        self.assertIn('"release_verified": false', text)
        self.assertIn('"sha256": "' + hashlib.sha256(self.package.read_bytes()).hexdigest() + '"', text)
        self.assertIn('validateCaravelPackage(pkg, meta)', text)
        self.assertFalse((self.out / "firmware/caravel-0.1.0-source.tar.gz").exists())

    def test_preview_accepts_default_build_identity_without_release_claim(self):
        package = self.fixture.write_package(product="FM-1_900")
        SITE.main(package, VERSION, self.out, preview=True)
        text = (self.out / "webapp/installer/index.html").read_text()
        self.assertIn('"product": "FM-1_900"', text)
        self.assertIn('"release_verified": false', text)

    def test_preview_emulator_is_always_labelled_without_changing_input(self):
        path = self.emulator / "index.html"
        original = path.read_text()
        for suffix in ("", " (preview)"):
            with self.subTest(input_suffix=suffix):
                input_html = original.replace("v0.1.0</small>", "v0.1.0" + suffix + "</small>")
                path.write_text(input_html)
                SITE.main(self.package, VERSION, self.out, preview=True, emulator=self.emulator)
                emitted = (self.out / "emulator/index.html").read_text()
                self.assertIn('v0.1.0 (local preview; not release-verified)</small>', emitted)
                self.assertNotIn('v0.1.0</small>', emitted)
                self.assertEqual(path.read_text(), input_html)
                shutil.rmtree(self.out)

    def test_output_symlink_ancestors_are_rejected_before_staging(self):
        outside = self.root.parent / "outside"
        outside.mkdir()
        RT.put(outside, "nested/keep.txt", b"DO NOT CHANGE")
        link = self.root.parent / "redirect"
        link.symlink_to(outside, target_is_directory=True)
        before = {p.relative_to(outside): p.read_bytes() for p in outside.rglob("*") if p.is_file()}
        for dest in (link / "site", link / "nested/site", link / "nested/../site"):
            with self.subTest(destination=dest), mock.patch.object(SITE.tempfile, "TemporaryDirectory") as stage:
                with self.assertRaisesRegex(R.ReleaseError, "symlink output parent"):
                    SITE.main(self.package, VERSION, dest, preview=True)
                stage.assert_not_called()
                self.assertEqual({p.relative_to(outside): p.read_bytes() for p in outside.rglob("*")
                                  if p.is_file()}, before)
                self.assertEqual(sorted(p.relative_to(outside).as_posix() for p in outside.rglob("*")),
                                 ["nested", "nested/keep.txt"])

    def test_output_symlink_itself_is_not_followed(self):
        outside = self.root.parent / "outside"
        outside.mkdir()
        self.out.symlink_to(outside, target_is_directory=True)
        with mock.patch.object(SITE.tempfile, "TemporaryDirectory") as stage:
            with self.assertRaisesRegex(R.ReleaseError, "new or empty"):
                self.build()
            stage.assert_not_called()
        self.assertTrue(self.out.is_symlink())
        self.assertEqual(list(outside.iterdir()), [])

    @unittest.skipUnless(sys.platform == "darwin" and Path("/var").is_symlink(), "macOS /var alias")
    def test_macos_var_alias_is_allowed_but_nested_symlinks_are_not(self):
        canonical = self.root.parent.resolve()
        if not canonical.is_relative_to("/private/var"):
            self.skipTest("Fixture temporary directory is not under /private/var")
        alias = Path("/var") / canonical.relative_to("/private/var")
        SITE.main(self.package, VERSION, alias / "site", preview=True)
        self.assertTrue((self.out / "webapp/installer/index.html").is_file())
        outside = canonical / "outside"
        outside.mkdir()
        (canonical / "redirect").symlink_to(outside, target_is_directory=True)
        with mock.patch.object(SITE.tempfile, "TemporaryDirectory") as stage:
            with self.assertRaisesRegex(R.ReleaseError, "symlink output parent"):
                SITE.main(self.package, VERSION, alias / "redirect/site", preview=True)
            stage.assert_not_called()
        self.assertEqual(list(outside.iterdir()), [])

    def test_existing_output_is_not_overwritten(self):
        self.out.mkdir()
        sentinel = self.out / "keep.txt"
        sentinel.write_text("keep")
        with self.assertRaisesRegex(R.ReleaseError, "new or empty"):
            self.build()
        self.assertEqual(sentinel.read_text(), "keep")

    def test_empty_existing_output_can_be_filled(self):
        self.out.mkdir()
        self.build()
        self.assertTrue((self.out / "emulator/index.html").is_file())

    def test_emulator_symlink_and_stale_version_are_rejected(self):
        target = self.emulator / "worklet.js"
        original = target.read_bytes()
        target.unlink()
        target.symlink_to(self.root / "web/emu/worklet.js")
        with self.assertRaisesRegex(R.ReleaseError, "symlink"):
            self.build()
        target.unlink()
        target.write_bytes(original)
        path = self.emulator / "index.html"
        path.write_text(path.read_text().replace("v0.1.0</small>", "v0.0.9</small>"))
        with self.assertRaisesRegex(R.ReleaseError, "version mismatch"):
            self.build()

    def test_wrong_emulator_support_file_is_rejected(self):
        (self.emulator / "fonts/OFL.txt").write_text("TEST ONLY wrong font notice")
        with self.assertRaisesRegex(R.ReleaseError, "support files differ"):
            self.build()

    def test_check_only_does_not_create_output(self):
        self.build(check_only=True)
        self.assertFalse(self.out.exists())

    def test_pages_workflow_pins_tag_and_verifies_before_build(self):
        text = (ROOT / ".github/workflows/pages.yml").read_text()
        self.assertIn("ref: ${{ steps.release.outputs.tag }}", text)
        self.assertIn("github.event.repository.private == false", text)
        self.assertIn("--release-dir", text)
        self.assertIn("--check-only", text)
        self.assertIn("--emulator build/emu", text)
        self.assertLess(text.index("--check-only"), text.index("build.generate()"))
        self.assertNotIn("sh build.sh", text)
        self.assertNotIn("fw-AC79_AIoT_SDK.git", text)
        # Check only syntax: no workflow scripts or network/device operations execute.
        for block in workflow_blocks().values():
            result = subprocess.run(["bash", "-n"], input=block, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)


class PagesBehaviorTest(unittest.TestCase):
    def download_inventory(self, manifest):
        script = workflow_python('Download the complete selected release inventory')
        with tempfile.TemporaryDirectory(prefix='caravel-pages-download-') as temp:
            commands = []
            def download(command, *, check):
                self.assertTrue(check)
                commands.append(command)
                if len(commands) == 1:
                    (Path(temp) / 'release/SHA256SUMS').write_bytes(manifest)
            with mock.patch.dict(os.environ, {'RUNNER_TEMP': temp, 'RELEASE_TAG': 'v0.7.0',
                                             'GH_REPO': 'fixture/caravel'}), \
                    mock.patch.object(subprocess, 'run', side_effect=download):
                exec(script, {})
            return commands

    def test_pages_downloads_only_manifest_members_and_keeps_extra_assets_out(self):
        names = ['caravel-0.7.0.fwsc', 'LICENSE', 'caravel-0.7.0-source.tar.gz']
        commands = self.download_inventory(b''.join(b'0' * 64 + b'  ' + n.encode() + b'\n' for n in names))
        self.assertEqual(len(commands), 2)
        self.assertEqual(commands[0][-2:], ['--pattern', 'SHA256SUMS'])
        self.assertEqual(commands[1][-6:], [arg for n in names for arg in ('--pattern', n)])
        self.assertNotIn('Caravel-0.7.0-Emulator.html', commands[1])
        self.assertNotIn('Caravel-0.7.0-FM1-Testpaket.zip', commands[1])

    def test_pages_rejects_unsafe_empty_duplicate_and_self_referencing_manifests(self):
        for manifest in [b'', b'not a checksum\n', *[
                b'0' * 64 + b'  ' + name + b'\n' for name in
                [b'../secret', b'/tmp/file', b'file*', b'file?', b'file[0]', b'SHA256SUMS']],
                (b'0' * 64 + b'  LICENSE\n') * 2]:
            with self.subTest(manifest=manifest), self.assertRaises(SystemExit):
                self.download_inventory(manifest)

    def test_pages_uses_the_complete_070_profile_without_changing_older_releases(self):
        block = workflow_blocks()['Match the selected engine and effects profile']
        for version, expected in [('0.1.0', []), ('0.6.0', ['CARAVEL_REVERB_MINIMAL', 'FELUCCA_SLICE']),
                                  ('0.7.0', ['CARAVEL_ACTION_BANKS', 'FELUCCA_RANDOM_MORPH',
                                             'CARAVEL_REVERB_MINIMAL', 'CARAVEL_STASIS_EXPAND', 'FELUCCA_SLICE']),
                                  ('0.7.1', ['CARAVEL_ACTION_BANKS', 'FELUCCA_RANDOM_MORPH',
                                             'CARAVEL_REVERB_MINIMAL', 'CARAVEL_STASIS_EXPAND', 'FELUCCA_SLICE']),
                                  ('0.7.2', ['CARAVEL_ACTION_BANKS', 'FELUCCA_RANDOM_MORPH',
                                             'CARAVEL_REVERB_MINIMAL', 'CARAVEL_STASIS_EXPAND', 'FELUCCA_SLICE'])]:
            with self.subTest(version=version), tempfile.TemporaryDirectory() as temp:
                env_file = Path(temp) / 'env'
                env_file.touch()
                result = subprocess.run(['bash', '-e'], input=block, text=True, capture_output=True,
                                        cwd=ROOT, env={**os.environ, 'VER': version, 'GITHUB_ENV': str(env_file)})
                self.assertEqual(result.returncode, 0, result.stderr)
                text = env_file.read_text()
                for flag in expected:
                    self.assertIn(flag, text)
                if not expected:
                    self.assertEqual(text, '')
                if version == '0.6.0':
                    self.assertNotIn('CARAVEL_ACTION_BANKS', text)

    def release_gate(self, metadata, *, version=VERSION, event_tag="", error=None):
        script = workflow_python("Select and validate the exact release tag")
        with tempfile.TemporaryDirectory(prefix="caravel-pages-gate-") as temp:
            env_file, output_file = Path(temp) / "env", Path(temp) / "output"
            env_file.write_text("")
            output_file.write_text("")
            env = {"INPUT_VERSION": version, "TAG": event_tag, "GH_REPO": "fixture/caravel",
                   "GITHUB_ENV": str(env_file), "GITHUB_OUTPUT": str(output_file)}
            result = subprocess.CompletedProcess([], 0, json.dumps(metadata), "")
            with mock.patch.dict(os.environ, env), mock.patch.object(
                    subprocess, "run", return_value=result, side_effect=error) as gh:
                try:
                    exec(script, {})
                except BaseException:
                    self.assertEqual(env_file.read_text(), "")
                    self.assertEqual(output_file.read_text(), "")
                    raise
                finally:
                    gh.assert_called_once_with(
                        ["gh", "release", "view", "v0.1.0", "--repo", "fixture/caravel",
                         "--json", "tagName,isDraft,isPrerelease,publishedAt"],
                        check=True, capture_output=True, text=True)
            return env_file.read_text(), output_file.read_text()

    @staticmethod
    def published_release():
        return {"tagName": "v0.1.0", "isDraft": False, "isPrerelease": False,
                "publishedAt": "2026-10-19T00:00:00Z"}

    def test_release_gate_accepts_published_exact_tag_for_manual_and_release_events(self):
        for version, event_tag in ((VERSION, ""), ("", "v0.1.0")):
            with self.subTest(version=version, event_tag=event_tag):
                env, output = self.release_gate(self.published_release(), version=version, event_tag=event_tag)
                self.assertEqual(output, "tag=v0.1.0\n")
                self.assertEqual(env, "VER=0.1.0\nRELEASE_TAG=v0.1.0\nEMU_VERSION=v0.1.0\n")
        text = (ROOT / ".github/workflows/pages.yml").read_text()
        query = text.index('["gh", "release", "view"')
        self.assertLess(query, text.index("uses: actions/checkout"))
        self.assertLess(query, text.index("- name: Download the complete selected release inventory"))

    def test_release_gate_rejects_draft_unpublished_prerelease_and_wrong_tag(self):
        for field, value in (("isDraft", True), ("isPrerelease", True), ("publishedAt", None),
                             ("publishedAt", ""), ("tagName", "v0.2.0"), ("tagName", "0.1.0")):
            with self.subTest(field=field, value=value):
                metadata = {**self.published_release(), field: value}
                with self.assertRaisesRegex(SystemExit, "published, non-draft and non-prerelease"):
                    self.release_gate(metadata)
        for field in self.published_release():
            with self.subTest(missing=field):
                metadata = self.published_release()
                del metadata[field]
                with self.assertRaises(SystemExit):
                    self.release_gate(metadata)

    def test_release_gate_fails_closed_when_gh_cannot_find_release(self):
        error = subprocess.CalledProcessError(1, ["gh", "release", "view"], stderr="release not found")
        with self.assertRaises(subprocess.CalledProcessError):
            self.release_gate(self.published_release(), error=error)

    def test_header_step_prints_all_mmio_diagnostics_and_fails(self):
        script = workflow_python("Generate headers from the selected source (no target binary rebuild)")
        for diagnostics in ([], ["src/test.c:1: MMIO violation", "loader/test.c:2: another violation"]):
            with self.subTest(diagnostics=diagnostics):
                build = mock.Mock()
                build.mmio_check.return_value = diagnostics
                output = io.StringIO()
                with mock.patch.dict(sys.modules, {"build": build}), contextlib.redirect_stderr(output), \
                        mock.patch.object(sys, "path", sys.path.copy()):
                    if diagnostics:
                        with self.assertRaisesRegex(SystemExit, "MMIO checks failed"):
                            exec(script, {})
                    else:
                        exec(script, {})
                build.generate.assert_called_once_with()
                build.mmio_check.assert_called_once_with()
                self.assertEqual(output.getvalue(), "".join(d + "\n" for d in diagnostics))


if __name__ == "__main__":
    unittest.main(verbosity=2)
