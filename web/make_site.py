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
# the firmware's SHIP_BITS (firmware/src/ui_draw.c) as one path on a 28 x 17 grid: the logo and the favicon
SHIP_PATH = ("M11 0h2v1h-2zM11 1h1v1h-1zM11 2h2v1h-2zM7 3h9v1h-9zM19 3h1v1h-1zM7 4h9v1h-9zM19 4h1v1h-1zM7 5h7v1h-7zM15 5h7v1h-7z"
             "M7 6h7v1h-7zM15 6h8v1h-8zM7 7h7v1h-7zM15 7h8v1h-8zM6 8h8v1h-8zM15 8h8v1h-8zM2 9h21v1h-21zM27 9h1v1h-1zM2 10h21v1h-21z"
             "M25 10h2v1h-2zM2 11h5v1h-5zM11 11h1v1h-1zM19 11h1v1h-1zM24 11h2v1h-2zM2 12h22v1h-22zM3 13h20v1h-20zM4 14h18v1h-18zM7 15h13v1h-13z")
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


def site_chrome(text, version, emulator, label=""):
    """Fill a page header: the version pill, and no EMULATOR link when the site has no emulator."""
    release.require(text.count("@VERSION@") == 1, "page must contain one header version marker")
    text = text.replace("@VERSION@", markup.escape(version.lower() + label))
    link = r'\s*<a [^>]*data-site="emulator"[^>]*>[^<]*</a>'
    if emulator:
        return text
    text, count = re.subn(link, "", text)
    release.require(count == 1, "page must contain one emulator navigation link")
    return text


def look_css(installer):
    """The shared palette / header / box rules, as the installer page carries them (the other pages repeat them)."""
    found = re.search(r"/\*LOOK-BEGIN\*/\n(.*?)\n\s*/\*LOOK-END\*/", installer, flags=re.S)
    release.require(found is not None, "installer template lost its shared look block")
    return found[1]


def landing(look, version, emulator, source_url, label=""):
    """The site's start page: header, hero and a box per page. Self-contained (no external requests)."""
    ver = markup.escape(version.lower() + label)
    ship = SHIP_PATH
    boxes = [("INSTALL", "webapp/installer/", "Install Caravel on the FM-1 over USB (Chrome or Edge)."),
             ("EDITOR", "webapp/editor/", "Edit sounds, sequences, banks and samples from the computer.")]
    if emulator:
        boxes.append(("EMULATOR", "emulator/", "The same code the instrument runs, in the browser."))
    cards = "".join(f'<section class="box sec"><span class="tag">{tag}</span><p><a href="{href}">{markup.escape(text)}</a></p></section>\n'
                    for tag, href, text in boxes)
    nav = "".join(f'<a href="{href}">{tag}</a>' for tag, href, _ in boxes)
    return f"""<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Caravel</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 28 17'%3E%3Cpath fill='%23ffaa29' d='{ship}'/%3E%3C/svg%3E">
<style>
{look}
  body {{ margin: 0; background: var(--bg); color: var(--text); font: 15px/1.6 -apple-system, BlinkMacSystemFont, "Helvetica Neue", Arial, sans-serif; }}
  main {{ max-width: 720px; margin: 0 auto; padding: 28px 16px 40px; }}
  p {{ margin: 0 0 12px; }}
  .box > :last-child {{ margin-bottom: 0; }}
  .hero {{ grid-column: 1 / -1; display: flex; flex-wrap: wrap; align-items: center; gap: 12px 24px; padding: 22px 16px 18px; }}
  .hero .ship {{ color: var(--amb); flex: none; }}
  .hero h1 {{ margin: 0; font: 700 28px/1.2 var(--mono); letter-spacing: .14em; text-transform: uppercase; color: var(--amb); }}
  .hero p {{ margin: 4px 0 0; color: var(--dim); }}
  .box a {{ color: var(--amb); }}
  .credits {{ margin: 0; color: var(--dim); font-size: 12px; line-height: 1.7; overflow-wrap: anywhere; }}
  .credits a {{ color: inherit; }}
</style>
<header class="site">
  <a class="logo" href="./" aria-label="Caravel"><svg class="ship" width="42" height="26" viewBox="0 0 28 17" aria-hidden="true"><path d="{ship}"/></svg>CARAVEL</a>
  <span class="pill">v{ver}</span>
  <nav aria-label="Caravel">{nav}<a href="https://github.com/bonszaimusic/caravel" target="_blank" rel="noopener">DOCS</a></nav>
</header>
<main>
<div class="boxes">
<section class="box hero">
  <span class="tag">FM-1 firmware</span>
  <svg class="ship" width="140" height="85" viewBox="0 0 28 17" aria-hidden="true"><path d="{ship}"/></svg>
  <div><h1>Caravel</h1><p>Multi-engine synth firmware for the FM-1</p></div>
</section>
{cards}</div>
<p class="credits">Caravel by bonszaimusic, based on Felucca 1.6.1 by Leo Kuroshita (@kurogedelic), <a href="https://hugelton.com" target="_blank" rel="noopener">H\u00fcgelton Instruments</a>. GPL-3.0, no warranty.<br>
M-VAVE and FM-1 are trademarks of their respective owners. Caravel is not affiliated with or endorsed by H\u00fcgelton Instruments or M-VAVE.<br>
<a href="firmware/LICENSING.md">LICENSING.md</a> \u00b7 <a href="firmware/NOTICE">NOTICE</a> \u00b7 <a href="firmware/CREDITS.md">CREDITS</a> \u00b7 <a href="firmware/LICENSE">GPL-3.0</a> \u00b7 <a href="{markup.escape(source_url, quote=True)}">Matching source</a> \u00b7 Felucca: <a href="https://github.com/hugelton/Felucca" target="_blank" rel="noopener">github.com/hugelton/Felucca</a></p>
</main>
"""


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
    label = " \u00b7 preview" if preview else ""
    look = look_css(text)
    text = site_chrome(text, version, emulator, label)
    if preview:
        text = text.replace('<h1>Caravel</h1>', '<h1>Caravel<small> (local preview; not release-verified)</small></h1>')
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
        (ed / "index.html").write_text(
            site_chrome((HERE / "editor.html").read_text(encoding="utf-8"), version, emulator, label), encoding="utf-8")
        for filename in ("fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js"):
            shutil.copyfile(HERE / filename, ed / filename)
        if emulator:
            dest = stage / "emulator"
            shutil.copytree(emulator, dest)
            emulator_source = "../firmware/" + source if not preview else "https://github.com/bonszaimusic/caravel"
            (dest / "index.html").write_text(source_href(emulator_html, emulator_source), encoding="utf-8")
        (stage / "index.html").write_text(
            landing(look, version, emulator, "firmware/" + source if not preview else source_url, label), encoding="utf-8")
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
