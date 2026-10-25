#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Assemble/verify build/release-VERSION after an actual build.py --release.

  python tools/release_artifacts.py VERSION [--verify-only] [--sdk PATH]

Source is always the checkout containing this script, not the caller's cwd.
Production source (including recursive submodules) must be committed and clean.
Release attachments and SHA256SUMS are flat (licence basenames at the root).
The source archive retains the original LICENSES/ paths.
Checks establish artifact consistency, NOT SDK rights or device acceptance.
"""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import struct
import subprocess
import tarfile
import tempfile

import fm1_install
import fm1pkg_make
import lz4blk

SRC = Path(__file__).resolve().parents[1]
# Reference identity, not an assertion that a local blob cache is this Git checkout.
SDK_REFERENCE = {
    "repository": "https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git",
    "tag": "AC79NN_SDK_V1.2.1_2023-12-13",
    "commit": "d179b4484759423312073f5fbb232501aa491047",
}
SDK_SHA256 = {
    "uboot.boot": "4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4",
    "cfg_tool.bin": "276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b",
    "cfg/eq_cfg_hw.bin": "41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d",
}
DOCS = ("LICENSE", "LICENSING.md", "NOTICE", "CREDITS.md")
LICENSES = (
    "Apache-2.0-msfa.txt", "Apache-2.0.txt", "MIT-DaisySP.txt",
    "MIT-Felucca-Engines.txt", "MIT-Fukiai.txt", "MIT-Rings.txt",
    "OFL-DotGothic16.txt", "OFL-InterTight.txt",
)
EXCLUDED = {".git", "build", "private", "review", "cache", "__pycache__", ".tools", ".DS_Store"}


class ReleaseError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise ReleaseError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def identity(version):
    m = re.fullmatch(r"([0-9])\.([0-9])(?:\.[0-9]){0,2}(?:-[A-Za-z0-9]+(?:-[A-Za-z0-9]+)*)?", version)
    require(m is not None, "version: use X.Y[.Z[.W]][-suffix], one digit each")
    return "FM-1_999"  # Fixed Caravel identity; previously published tags keep their own source.


def safe_path(name):
    p = PurePosixPath(name)
    require(bool(name) and not p.is_absolute() and p.as_posix() == name
            and all(part not in (".", "..") for part in p.parts)
            and not any(ord(c) < 32 or c in "\\:" for c in name),
            f"unsafe path: {name!r}")
    return p


def guard_output_tree(root):
    """Read-only preflight before release writes; never traverse output symlinks.

    Reject the whole existing output tree, including ignored/nested files, if it
    can redirect writes. This assumes no concurrent replacement during a build.
    """
    out = Path(root).resolve() / "build"

    def visit(path):
        try:
            info = path.lstat()
        except FileNotFoundError:
            return
        require(not stat.S_ISLNK(info.st_mode), f"symlink output path not allowed: {path}")
        if stat.S_ISDIR(info.st_mode):
            for child in path.iterdir():
                visit(child)
        else:
            require(path != out and stat.S_ISREG(info.st_mode), f"invalid output path: {path}")
            require(info.st_nlink == 1, f"hard-linked output file not allowed: {path}")

    visit(out)


def read_regular(root, name, allow_empty=False):
    parts = safe_path(name).parts
    require(not root.is_symlink(), f"symlink not allowed: {root}")
    p = root
    for part in parts:
        p = p / part
        require(not p.is_symlink(), f"symlink not allowed: {p}")
    require(p.is_file(), f"missing required file: {p}")
    data = p.read_bytes()
    require(allow_empty or bool(data), f"empty required file: {p}")
    return data


def git(root, *args):
    result = subprocess.run(["git", "--no-pager", "--no-optional-locks", "-C", str(root), *args],
                            capture_output=True)
    require(result.returncode == 0, f"git {args[0]} failed in {root}: {result.stderr.decode(errors='replace')}")
    return result.stdout


def source_snapshot(root):
    """Read only pinned tracked blobs; reject dirty, uninitialized or unsafe trees."""
    root = root.resolve()
    files, commits = {}, {}

    def visit(repo, prefix="", pin=None):
        require(git(repo, "rev-parse", "--show-toplevel").decode().strip() == str(repo),
                f"uninitialized submodule: {repo}")
        head = git(repo, "rev-parse", "HEAD").decode().strip()
        require(pin is None or head == pin, f"submodule pin mismatch: {repo}")
        require(not git(repo, "status", "--porcelain=v1", "--untracked-files=all", "--ignore-submodules=none"),
                f"dirty source/submodule: {repo}; commit reviewed changes before a production release")
        commits[prefix.rstrip("/") or "."] = head
        for item in git(repo, "ls-tree", "-rz", "--full-tree", "HEAD").split(b"\0"):
            if not item:
                continue
            meta, rawname = item.split(b"\t", 1)
            mode, typ, oid = meta.decode().split()
            name = rawname.decode("utf-8")
            full = prefix + name
            parts = safe_path(full).parts
            require(not EXCLUDED.intersection(parts) and not any(p.startswith("._") for p in parts),
                    f"excluded source path: {full}")
            if mode == "160000":
                require((repo / name).is_dir() and not (repo / name).is_symlink(),
                        f"missing submodule: {full}")
                visit(repo / name, full + "/", oid)
            else:
                require(mode in ("100644", "100755") and typ == "blob", f"unsupported source mode: {full}")
                data = read_regular(repo, name, allow_empty=True)
                blob = b"blob " + str(len(data)).encode() + b"\0" + data
                digest = hashlib.sha1(blob).hexdigest() if len(oid) == 40 else hashlib.sha256(blob).hexdigest()
                require(digest == oid, f"source differs from pinned blob: {full}")
                files[full] = (0o755 if mode == "100755" else 0o644, data)
        require(not git(repo, "status", "--porcelain=v1", "--untracked-files=all", "--ignore-submodules=none"),
                f"source changed during inspection: {repo}")

    visit(root)
    for name in (*DOCS, *("LICENSES/" + n for n in LICENSES),
                 "assets/samples-cc0/ATTRIBUTION.txt", "firmware/engines/LICENSE",
                 "firmware/engines/src/felucca_engine.h", "firmware/src/felucca.c",
                 "tools/build.py", "tools/fm1pkg_make.py", "tools/release_artifacts.py", "build.sh"):
        require(name in files and bool(files[name][1]), f"missing/empty required tracked source/notice: {name}")
    require(files["firmware/engines/LICENSE"][1] == files["LICENSES/MIT-Felucca-Engines.txt"][1],
            "engine licence differs from shipped licence")
    return files, commits


def source_identity(snapshot):
    files, commits = snapshot
    inventory = [[n, mode, sha(data)] for n, (mode, data) in sorted(files.items())]
    return {"commits": commits, "tracked_sha256": sha(json.dumps(inventory, separators=(",", ":")).encode())}


def sdk_hashes(sdk=None):
    old = fm1pkg_make.SDK
    try:
        fm1pkg_make.SDK = sdk
        actual = {name: sha(fm1pkg_make.sdk_file(name)) for name in SDK_SHA256}
    finally:
        fm1pkg_make.SDK = old
    require(actual == SDK_SHA256, "SDK hash mismatch: release requires the exact reference SDK bytes (not licence clearance)")
    return actual


def verify_loader(ota):
    def header(data):
        require(len(data) >= 32 and fm1pkg_make.crc16(data[2:32]) == int.from_bytes(data[:2], "little"),
                "loader header CRC/size mismatch")
        return struct.unpack("<HHIIBBH16s", data[:32])

    _, crc, offset, size, flags, _, _, name = header(ota)
    require(offset == 32 and size == len(ota) - 32 and flags == 0x41
            and name.rstrip(b"\0") == b"usb_hid_ota.bin" and fm1pkg_make.crc16(ota[32:]) == crc,
            "loader outer contents mismatch")
    _, crc, size, address, flags, _, _, name = header(ota[32:])
    require(0 < size <= 0x14000 and address == 0x01C0A800 and flags == 0
            and name.rstrip(b"\0") == b"usb_hid_ota.bin", "loader inner contents mismatch")
    image, pos = bytearray(), 64
    while pos < len(ota):
        require(pos + 8 <= len(ota), "truncated loader block")
        clen, dlen = struct.unpack_from("<II", ota, pos)
        pos += 8
        require(0 < clen <= 4096 and 0 < dlen <= 4096 and pos + clen <= len(ota)
                and len(image) + dlen <= size, "invalid loader block bounds")
        try:
            raw = lz4blk.decompress(ota[pos:pos + clen])
        except (IndexError, ValueError) as error:
            raise ReleaseError(f"invalid loader compression: {error}") from error
        require(len(raw) == dlen, "loader decompressed size mismatch")
        image.extend(raw)
        pos += clen
    require(len(image) == size and fm1pkg_make.crc16(image) == crc
            and b"FELUCCA-LOADER-1" in image, "loader image CRC/marker mismatch")


def verify_binaries(root, version, sdk=None):
    product = identity(version)
    out = root / "build"
    app = read_regular(out, "felucca.bin")
    ota = read_regular(out, "loader/ota.bin")
    pkg = read_regular(out, f"caravel-{version}.fwsc")
    require(len(app) <= fm1pkg_make.APP_SLOT, "app exceeds package slot")
    require(("v" + version.lower()).encode() + b"\0" in app and product.encode() + b"\0" in app,
            "app version/identity mismatch with release override")
    verify_loader(ota)
    hashes = sdk_hashes(sdk)
    require(fm1_install.product_of(pkg) == product, "package identity mismatch")
    old = fm1pkg_make.SDK
    try:
        fm1pkg_make.SDK = sdk
        expected = fm1pkg_make.ufw(fm1pkg_make.flash_image(app, fm1pkg_make.KEY), ota, product)
    finally:
        fm1pkg_make.SDK = old
    require(pkg == expected, "package contents/CRC mismatch with built app, loader and reference SDK")
    return {f"caravel-{version}.fwsc": pkg, f"caravel-{version}-app.bin": app,
            f"caravel-{version}-loader.bin": ota}, hashes


def build_record(version, snapshot, binaries, hashes):
    source = snapshot[0]["firmware/src/felucca.c"][1].decode()
    default = re.search(r'^#define FELUCCA_VERSION "([^"\n]+)"', source, re.M)
    require(default is not None, "source FELUCCA_VERSION default missing")
    return {"schema": 1, "version": version, "firmware_version_override": "v" + version.lower(),
            "source_default_version": default[1], "product": identity(version),
            "source": source_identity(snapshot), "sdk_reference": dict(SDK_REFERENCE), "sdk_sha256": hashes,
            "binaries": {n: sha(b) for n, b in sorted(binaries.items())}}


def record_build(root, version, snapshot, sdk=None):
    """Called by build.py only after its target build and target checks succeed."""
    guard_output_tree(root)
    require(source_identity(source_snapshot(root)) == source_identity(snapshot), "source changed during build")
    binaries, hashes = verify_binaries(root, version, sdk)
    record = build_record(version, snapshot, binaries, hashes)
    (root / "build" / f"release-{version}.build.json").write_bytes(json_bytes(record))


def json_bytes(value):
    return (json.dumps(value, sort_keys=True, indent=2) + "\n").encode()


def license_artifacts(files):
    """Map source-snapshot LICENSES/ entries to unique flat attachment basenames."""
    result = {}
    for path, (_, data) in sorted(files.items()):
        if path.startswith("LICENSES/"):
            name = PurePosixPath(path).name
            safe_path(name)
            require(name not in result, f"duplicate licence attachment basename: {name}")
            result[name] = data
    return result


def expected_artifacts(root, version, sdk=None):
    identity(version)
    snapshot = source_snapshot(root)
    files = snapshot[0]
    binaries, hashes = verify_binaries(root, version, sdk)
    record_bytes = read_regular(root / "build", f"release-{version}.build.json")
    record = json.loads(record_bytes)
    require(record == build_record(version, snapshot, binaries, hashes),
            "release build record mismatch; run an actual build.py --release for this clean source")
    artifacts = dict(binaries)
    artifacts.update({n: files[n][1] for n in DOCS})
    artifacts["ATTRIBUTION.txt"] = files["assets/samples-cc0/ATTRIBUTION.txt"][1]
    artifacts["release-build.json"] = record_bytes
    licenses = license_artifacts(files)
    reserved = set(artifacts) | {"SHA256SUMS", f"caravel-{version}-source.tar.gz"}
    require(not reserved.intersection(licenses), "licence attachment collides with release artifact")
    artifacts.update(licenses)
    return artifacts, snapshot


def write_source(path, version, files):
    with path.open("wb") as raw, gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as gz:
        with tarfile.open(fileobj=gz, mode="w", format=tarfile.PAX_FORMAT) as archive:
            for name, (mode, data) in sorted(files.items()):
                entry = tarfile.TarInfo(f"caravel-{version}/{name}")
                entry.size, entry.mode, entry.mtime = len(data), mode, 0
                archive.addfile(entry, io.BytesIO(data))


def verify_source(path, version, files):
    with tarfile.open(path, "r:gz") as archive:
        names = []
        for member in archive:
            safe_path(member.name)
            prefix = f"caravel-{version}/"
            require(member.name.startswith(prefix), "source archive prefix mismatch")
            name = member.name[len(prefix):]
            require(name in files and member.isfile() and member.mode == files[name][0]
                    and member.uid == member.gid == member.mtime == 0
                    and not member.uname and not member.gname and not member.linkname,
                    f"unsafe/unexpected source member: {member.name}")
            require(member.size == len(files[name][1]) and archive.extractfile(member).read() == files[name][1],
                    f"source archive contents mismatch: {name}")
            names.append(name)
        require(names == sorted(files), "source archive missing/duplicate/out-of-order members")


def checksum_bytes(artifacts):
    return "".join(f"{sha(data)}  {name}\n" for name, data in sorted(artifacts.items())).encode()


def verify_checksums(directory):
    """Verify a complete flat downloaded release; return {basename: sha256}.

    No checkout, build products or SDK are needed. This checks consistency with
    SHA256SUMS, not its authenticity or correspondence to a particular source.
    """
    directory = Path(directory)
    manifest = read_regular(directory, "SHA256SUMS")
    data, hashes = {}, {}
    for line in manifest.splitlines(keepends=True):
        match = re.fullmatch(rb"([0-9a-f]{64})  ([^\r\n]+)\n", line)
        require(match is not None, "SHA256SUMS mismatch: invalid manifest line")
        name = match[2].decode("utf-8")
        try:
            parts = safe_path(name).parts
        except ReleaseError as error:
            raise ReleaseError("SHA256SUMS mismatch: unsafe filename") from error
        require(len(parts) == 1 and name != "SHA256SUMS" and name not in data,
                "SHA256SUMS mismatch: unsafe, duplicate or self-referencing filename")
        data[name] = read_regular(directory, name)
        hashes[name] = match[1].decode("ascii")
    require(bool(data) and manifest == checksum_bytes(data), "SHA256SUMS mismatch")
    found = set()
    for path in directory.iterdir():
        require(not path.is_symlink() and path.is_file(), f"non-flat/unsafe release attachment: {path}")
        found.add(path.name)
    require(found == set(data) | {"SHA256SUMS"}, "SHA256SUMS mismatch: release inventory differs")
    return hashes


def verify_directory(directory, version, artifacts, snapshot):
    source = f"caravel-{version}-source.tar.gz"
    required = set(artifacts) | {source, "SHA256SUMS"}
    found = set()
    for path in directory.iterdir():
        require(not path.is_symlink(), f"symlink in release directory: {path}")
        if path.is_file():
            found.add(path.relative_to(directory).as_posix())
        else:
            raise ReleaseError(f"unexpected release directory: {path}")
    require(found == required, f"release inventory mismatch: missing={sorted(required - found)}, extra={sorted(found - required)}")
    for name, data in artifacts.items():
        require(read_regular(directory, name) == data, f"release artifact mismatch: {name}")
    verify_source(directory / source, version, snapshot[0])
    verify_checksums(directory)


def release(root, version, verify_only=False, sdk=None):
    root = Path(root).resolve()
    guard_output_tree(root)
    artifacts, snapshot = expected_artifacts(root, version, sdk)
    directory = root / "build" / f"release-{version}"
    require(not directory.is_symlink(), f"symlink release directory: {directory}")
    if verify_only:
        verify_directory(directory, version, artifacts, snapshot)
    else:
        with tempfile.TemporaryDirectory(prefix=".release-", dir=root / "build") as tmp:
            staging = Path(tmp) / "artifacts"
            staging.mkdir()
            for name, data in artifacts.items():
                target = staging / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
            source = f"caravel-{version}-source.tar.gz"
            write_source(staging / source, version, snapshot[0])
            (staging / "SHA256SUMS").write_bytes(checksum_bytes({**artifacts, source: (staging / source).read_bytes()}))
            verify_directory(staging, version, artifacts, snapshot)
            require(source_identity(source_snapshot(root)) == source_identity(snapshot), "source changed during assembly")
            if directory.exists():
                shutil.rmtree(directory)
            staging.rename(directory)
    return directory


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version")
    parser.add_argument("--verify-only", action="store_true")
    parser.add_argument("--sdk", type=Path, help="SDK checkout (default: AC79_SDK)")
    args = parser.parse_args(argv)
    try:
        directory = release(SRC, args.version, args.verify_only, args.sdk)
    except (ReleaseError, OSError, ValueError, tarfile.TarError, fm1_install.InstallError) as error:
        parser.exit(1, f"release: {error}\n")
    print(f"release verified: {directory} (artifact consistency only; not redistribution/device approval)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
