#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Copyright (C) 2026 bonszaimusic
"""Build an offline site from one verified, flat release download.

  web/make_site.py PACKAGE VERSION OUT --release-dir RELEASE [--emulator BUILD]
  web/make_site.py PACKAGE VERSION OUT --preview

Production verifies the manifest, tagged source archive, notices and build record
before writing. Preview is explicit and is not a source/binary release attestation.
No network access, device access or deployment is performed by this program.
"""
import argparse
import html as markup
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "tools"))
import release_artifacts as release

BLOCKS, BLK, KEEP = 20, 0x30, 0x2F
EMULATOR_FILES = ("index.html", "felucca.wasm", "worklet.js",
                  "fonts/DotGothic16-subset.woff", "fonts/OFL.txt")


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    release.require(len(raw) >= BLOCKS * BLK, "truncated package")
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS)
                   if (m := raw[i * BLK + KEEP]) != 0x7D)


def source_href(text, url):
    pattern = r'(<a id="matching-source" href=")[^"]*(")'
    text, count = re.subn(pattern, lambda m: m[1] + markup.escape(url, quote=True) + m[2], text)
    release.require(count == 1, "template must contain one matching-source link")
    return text


def checked_release(root, directory, version, package):
    """Verify a downloaded inventory against this exact tagged checkout.

    Package reconstruction/SDK checks happen in the release build. Here we verify
    their published record, all downloaded hashes, and complete source identity.
    Checksums are consistency checks, not independent cryptographic signatures.
    """
    directory = Path(directory)
    hashes = release.verify_checksums(directory)
    snapshot = release.source_snapshot(root)
    files = snapshot[0]
    licenses = release.license_artifacts(files)
    binaries = {f"caravel-{version}{suffix}": release.read_regular(directory, f"caravel-{version}{suffix}")
                for suffix in (".fwsc", "-app.bin", "-loader.bin")}
    source = f"caravel-{version}-source.tar.gz"
    required = set(binaries) | set(release.DOCS) | set(licenses) | {source, "release-build.json", "ATTRIBUTION.txt"}
    release.require(set(hashes) == required, "release download inventory mismatch")
    release.require(Path(package).resolve() == (directory / f"caravel-{version}.fwsc").resolve()
                    and not Path(package).is_symlink(), "package must be from the verified release directory")
    record = json.loads(release.read_regular(directory, "release-build.json"))
    release.require(record == release.build_record(version, snapshot, binaries, dict(release.SDK_SHA256)),
                    "downloaded release/source/version record mismatch")
    app = binaries[f"caravel-{version}-app.bin"]
    product = release.identity(version)
    release.require(("v" + version.lower()).encode() + b"\0" in app
                    and product.encode() + b"\0" in app, "downloaded app version/product mismatch")
    release.verify_loader(binaries[f"caravel-{version}-loader.bin"])
    release.verify_source(directory / source, version, files)
    for name in release.DOCS:
        release.require(release.read_regular(directory, name) == files[name][1], "release notice/source mismatch")
    release.require(release.read_regular(directory, "ATTRIBUTION.txt") == files["assets/samples-cc0/ATTRIBUTION.txt"][1],
                    "release sample attribution mismatch")
    for name, data in licenses.items():
        release.require(release.read_regular(directory, name) == data, "release licence/source mismatch")
    return hashes


def checked_emulator(directory, version, preview):
    directory = Path(directory)
    expected = set(EMULATOR_FILES)
    actual = set()
    release.require(directory.is_dir() and not directory.is_symlink(), "missing/unsafe emulator directory")
    for path in directory.rglob("*"):
        release.require(not path.is_symlink(), "emulator symlinks are not supported")
        if path.is_file():
            actual.add(path.relative_to(directory).as_posix())
        else:
            release.require(path.is_dir(), "unsupported emulator entry")
    release.require(actual == expected, "unexpected/missing emulator files")
    text = release.read_regular(directory, "index.html").decode("utf-8")
    version_label = f'<small class="version">v{version.lower()}'
    release.require(version_label in text, "emulator version mismatch")
    if preview:
        text, count = re.subn(re.escape(version_label) + r"[^<]*</small>",
                              version_label + " (local preview; not release-verified)</small>", text)
        release.require(count == 1, "emulator must contain one version label")
    else:
        release.require(version_label + "</small>" in text, "release emulator has a preview/other label")
    release.require(release.read_regular(directory, "felucca.wasm").startswith(b"\0asm\x01\0\0\0"),
                    "invalid emulator WASM header")
    for name in ("worklet.js", "fonts/DotGothic16-subset.woff", "fonts/OFL.txt"):
        release.require(release.read_regular(directory, name) == release.read_regular(HERE / "emu", name),
                        "emulator support files differ from selected source")
    return text


