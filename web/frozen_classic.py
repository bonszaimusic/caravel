#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Fetch or verify the pinned, published Classic bytes. Never build Classic."""
import argparse
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path, PurePosixPath
import re
import tempfile
import zipfile
from urllib.parse import quote, urlsplit, urljoin, unquote
from urllib.request import Request, urlopen

MANIFEST = Path(__file__).with_name("classic_manifest.json")
COMMIT = "d85ecb7e82c3678cea24c4812a8a331f097f3e72"


def inventory(manifest=MANIFEST):
    data = json.loads(Path(manifest).read_text(encoding="utf-8"))
    if (data.get("schema_version") != 1 or data.get("version") != "0.13.0"
            or data.get("source_commit") != COMMIT):
        raise ValueError("unsupported Classic inventory identity")
    files = data.get("files")
    if not isinstance(files, list) or not files:
        raise ValueError("empty Classic inventory")
    seen = set()
    for row in files:
        name = row.get("path", "")
        path = PurePosixPath(name)
        if (not name or path.is_absolute() or path.as_posix() != name
                or any(p in (".", "..") for p in path.parts)
                or "\\" in name or "\x00" in name or name in seen):
            raise ValueError("unsafe or duplicate Classic path")
        if (type(row.get("size")) is not int or not 0 <= row["size"] <= 40 * 1024 * 1024
                or not re.fullmatch(r"[0-9a-f]{64}", row.get("sha256", ""))):
            raise ValueError("invalid Classic size/digest")
        seen.add(name)
    return data


def check_bytes(row, payload):
    if len(payload) != row["size"] or hashlib.sha256(payload).hexdigest() != row["sha256"]:
        raise ValueError(f"frozen Classic bytes differ: {row['path']}")


def verify_runtime_references(root):
    """Do not mistake a hash-complete inventory for a complete runtime copy."""
    origin = "https://bonszaimusic.github.io/caravel/"
    class Links(HTMLParser):
        def __init__(self):
            super().__init__()
            self.refs = []
        def handle_starttag(self, tag, attrs):
            self.refs.extend(value for key, value in attrs if key in ("href", "src") and value)
    checked = set()
    for path in root.rglob("*"):
        if path.suffix not in (".html", ".js", ".css", ".svg"):
            continue
        source = path.relative_to(root).as_posix()
        text = path.read_text(encoding="utf-8")
        refs = re.findall(r'''(?:\bimport|\bfetch|addModule|importScripts)\(\s*['"]([^'"]+)''', text)
        refs += re.findall(r'''\bfrom\s*['"]([^'"]+)''', text)
        refs += re.findall(r'''\bnew\s+URL\(\s*['"]([^'"]+)''', text)
        refs += re.findall(r'''url\(\s*['"]?([^'"\s)]+)''', text)
        if path.suffix in (".html", ".svg"):
            parser = Links()
            parser.feed(text)
            refs += parser.refs
        for ref in refs:
            if ref.startswith("#") or "${" in ref:
                continue
            url = urlsplit(urljoin(origin + source, ref))
            if url.netloc != "bonszaimusic.github.io" or not url.path.startswith("/caravel/"):
                continue
            relative = unquote(url.path[len("/caravel/"):])
            if (PurePosixPath(relative).is_absolute() or ".." in PurePosixPath(relative).parts
                    or "\\" in relative or "\x00" in relative):
                raise ValueError(f"unsafe Classic runtime dependency: {source} -> {ref}")
            target = root / relative
            if target.is_dir():
                target /= "index.html"
            if not target.is_file():
                raise ValueError(f"missing Classic runtime dependency: {source} -> {relative}")
            checked.add((source, target.relative_to(root).as_posix()))
    return len(checked)


def verify(root, manifest=MANIFEST):
    root = Path(root)
    data = inventory(manifest)
    if root.is_symlink() or not root.is_dir():
        raise ValueError("Classic input must be a regular directory")
    actual = set()
    for path in root.rglob("*"):
        if path.is_symlink() or not (path.is_dir() or path.is_file()):
            raise ValueError("linked/special Classic file refused")
        if path.is_file():
            actual.add(path.relative_to(root).as_posix())
    if actual != {row["path"] for row in data["files"]}:
        raise ValueError("Classic file inventory differs (missing or extra files)")
    for row in data["files"]:
        check_bytes(row, (root / row["path"]).read_bytes())
    verify_runtime_references(root)
    return data


def fetch(out, manifest=MANIFEST, source=None):
    data = inventory(manifest)
    source = source or data["origin"]
    url = urlsplit(source)
    if url.scheme not in ("https", "http") or not url.netloc or url.query or url.fragment:
        raise ValueError("invalid Classic source URL")
    if url.scheme == "http" and url.hostname not in ("localhost", "127.0.0.1", "::1"):
        raise ValueError("HTTP is only allowed for local offline tests")
    out = Path(out).absolute()
    if out.exists() or out.is_symlink():
        raise ValueError("refuse existing Classic destination")
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".classic-frozen-", dir=out.parent) as temporary:
        stage = Path(temporary) / "site"
        stage.mkdir()
        for row in data["files"]:
            request = Request(source.rstrip("/") + "/" + quote(row["path"]),
                              headers={"Accept-Encoding": "identity"})
            with urlopen(request, timeout=30) as response:
                payload = response.read(row["size"] + 1)
            check_bytes(row, payload)
            path = stage / row["path"]
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(payload)
        verify(stage, manifest)
        stage.rename(out)
    return data


def unpack(archive, out, manifest=MANIFEST):
    """Transport the exact frozen inventory from a release asset, without building."""
    data = inventory(manifest)
    out = Path(out).absolute()
    if out.exists() or out.is_symlink():
        raise ValueError("refuse existing Classic destination")
    out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as source, tempfile.TemporaryDirectory(prefix=".classic-frozen-", dir=out.parent) as temporary:
        expected = {row["path"]: row for row in data["files"]}
        if len(source.infolist()) != len(expected) or set(source.namelist()) != set(expected):
            raise ValueError("Classic archive inventory differs")
        stage = Path(temporary) / "site"
        stage.mkdir()
        for item in source.infolist():
            row = expected[item.filename]
            if item.file_size != row["size"] or item.external_attr >> 28 == 10:
                raise ValueError("Classic archive entry refused")
            payload = source.read(item)
            check_bytes(row, payload)
            path = stage / item.filename
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(payload)
        verify(stage, manifest)
        stage.rename(out)
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--out", type=Path)
    group.add_argument("--verify", type=Path)
    parser.add_argument("--source", help="existing published byte snapshot, never source code")
    parser.add_argument("--archive", type=Path, help="verified frozen ZIP transport instead of network fetch")
    args = parser.parse_args()
    if args.archive and (args.verify or args.source):
        parser.error("--archive requires --out and cannot be combined with --source")
    try:
        data = (verify(args.verify, args.manifest) if args.verify else
                unpack(args.archive, args.out, args.manifest) if args.archive else
                fetch(args.out, args.manifest, args.source))
    except (ValueError, OSError, zipfile.BadZipFile) as error:
        parser.exit(1, f"Classic snapshot refused: {error}\n")
    print(f"Frozen Classic verified: {len(data['files'])} files; no build")


if __name__ == "__main__":
    main()
