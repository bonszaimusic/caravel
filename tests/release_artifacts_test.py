#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Offline, stdlib-only release tests. ALL firmware/SDK bytes are TEST-ONLY.

Fixture commits occur only in disposable repositories, never in the checkout.
No synthetic artifact is placed in the project's build directory.
Run: python -B tests/release_artifacts_test.py
"""
import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import build as B
import fm1pkg_make as P
import release_artifacts as R

VERSION = "0.1.0"


def git(root, *args):
    env = {**os.environ, "GIT_EDITOR": "true", "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": os.devnull}
    return subprocess.run(["git", "--no-pager", "-c", "user.name=Release Test Fixture",
                           "-c", "user.email=fixture@example.invalid", "-c", "commit.gpgsign=false",
                           "-c", "protocol.file.allow=always", "-C", str(root), *args],
                          env=env, check=True, capture_output=True).stdout


def put(root, name, data=b"TEST ONLY - not production source or firmware\n"):
    p = root / name
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(data)
    return p


def commit(root):
    git(root, "add", "--all")
    git(root, "commit", "-qm", "TEST ONLY fixture")


class ReleaseArtifactsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace = tempfile.TemporaryDirectory(prefix="caravel-release-tests-")
        cls.base = Path(cls.workspace.name)
        engine = cls.base / "engine"
        engine.mkdir()
        git(engine, "init", "-q")
        put(engine, "LICENSE")
        put(engine, "src/felucca_engine.h")
        put(engine, "test/run.sh", b"#!/bin/sh\nexit 0\n").chmod(0o755)
        commit(engine)
        cls.template = cls.base / "template"
        cls.template.mkdir()
        git(cls.template, "init", "-q")
        for name in (*R.DOCS, *("LICENSES/" + n for n in R.LICENSES),
                     "assets/samples-cc0/ATTRIBUTION.txt", "tools/build.py",
                     "tools/fm1pkg_make.py", "tools/release_artifacts.py", "build.sh"):
            put(cls.template, name)
        put(cls.template, "firmware/src/felucca.c", b'#define FELUCCA_VERSION "v0.1.0"\n')
        put(cls.template, ".gitignore", b"build/\n__pycache__/\n")
        put(cls.template, "empty.txt", b"")
        git(cls.template, "submodule", "add", "-q", str(engine), "firmware/engines")
        commit(cls.template)
        cls.sdk = cls.base / "sdk"
        cls.sdk_bytes = {n: ("TEST ONLY SDK " + n).encode() for n in R.SDK_SHA256}
        for name, data in cls.sdk_bytes.items():
            put(cls.sdk, "cpu/wl82/tools/" + name, data)

    @classmethod
    def tearDownClass(cls):
        cls.workspace.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(dir=self.base)
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name) / "source"
        shutil.copytree(self.template, self.root)
        hashes = {n: R.sha(d) for n, d in self.sdk_bytes.items()}
        self.addCleanup(mock.patch.stopall)
        mock.patch.object(R, "SDK_SHA256", hashes).start()
        mock.patch.object(B, "SDK_SHA256", hashes).start()
        self.app = b"TEST ONLY synthetic app\0v0.1.0\0FM-1_999\0" + bytes(128)
        self.ota = B.ldr_wrap(b"TEST ONLY loader\0FELUCCA-LOADER-1\0" + bytes(256))
        put(self.root, "build/felucca.bin", self.app)
        put(self.root, "build/loader/ota.bin", self.ota)
        self.write_package()
        self.snapshot = R.source_snapshot(self.root)
        R.record_build(self.root, VERSION, self.snapshot, self.sdk)

    def write_package(self, app=None, ota=None, product="FM-1_999"):
        with mock.patch.object(P, "SDK", self.sdk):
            data = P.ufw(P.flash_image(self.app if app is None else app, P.KEY),
                         self.ota if ota is None else ota, product)
        return put(self.root, "build/caravel-0.1.0.fwsc", data)

    def assemble(self):
        return R.release(self.root, VERSION, sdk=self.sdk)

    def verify(self):
        return R.release(self.root, VERSION, verify_only=True, sdk=self.sdk)

    def test_round_trip_inventory_checksums_and_determinism(self):
        directory = self.assemble()
        self.assertEqual(directory, self.verify())
        before = {p.relative_to(directory).as_posix(): p.read_bytes()
                  for p in directory.rglob("*") if p.is_file()}
        self.assemble()
        after = {p.relative_to(directory).as_posix(): p.read_bytes()
                 for p in directory.rglob("*") if p.is_file()}
        self.assertEqual(before, after)
        self.assertTrue(set(R.DOCS).issubset(before))
        self.assertIn("ATTRIBUTION.txt", before)
        self.assertIn("release-build.json", before)
        self.assertTrue(set(R.LICENSES).issubset(before))
        self.assertTrue(all("/" not in name for name in before))
        self.assertFalse((directory / "LICENSES").exists())
        self.assertEqual(len(before) - 1, len(before["SHA256SUMS"].splitlines()))
        with tarfile.open(directory / "caravel-0.1.0-source.tar.gz") as archive:
            files = {m.name: m for m in archive}
            self.assertIn("caravel-0.1.0/firmware/engines/src/felucca_engine.h", files)
            for name in R.LICENSES:
                member = archive.extractfile("caravel-0.1.0/LICENSES/" + name)
                self.assertEqual(member.read(), before[name])
            self.assertEqual(files["caravel-0.1.0/firmware/engines/test/run.sh"].mode, 0o755)
            self.assertEqual(files["caravel-0.1.0/empty.txt"].size, 0)
            self.assertFalse(any(".git/" in n or "/build/" in n for n in files))

    def test_flat_downloaded_publication_layout(self):
        directory = self.assemble()
        download = Path(self.tmp.name) / "github-download"
        download.mkdir()
        for attachment in directory.iterdir():
            self.assertTrue(attachment.is_file())
            shutil.copyfile(attachment, download / attachment.name)
        hashes = R.verify_checksums(download)
        self.assertEqual(set(hashes), {p.name for p in download.iterdir()} - {"SHA256SUMS"})
        self.assertTrue(set(R.LICENSES).issubset(hashes))
        # Model Pages only AFTER verification, retaining its existing licence URLs.
        site = Path(self.tmp.name) / "site/firmware/LICENSES"
        site.mkdir(parents=True)
        for name in R.license_artifacts(self.snapshot[0]):
            shutil.copyfile(download / name, site / name)
            self.assertEqual(R.sha((site / name).read_bytes()), hashes[name])
        manifest = (download / "SHA256SUMS").read_bytes()
        (download / "SHA256SUMS").write_bytes(manifest.replace(b"  Apache-2.0.txt", b"  LICENSES/Apache-2.0.txt"))
        with self.assertRaises(R.ReleaseError):
            R.verify_checksums(download)

    def test_flat_licence_basename_collisions_are_rejected(self):
        files = dict(self.snapshot[0])
        files["LICENSES/nested/Apache-2.0.txt"] = (0o644, b"duplicate basename")
        with self.assertRaisesRegex(R.ReleaseError, "duplicate licence attachment basename"):
            R.license_artifacts(files)
        put(self.root, "LICENSES/NOTICE", b"colliding licence")
        commit(self.root)
        R.record_build(self.root, VERSION, R.source_snapshot(self.root), self.sdk)
        with self.assertRaisesRegex(R.ReleaseError, "licence attachment collides"):
            self.assemble()

    def test_checksum_only_verifier_rejects_changed_missing_extra_and_nonflat_assets(self):
        directory = self.assemble()
        notice = directory / "NOTICE"
        original = notice.read_bytes()
        notice.write_bytes(b"modified")
        with self.assertRaisesRegex(R.ReleaseError, "SHA256SUMS mismatch"):
            R.verify_checksums(directory)
        notice.unlink()
        with self.assertRaisesRegex(R.ReleaseError, "missing required file"):
            R.verify_checksums(directory)
        notice.write_bytes(original)
        extra = put(directory, "extra.txt")
        with self.assertRaisesRegex(R.ReleaseError, "inventory differs"):
            R.verify_checksums(directory)
        extra.unlink()
        (directory / "LICENSES").mkdir()
        with self.assertRaisesRegex(R.ReleaseError, "non-flat/unsafe"):
            R.verify_checksums(directory)

    def test_sdk_reference_metadata_and_workflow_commit_pin(self):
        record = json.loads((self.assemble() / "release-build.json").read_bytes())
        self.assertEqual(record["sdk_reference"], R.SDK_REFERENCE)
        workflow = (ROOT / ".github/workflows/release.yml").read_text()
        self.assertIn(R.SDK_REFERENCE["repository"], workflow)
        self.assertIn(R.SDK_REFERENCE["tag"], workflow)
        self.assertIn("rev-parse HEAD | grep -Fx '" + R.SDK_REFERENCE["commit"] + "'", workflow)
        self.assertIn('"build/release-$VER"/*', workflow)
        self.assertNotIn('/LICENSES/*', workflow)

    def test_missing_built_inputs_and_build_record(self):
        for name in ("felucca.bin", "loader/ota.bin", "caravel-0.1.0.fwsc", "release-0.1.0.build.json"):
            with self.subTest(name=name):
                p = self.root / "build" / name
                old = p.read_bytes()
                p.unlink()
                with self.assertRaisesRegex(R.ReleaseError, "missing required file"):
                    self.assemble()
                p.write_bytes(old)

    def test_missing_shipped_notices_source_loader_or_checksum(self):
        directory = self.assemble()
        for name in (*R.DOCS, "ATTRIBUTION.txt", "Apache-2.0.txt", "SHA256SUMS",
                     "caravel-0.1.0-source.tar.gz", "caravel-0.1.0-loader.bin"):
            with self.subTest(name=name):
                p = directory / name
                old = p.read_bytes()
                p.unlink()
                with self.assertRaisesRegex(R.ReleaseError, "inventory mismatch"):
                    self.verify()
                p.write_bytes(old)

    def test_missing_required_tracked_notice_or_source(self):
        for name in ("NOTICE", "CREDITS.md", "LICENSES/MIT-Rings.txt", "assets/samples-cc0/ATTRIBUTION.txt", "tools/build.py"):
            with self.subTest(name=name):
                p = self.root / name
                old = p.read_bytes()
                p.unlink()
                commit(self.root)
                with self.assertRaisesRegex(R.ReleaseError, "missing/empty required tracked"):
                    R.source_snapshot(self.root)
                put(self.root, name, old)
                commit(self.root)

    def test_dirty_source_and_untracked_source(self):
        p = self.root / "NOTICE"
        old = p.read_bytes()
        p.write_bytes(old + b"dirty")
        with self.assertRaisesRegex(R.ReleaseError, "dirty source"):
            self.assemble()
        p.write_bytes(old)
        put(self.root, "unreviewed.txt")
        with self.assertRaisesRegex(R.ReleaseError, "dirty source"):
            self.assemble()

    def test_dirty_submodule(self):
        put(self.root, "firmware/engines/src/felucca_engine.h", b"dirty")
        with self.assertRaisesRegex(R.ReleaseError, "dirty source/submodule"):
            self.assemble()

    def test_uninitialized_submodule(self):
        shutil.rmtree(self.root / "firmware/engines")
        (self.root / "firmware/engines").mkdir()
        with self.assertRaisesRegex(R.ReleaseError, "uninitialized submodule"):
            R.source_snapshot(self.root)

    def test_submodule_pin_mismatch(self):
        put(self.root, "firmware/engines/new.txt")
        commit(self.root / "firmware/engines")
        with self.assertRaises(R.ReleaseError):
            self.assemble()

    def test_recursive_submodule_is_included(self):
        child = self.base / "engine"
        git(self.root / "firmware/engines", "submodule", "add", "-q", str(child), "deps/engine")
        commit(self.root / "firmware/engines")
        commit(self.root)
        snapshot = R.source_snapshot(self.root)
        self.assertIn("firmware/engines/deps/engine/src/felucca_engine.h", snapshot[0])
        self.assertIn("firmware/engines/deps/engine", snapshot[1])

    def test_stale_build_record_after_source_commit(self):
        put(self.root, "NOTICE", b"reviewed new notice")
        commit(self.root)
        with self.assertRaisesRegex(R.ReleaseError, "build record mismatch"):
            self.assemble()

    def test_changed_sdk_hash_and_missing_sdk(self):
        with mock.patch.object(R, "SDK_SHA256", {**R.SDK_SHA256, "uboot.boot": "0" * 64}):
            with self.assertRaisesRegex(R.ReleaseError, "SDK hash mismatch"):
                self.assemble()
        with self.assertRaises(SystemExit):
            R.sdk_hashes(self.root / "nonexistent-sdk")

    def test_mismatched_app_version(self):
        put(self.root, "build/felucca.bin", self.app.replace(b"v0.1.0", b"v0.2.0"))
        with self.assertRaisesRegex(R.ReleaseError, "app version/identity mismatch"):
            self.assemble()

    def test_mismatched_package_identity(self):
        self.write_package(product="FM-1_900")
        with self.assertRaisesRegex(R.ReleaseError, "package identity mismatch"):
            self.assemble()

    def test_mismatched_package_app_loader_or_crc(self):
        for app, ota in ((self.app + b"different", self.ota), (self.app, self.ota + b"different")):
            self.write_package(app=app, ota=ota)
            with self.assertRaisesRegex(R.ReleaseError, "package contents/CRC mismatch"):
                self.assemble()
        p = self.write_package()
        data = bytearray(p.read_bytes())
        data[-1] ^= 1
        p.write_bytes(data)
        with self.assertRaisesRegex(R.ReleaseError, "package contents/CRC mismatch"):
            self.assemble()

    def test_invalid_loader(self):
        p = self.root / "build/loader/ota.bin"
        p.write_bytes(self.ota[:-1])
        with self.assertRaisesRegex(R.ReleaseError, "loader"):
            self.assemble()

    def test_release_version_override_is_recorded(self):
        put(self.root, "firmware/src/felucca.c", b'#define FELUCCA_VERSION "v0.0.9"\n')
        commit(self.root)
        R.record_build(self.root, VERSION, R.source_snapshot(self.root), self.sdk)
        record = json.loads((self.assemble() / "release-build.json").read_bytes())
        self.assertEqual(record["source_default_version"], "v0.0.9")
        self.assertEqual(record["firmware_version_override"], "v0.1.0")

    def test_existing_uppercase_suffix_contract_uses_lowercase_firmware_override(self):
        version = "0.1.0-RC1"
        self.app = self.app.replace(b"v0.1.0", b"v0.1.0-rc1")
        put(self.root, "build/felucca.bin", self.app)
        package = self.write_package().read_bytes()
        put(self.root, f"build/caravel-{version}.fwsc", package)
        R.record_build(self.root, version, self.snapshot, self.sdk)
        directory = R.release(self.root, version, sdk=self.sdk)
        R.release(self.root, version, verify_only=True, sdk=self.sdk)
        record = json.loads((directory / "release-build.json").read_bytes())
        self.assertEqual(record["firmware_version_override"], "v0.1.0-rc1")

    def test_checksum_corruption_omission_duplicates_and_path_escape(self):
        directory = self.assemble()
        p = directory / "SHA256SUMS"
        old = p.read_bytes()
        for data in (old.replace(old[:64], b"0" * 64, 1), old.split(b"\n", 1)[1],
                     old + old.splitlines(keepends=True)[0], old + b"0" * 64 + b"  ../outside\n"):
            with self.subTest(data=data[:80]):
                p.write_bytes(data)
                with self.assertRaisesRegex(R.ReleaseError, "SHA256SUMS mismatch"):
                    self.verify()
        p.write_bytes(old)
        (directory / "NOTICE").write_bytes(b"wrong notice")
        with self.assertRaisesRegex(R.ReleaseError, "artifact mismatch: NOTICE"):
            self.verify()

    def test_source_tar_missing_corrupt_duplicate_escape_and_symlink(self):
        directory = self.assemble()
        path = directory / "caravel-0.1.0-source.tar.gz"
        files = self.snapshot[0]
        for kind in ("missing", "corrupt", "duplicate", "escape", "symlink", "mode"):
            with self.subTest(kind=kind):
                with tarfile.open(path, "w:gz") as archive:
                    for i, (name, (mode, data)) in enumerate(sorted(files.items())):
                        if kind == "missing" and i == 0:
                            continue
                        member = tarfile.TarInfo("caravel-0.1.0/" + name)
                        member.mode, member.size = mode, len(data)
                        if i == 0:
                            if kind == "escape":
                                member.name = "caravel-0.1.0/../../outside"
                            if kind == "symlink":
                                member.type, member.linkname, member.size = tarfile.SYMTYPE, "../../outside", 0
                            if kind == "mode":
                                member.mode = 0o777
                            if kind == "corrupt":
                                data = bytes([data[0] ^ 1]) + data[1:]
                        archive.addfile(member, io.BytesIO(data))
                        if kind == "duplicate" and i == 0:
                            archive.addfile(member, io.BytesIO(data))
                with self.assertRaises(R.ReleaseError):
                    self.verify()

    def test_symlink_and_extra_release_assets(self):
        directory = self.assemble()
        p = directory / "NOTICE"
        p.unlink()
        p.symlink_to(self.root / "NOTICE")
        with self.assertRaisesRegex(R.ReleaseError, "symlink"):
            self.verify()
        p.unlink()
        put(directory, "NOTICE", (self.root / "NOTICE").read_bytes())
        put(directory, "unexpected.txt")
        with self.assertRaisesRegex(R.ReleaseError, "extra="):
            self.verify()

    def test_unsafe_source_symlink_and_private_path(self):
        (self.root / "escape").symlink_to(self.root / "NOTICE")
        commit(self.root)
        with self.assertRaisesRegex(R.ReleaseError, "unsupported source mode"):
            R.source_snapshot(self.root)
        (self.root / "escape").unlink()
        put(self.root, "private/secret.txt")
        commit(self.root)
        with self.assertRaisesRegex(R.ReleaseError, "excluded source path"):
            R.source_snapshot(self.root)

    def test_symlink_build_directory_is_rejected(self):
        build = self.root / "build"
        moved = Path(self.tmp.name) / "outside-build"
        build.rename(moved)
        build.symlink_to(moved, target_is_directory=True)
        with self.assertRaisesRegex(R.ReleaseError, "symlink output path"):
            self.assemble()

    def test_build_record_metadata_is_verified(self):
        path = self.root / "build/release-0.1.0.build.json"
        record = json.loads(path.read_bytes())
        for field, value in (("source_default_version", "v9.9.9"), ("product", "FM-1_903"),
                             ("firmware_version_override", "v9.9.9"),
                             ("sdk_reference", {**R.SDK_REFERENCE, "commit": "0" * 40})):
            with self.subTest(field=field):
                path.write_bytes(R.json_bytes({**record, field: value}))
                with self.assertRaisesRegex(R.ReleaseError, "build record mismatch"):
                    self.assemble()
        path.write_bytes(R.json_bytes(record))

    def test_version_and_path_validation(self):
        for version in ("../0.1.0", "0.1.0/escape", "10.1.0", "v0.1.0"):
            with self.assertRaises(R.ReleaseError):
                R.identity(version)
        for path in ("/absolute", "../escape", "a/../b", "a//b", "a\\b", "a\nb"):
            with self.assertRaises(R.ReleaseError):
                R.safe_path(path)
        self.assertEqual(R.identity("1.2.3-rc1"), "FM-1_999")
        # Firmware identity is independent of the release version.
        for version in ("0.3.4", "0.9.9", "1.0.0", "9.9.9-rc2"):
            self.assertEqual(R.identity(version), "FM-1_999")

    def build_context(self, release=True, binary_version=None, extra_args=()):
        stack = contextlib.ExitStack()
        for name, value in (("SRC", self.root), ("FW", self.root / "firmware"), ("OUT", self.root / "build"),
                            ("GEN", self.root / "build/gen"), ("LDR", self.root / "build/loader")):
            stack.enter_context(mock.patch.object(B, name, value))
        mode_args = (["--binary-version", binary_version] if binary_version is not None else
                     ["--release", VERSION] if release else [])
        args = ["build.py", "--sdk", str(self.sdk), *mode_args, *extra_args]
        stack.enter_context(mock.patch.object(sys, "argv", args))
        stack.enter_context(mock.patch.object(B, "generate"))
        stack.enter_context(mock.patch.object(B, "build_loader", return_value=self.ota))
        stack.enter_context(mock.patch.object(B, "build_app", return_value=(self.app, {}, "", "")))
        stack.enter_context(mock.patch.object(B, "check", return_value=([], [])))
        stack.enter_context(mock.patch.object(B, "mmio_check", return_value=[]))
        stack.output = stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
        return stack

    def test_build_preflight_rejects_output_symlinks_before_any_mutation(self):
        cases = (
            ("build", True), ("build/gen", True), ("build/loader", True),
            ("build/release-0.1.0", True), ("build/caravel-0.1.0.fwsc", False),
            ("build/felucca.bin", False), ("build/loader/ota.bin", False),
            ("build/loader/loader.elf", False), ("build/gen/ui_fonts.h", False),
            ("build/release-0.1.0.build.json", False), ("build/ATTRIBUTION.txt", False),
        )
        for index, (name, is_directory) in enumerate(cases):
            with self.subTest(path=name):
                target = self.root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                saved = Path(self.tmp.name) / f"saved-output-{index}"
                if target.exists():
                    target.rename(saved)
                outside = Path(self.tmp.name) / f"outside-{index}"
                if is_directory:
                    outside.mkdir()
                    sentinel = put(outside, "sentinel.bin", b"DO NOT CHANGE outside sentinel")
                else:
                    sentinel = put(outside.parent, outside.name, b"DO NOT CHANGE outside sentinel")
                target.symlink_to(outside, target_is_directory=is_directory)
                record = self.root / "build/release-0.1.0.build.json"
                record_before = record.read_bytes() if record.is_file() else None
                try:
                    with self.build_context(), mock.patch.object(B, "tc") as tc, \
                            mock.patch.object(R, "record_build") as record_build, \
                            mock.patch.object(R, "release") as release:
                        with self.assertRaisesRegex(SystemExit, "release: symlink output path"):
                            B.main()
                        for function in (B.generate, B.build_loader, B.build_app, tc, record_build, release):
                            function.assert_not_called()
                    self.assertEqual(sentinel.read_bytes(), b"DO NOT CHANGE outside sentinel")
                    self.assertTrue(target.is_symlink())
                    if record_before is not None:
                        self.assertEqual(record.read_bytes(), record_before)
                finally:
                    target.unlink()
                    if saved.exists():
                        saved.rename(target)

    def test_build_preflight_never_calls_generation_or_compilation(self):
        sentinel = put(Path(self.tmp.name), "outside-package", b"unchanged")
        package = self.root / "build/caravel-0.1.0.fwsc"
        package.unlink()
        package.symlink_to(sentinel)
        with self.build_context():
            with self.assertRaisesRegex(SystemExit, "release: symlink output path"):
                B.main()
            for function in (B.generate, B.build_loader, B.build_app, B.check, B.mmio_check):
                function.assert_not_called()
        self.assertEqual(sentinel.read_bytes(), b"unchanged")

    def test_build_preflight_rejects_dangling_symlinks_and_hardlinks(self):
        output = self.root / "build/gen/nested/table.h"
        output.parent.mkdir(parents=True)
        missing = Path(self.tmp.name) / "outside-missing"
        output.symlink_to(missing)
        with self.build_context(), self.assertRaisesRegex(SystemExit, "symlink output path"):
            B.main()
        self.assertFalse(missing.exists())
        output.unlink()
        sentinel = put(Path(self.tmp.name), "outside-hardlink", b"unchanged")
        os.link(sentinel, output)
        with self.build_context(), self.assertRaisesRegex(SystemExit, "hard-linked output file"):
            B.main()
        self.assertEqual(sentinel.read_bytes(), b"unchanged")

    def test_binary_version_rebuilds_without_git_or_release_assembly(self):
        expected_package = (self.root / "build/caravel-0.1.0.fwsc").read_bytes()
        shutil.rmtree(self.root / ".git")
        (self.root / "firmware/engines/.git").unlink()
        shutil.rmtree(self.root / "build")

        def app():
            put(self.root, "build/felucca.bin", self.app)
            return self.app, {}, "", ""

        def loader():
            put(self.root, "build/loader/ota.bin", self.ota)
            return self.ota

        with self.build_context(binary_version=VERSION) as context, \
                mock.patch.object(B, "build_app", side_effect=app), \
                mock.patch.object(B, "build_loader", side_effect=loader), \
                mock.patch.object(R, "git") as git_call, \
                mock.patch.object(R, "source_snapshot") as snapshot, \
                mock.patch.object(R, "record_build") as record, mock.patch.object(R, "release") as release:
            self.assertEqual(B.main(), 0)
            for function in (git_call, snapshot, record, release):
                function.assert_not_called()
            for function in (B.generate, B.build_app, B.build_loader, B.check, B.mmio_check):
                function.assert_called_once()
            self.assertEqual(B.PRODUCT, "FM-1_999")
            self.assertEqual(B.VERSION, "v0.1.0")
            self.assertIn("binary build only: not a verified release folder", context.output.getvalue())
        self.assertEqual((self.root / "build/caravel-0.1.0.fwsc").read_bytes(), expected_package)
        self.assertFalse(any((self.root / "build").glob("release-*")))
        self.assertFalse(any((self.root / "build").rglob("*.tar.gz")))
        with self.assertRaisesRegex(R.ReleaseError, "git"):
            R.release(self.root, VERSION, sdk=self.sdk)

    def test_binary_version_and_release_use_identical_compile_flags_and_package(self):
        actual_build_app = B.build_app
        commands, packages = [], []

        def capture_compile_arguments():
            with mock.patch.object(B, "tc_all", side_effect=RuntimeError("compile probe")) as tc_all:
                with self.assertRaisesRegex(RuntimeError, "compile probe"):
                    actual_build_app()
                commands.append(tc_all.call_args.args)
            return self.app, {}, "", ""

        for binary_version in (None, VERSION):
            with self.build_context(binary_version=binary_version), \
                    mock.patch.object(B, "build_app", side_effect=capture_compile_arguments):
                self.assertEqual(B.main(), 0)
            packages.append((self.root / "build/caravel-0.1.0.fwsc").read_bytes())
        self.assertEqual(commands[0], commands[1])
        self.assertIn('-DFELUCCA_ID="FM-1_999"', commands[1][-1])
        self.assertIn('-DFELUCCA_VERSION="v0.1.0"', commands[1][-1])
        self.assertEqual(packages[0], packages[1])

    def test_plain_build_says_dev_on_the_device_and_a_release_does_not(self):
        with self.build_context(release=False):
            self.assertEqual(B.main(), 0)
            self.assertEqual(B.PRODUCT, "FM-1_900")
            self.assertEqual(B.VERSION, "v0.1.0-dev")           # the source's v0.1.0 and "-dev"
        with self.build_context(release=False, binary_version=VERSION):
            self.assertEqual(B.main(), 0)
            self.assertEqual(B.VERSION, "v0.1.0")                # a versioned build: no suffix
        put(self.root, "firmware/src/felucca.c", b'#define FELUCCA_VERSION "v0.1.0-dev"\n')
        with self.build_context(release=False):
            self.assertEqual(B.main(), 0)
            self.assertIsNone(B.VERSION)                         # (an already marked source: its own macro, not twice)
        put(self.root, "firmware/src/felucca.c", b'#define FELUCCA_VERSION "v0.1.0"\n')

    def test_binary_version_suffix_uses_release_identity_and_filename(self):
        with self.build_context(binary_version="1.2.3-RC1"), \
                mock.patch.object(R, "source_snapshot") as snapshot, \
                mock.patch.object(R, "record_build") as record, mock.patch.object(R, "release") as release:
            self.assertEqual(B.main(), 0)
            self.assertEqual(B.PRODUCT, "FM-1_999")
            self.assertEqual(B.VERSION, "v1.2.3-rc1")
            for function in (snapshot, record, release):
                function.assert_not_called()
        package = (self.root / "build/caravel-1.2.3-RC1.fwsc").read_bytes()
        self.assertEqual(R.fm1_install.product_of(package), "FM-1_999")

    def test_binary_version_argument_rejection_precedes_build(self):
        cases = (
            (True, ["--binary-version", VERSION], None),
            (False, ["--binary-version"], None),
            (False, ["--binary-version", "../0.1.0"], "binary-version: version"),
            (False, ["--binary-version", "10.1.0"], "binary-version: version"),
            (False, ["--binary-version", ""], "binary-version: version"),
            (False, ["--release", ""], "release: version"),
        )
        for release_mode, extra_args, message in cases:
            with self.subTest(args=extra_args), \
                    self.build_context(release=release_mode, extra_args=extra_args), \
                    contextlib.redirect_stderr(io.StringIO()), \
                    mock.patch.object(R, "guard_output_tree") as guard, \
                    mock.patch.object(R, "source_snapshot") as snapshot:
                with self.assertRaises(SystemExit) as error:
                    B.main()
                if message:
                    self.assertIn(message, str(error.exception))
                else:
                    self.assertEqual(error.exception.code, 2)
                for function in (guard, snapshot, B.generate, B.build_loader, B.build_app):
                    function.assert_not_called()

    def test_binary_version_sdk_mismatch_fails_before_build_without_touching_record(self):
        record_path = self.root / "build/release-0.1.0.build.json"
        before = record_path.read_bytes()
        wrong = {**R.SDK_SHA256, "uboot.boot": "0" * 64}
        with self.build_context(binary_version=VERSION), mock.patch.object(B, "SDK_SHA256", wrong), \
                mock.patch.object(R, "source_snapshot") as snapshot, \
                mock.patch.object(R, "record_build") as record, mock.patch.object(R, "release") as release:
            with self.assertRaisesRegex(SystemExit, "binary-version: SDK"):
                B.main()
            for function in (snapshot, record, release, B.generate, B.build_loader, B.build_app):
                function.assert_not_called()
        self.assertEqual(record_path.read_bytes(), before)

    def test_binary_version_output_guard_precedes_all_build_calls(self):
        for index, name in enumerate(("caravel-0.1.0.fwsc", "loader/ota.bin", "gen/ui_fonts.h")):
            with self.subTest(path=name):
                output = self.root / "build" / name
                output.parent.mkdir(parents=True, exist_ok=True)
                original = output.read_bytes() if output.exists() else None
                output.unlink(missing_ok=True)
                sentinel = put(Path(self.tmp.name), f"binary-outside-{index}", b"unchanged")
                output.symlink_to(sentinel)
                try:
                    with self.build_context(binary_version=VERSION), \
                            mock.patch.object(R, "guard_output_tree", wraps=R.guard_output_tree) as guard, \
                            mock.patch.object(R, "source_snapshot") as snapshot:
                        with self.assertRaisesRegex(SystemExit, "binary-version: symlink output path"):
                            B.main()
                        guard.assert_called_once_with(self.root)
                        for function in (snapshot, B.generate, B.build_loader, B.build_app):
                            function.assert_not_called()
                    self.assertEqual(sentinel.read_bytes(), b"unchanged")
                finally:
                    output.unlink()
                    if original is not None:
                        output.write_bytes(original)

    def test_ordinary_default_keeps_default_identity_after_versioned_build(self):
        with self.build_context(binary_version=VERSION):
            self.assertEqual(B.main(), 0)
        with self.build_context(release=False), mock.patch.object(R, "source_snapshot") as snapshot, \
                mock.patch.object(R, "record_build") as record, mock.patch.object(R, "release") as release:
            self.assertEqual(B.main(), 0)
            self.assertEqual(B.PRODUCT, "FM-1_900")
            self.assertEqual(B.VERSION, "v0.1.0-dev")            # (the default identity; the version says dev)
            for function in (snapshot, record, release):
                function.assert_not_called()
        package = (self.root / "build/caravel.fwsc").read_bytes()
        self.assertEqual(R.fm1_install.product_of(package), "FM-1_900")

    def test_build_integration_after_mocked_target_checks_test_only(self):
        with self.build_context():
            self.assertEqual(B.main(), 0)
        self.verify()

    def test_release_sdk_mismatch_fails_before_build_default_still_warns(self):
        wrong = {**R.SDK_SHA256, "uboot.boot": "0" * 64}
        with mock.patch.object(B, "SDK_SHA256", wrong):
            with self.build_context(), self.assertRaisesRegex(SystemExit, "release: SDK"):
                B.main()
            with self.build_context(release=False), mock.patch.object(B, "generate", side_effect=RuntimeError("generation reached")):
                with self.assertRaisesRegex(RuntimeError, "generation reached"):
                    B.main()


if __name__ == "__main__":
    unittest.main(verbosity=2)