def checked_output(out):
    """Preflight before staging; assumes no concurrent path replacement.

    Do not resolve arbitrary ancestors: that would hide write-redirecting links.
    Only macOS's standard leading system aliases may be canonicalized first.
    """
    out = out.absolute()
    if sys.platform == "darwin":
        for name in ("var", "tmp", "etc"):
            alias, target = Path("/" + name), Path("/private") / name
            if (out != alias and alias in out.parents and alias.is_symlink()
                    and alias.resolve() == target):
                out = target / out.relative_to(alias)
                break
    for parent in reversed(out.parents):
        release.require(not parent.is_symlink(), f"symlink output parent not allowed: {parent}")
    release.require(not out.is_symlink() and (not out.exists() or (out.is_dir() and not any(out.iterdir()))),
                    "site destination must be new or empty (never overwrite existing content)")
    release.require(out.parent.is_dir(), "site output parent must exist")
    return out


def main(pkg, version, out, *, release_dir=None, emulator=None, preview=False, check_only=False):
    expected_product = release.identity(version)
    pkg, out = Path(pkg), Path(out)
    release.require(not pkg.is_symlink(), "package symlink is not supported")
    raw = pkg.read_bytes()
    product = product_of(raw)
    release.require(re.fullmatch(r"FM-1_9[0-9][0-9]", product) is not None
                    and (preview or product == expected_product), "package product/version mismatch")
    release.require(b"FELUCCA-LOADER-1" in raw, "package has no Caravel/Felucca loader marker")
    release.require(not (preview and release_dir), "preview and verified release modes are exclusive")
    hashes = None
    if not preview:
        release.require(release_dir is not None, "provide --release-dir, or --preview for local-only testing")
        hashes = checked_release(HERE.parent, release_dir, version, pkg)
    emulator_html = checked_emulator(emulator, version, preview) if emulator else None
    if check_only:
        print("site inputs verified; no output/deployment performed")
        return
    out = checked_output(out)
    name = f"caravel-{version}.fwsc"
    source = f"caravel-{version}-source.tar.gz"
    source_url = "../../firmware/" + source if not preview else "https://github.com/bonszaimusic/caravel"
    text = (HERE / "index_pkg.html").read_text(encoding="utf-8")
    lib = "\n".join(strip_module((HERE / n).read_text(encoding="utf-8"))
                    for n in ("fm1pkg.js", "fm1ota.js", "fm1backup.js"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../../firmware/" + name,
                       "source": source_url, "release_verified": not preview})
    for mark in ("/*LIB*/", "/*META*/"):
        release.require(text.count(mark) == 1, "installer metadata/library marker count changed")
    text = source_href(text.replace("/*LIB*/", lib).replace("/*META*/", meta), source_url)
    if preview:
        text = text.replace('<h1>Caravel</h1>', '<h1>Caravel (local preview; not release-verified)</h1>')
    with tempfile.TemporaryDirectory(prefix=".caravel-site-", dir=out.parent) as temp:
        stage = Path(temp)
        inst, ed, fw = stage / "webapp/installer", stage / "webapp/editor", stage / "firmware"
        for d in (inst, ed, fw / "LICENSES"):
            d.mkdir(parents=True)
        (inst / "index.html").write_text(text, encoding="utf-8")
        if hashes is not None:
            for entry in (*hashes, "SHA256SUMS"):
                shutil.copyfile(Path(release_dir) / entry, fw / entry)
        else:
            shutil.copyfile(pkg, fw / name)
            for doc in release.DOCS:
                shutil.copyfile(HERE.parent / doc, fw / doc)
        licenses = sorted((HERE.parent / "LICENSES").glob("*.txt"))
        for path in licenses:
            shutil.copyfile(path, fw / "LICENSES" / path.name)
        (fw / "LICENSES/index.html").write_text(
            '<!doctype html><meta charset="utf-8"><title>Caravel licences</title><h1>Licence texts</h1><ul>'
            + "".join(f'<li><a href="{markup.escape(p.name, quote=True)}">{markup.escape(p.name)}</a></li>' for p in licenses)
            + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE</a> · '
              '<a href="../NOTICE">NOTICE</a> · <a href="../CREDITS.md">CREDITS</a></p>\n', encoding="utf-8")
        shutil.copyfile(HERE / "editor.html", ed / "index.html")
        for filename in ("fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js"):
            shutil.copyfile(HERE / filename, ed / filename)
        if emulator:
            dest = stage / "emulator"
            shutil.copytree(emulator, dest)
            emulator_source = "../firmware/" + source if not preview else "https://github.com/bonszaimusic/caravel"
            (dest / "index.html").write_text(source_href(emulator_html, emulator_source), encoding="utf-8")
        (stage / "index.html").write_text(
            '<!doctype html><meta charset="utf-8"><title>Caravel</title>'
            '<meta http-equiv="refresh" content="0; url=webapp/installer/">'
            '<a href="webapp/installer/">Caravel installer</a>\n', encoding="utf-8")
        stage.rename(out)
    print(f"site: {out}; version {version}; verified release: {not preview}; no deployment performed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pkg")
    parser.add_argument("version")
    parser.add_argument("out")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--release-dir", type=Path)
    mode.add_argument("--preview", action="store_true")
    parser.add_argument("--emulator", type=Path)
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()
    try:
        main(args.pkg, args.version, args.out, release_dir=args.release_dir,
             emulator=args.emulator, preview=args.preview, check_only=args.check_only)
    except (release.ReleaseError, OSError, ValueError) as error:
        parser.exit(1, f"site verification failed: {error}\n")
