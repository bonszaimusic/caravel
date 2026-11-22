#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Compose two independently verified, complete Caravel sites for GitHub Pages.

This is NOT a release builder. Classic must match the frozen published byte
inventory: never regenerate it. A modern input must be independently release
verified. No deployment or GitHub write occurs here.

  python3 web/dual_site.py --classic site-013 --out site-dual
  python3 web/dual_site.py --classic site-013 --v1 site-100 --out site-dual
"""
import argparse
import hashlib
import html
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import frozen_classic
import default_routes

CLASSIC = "0.13.0"
SITE_ORIGIN = "https://bonszaimusic.github.io/caravel/"
REQUIRED_PAGES = ("index.html", "webapp/installer/index.html",
                  "webapp/editor/index.html", "emulator/index.html",
                  "tutorials/index.html", "firmware/SHA256SUMS")


def verify_generation_behavior(root: Path) -> dict:
    result = subprocess.run(["node", str(Path(__file__).with_name("check_generation_site.mjs")), str(root)],
                            capture_output=True, text=True, timeout=30)
    if result.returncode:
        raise ValueError("1.x generation behavior check failed: " + result.stderr[-1500:])
    proof = json.loads(result.stdout)
    if proof.get("result") != "PASS":
        raise ValueError("1.x generation behavior not verified")
    return proof


def verify_site(root: Path, version: str) -> dict:
    """Fail closed on unverified/partial/mismatched or linked release sites."""
    if not root.is_dir() or root.is_symlink():
        raise ValueError(f"missing/linked site: {root}")
    for parent, dirs, files in os.walk(root, followlinks=False):
        for item in dirs + files:
            if (Path(parent) / item).is_symlink():
                raise ValueError(f"symlink forbidden in site: {Path(parent) / item}")
    for name in REQUIRED_PAGES:
        if not (root / name).is_file():
            raise ValueError(f"missing site artifact: {name}")
    if not re.fullmatch(r"(?:0\.13\.0|1\.[0-9]+\.[0-9]+)", version):
        raise ValueError(f"unsupported release generation: {version}")
    landing = (root / "index.html").read_text(encoding="utf-8")
    installer = (root / "webapp/installer/index.html").read_text(encoding="utf-8")
    emulator = (root / "emulator/index.html").read_text(encoding="utf-8")
    if f'<span class="pill">v{version}</span>' not in landing:
        raise ValueError("site landing/version mismatch")
    if f'<small class="version">v{version}</small>' not in emulator:
        raise ValueError("emulator/version mismatch")
    if '"release_verified": true' not in installer or "local preview" in installer:
        raise ValueError("unverified/preview installer refused")
    # The embedded installer preflight must be tied to the package physically
    # staged in this site. Do not infer trust from a version label alone.
    package = root / "firmware" / f"caravel-{version}.fwsc"
    if not package.is_file():
        raise ValueError(f"missing firmware: {package.name}")
    payload = package.read_bytes()
    digest = hashlib.sha256(payload).hexdigest()
    if (f'"version": "{version}"' not in installer or
            f'"pkg": "../../firmware/{package.name}"' not in installer or
            f'"sha256": "{digest}"' not in installer or
            f'"size": {len(payload)}' not in installer):
        raise ValueError("installer package preflight metadata mismatch")
    manifest = (root / "firmware/SHA256SUMS").read_text(encoding="ascii")
    if f"{digest}  {package.name}\n" not in manifest:
        raise ValueError("package digest does not match SHA256SUMS")
    if not (root / "emulator/felucca.wasm").read_bytes().startswith(b"\0asm\x01\0\0\0"):
        raise ValueError("invalid emulator WASM")
    if version.startswith("1."):
        # Verify the actual breaking-update guard, not incidental localized prose.
        # Runtime simulated-MIDI behavior is covered by web/test_breaking_behavior.mjs.
        controls = ('id="breaking"', 'id="brk-understand"',
                    'id="brk-backup"')
        if not re.search(r'"breaking"\s*:\s*true\b', installer):
            raise ValueError("1.x installer missing breaking=true metadata")
        if any(control not in installer for control in controls):
            raise ValueError("1.x installer missing breaking-update acknowledgement controls")
        guards = (
            r'const\s+breaking\s*=\s*!!meta\.breaking',
            r'const\s+brkReady\s*=\s*\(\)\s*=>\s*!breaking\s*\|\|\s*\(\s*\$\("brk-understand"\)\.checked\s*&&\s*\$\("brk-backup"\)\.checked\s*\)',
            r'\$\("go"\)\.disabled\s*=\s*on\s*\|\|\s*!image\s*\|\|\s*!brkReady\(\)',
            r'if\s*\(writing\s*\|\|\s*!image\s*\|\|\s*!brkReady\(\)\)\s*return\s*;',
            r'if\s*\(breaking\)[^{]*\{.*?captureBackup\(.*?confirm\(t\("brkConfirm"\)\).*?await up\.install\(image,\s*meta\.product,\s*step(?:\s*,\s*\{[^}]*\})?\s*\)',
        )
        if not all(re.search(expr, installer, re.S) for expr in guards):
            raise ValueError("1.x installer missing functional breaking-update guard")
        verify_generation_behavior(root)
    return {"version": version, "sha256": digest, "bytes": len(payload)}


def rewrite_v1_links(root: Path) -> None:
    """Keep absolute legacy site links inside v1 instead of crossing to Classic."""
    for path in root.rglob("*.html"):
        original = path.read_text(encoding="utf-8")
        updated = re.sub(re.escape(SITE_ORIGIN) + r"(?!classic/|v1/|0\.13/|versions/)", SITE_ORIGIN + "v1/", original)
        updated = re.sub(r'(?<=href=")/caravel/(?!classic/|v1/|0\.13/|versions/)', '/caravel/v1/', updated)
        if updated != original:
            path.write_text(updated, encoding="utf-8")


def landing(with_v1: bool) -> str:
    modern = ('''<a class="card" href="v1/"><small>NEW GENERATION</small>
      <strong>Caravel 1.x</strong><span>PLAY / BUILD, CAPTURE and FORMAT 2</span>
      <span>Installer · Emulator · Editor · Tutorials</span></a>''' if with_v1 else
      '''<div class="card pending" aria-disabled="true"><small>IN DEVELOPMENT</small>
      <strong>Caravel 1.x</strong><span>PLAY / BUILD, CAPTURE and FORMAT 2</span>
      <span>Not released yet — no firmware download</span></div>''')
    academy = ('''<a class="card academy" href="v1/emulator/#stage-1"><small>NEW · CARAVEL ACADEMY</small>
      <strong>Your first ambient track</strong>
      <span>Interactive lessons with the real firmware in your browser. <b>Stage 1 is live: Strings before anything else.</b> More stages open together with the YouTube series.</span>
      <span>Start Stage 1 →</span></a>''' if with_v1 else "")
    note = ('''<p>Default links such as <a href="emulator/">/emulator/</a>,
    <a href="webapp/installer/">/webapp/installer/</a> and
    <a href="tutorials/">/tutorials/</a> serve current Caravel 1.x.
    The complete frozen 0.13.0 site is available under <a href="0.13/">/0.13/</a>
    (and the older <a href="classic/">/classic/</a> alias).
    Version-unique old download URLs remain available.</p>''' if with_v1 else
    '''<p>Caravel 1.x is not yet a verified input to this site build.
    Default links continue to serve Classic 0.13.0 until a verified 1.x
    release is available. The <a href="0.13/">/0.13/</a> and
    <a href="classic/">/classic/</a> archives are also available.</p>''')
    return f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Caravel — choose your generation</title>
<style>
:root{{color-scheme:dark;font:16px/1.55 system-ui,Arial,sans-serif;background:#110e09;color:#f3e6d0}}
body{{margin:0 auto;max-width:850px;padding:32px 20px 60px}}
header{{border-bottom:1px solid #554129;padding-bottom:18px;margin-bottom:32px}}
h1{{color:#ffaa29;letter-spacing:.15em;margin:0;font-size:2.1rem}}
h2{{font-size:1.15rem}}p{{color:#d1c3ab}}
.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:20px}}
.card{{display:flex;flex-direction:column;gap:12px;border:1px solid #7c603b;border-radius:12px;
background:#21190f;color:#f3e6d0;padding:22px;text-decoration:none}}
a.card:hover,a.card:focus-visible{{outline:2px solid #ffaa29;outline-offset:2px}}
.card small{{color:#ffaa29;letter-spacing:.1em;font-weight:700}}
.card strong{{font-size:1.5rem}}.card span{{font-size:.93rem;color:#dcc9a8}}
.academy{{margin-top:20px;border-color:#dcff31;background:#1d2410}}.academy small{{color:#dcff31}}.pending{{opacity:.62;border-style:dashed}}.warning{{border-left:3px solid #ffaa29;padding:10px 18px;margin-top:28px}}
a{{color:#ffbd58}}
</style></head><body>
<header><h1>CARAVEL</h1><p>Independent firmware for the M-VAVE FM-1</p></header>
<main><h2>Choose your firmware generation</h2><div class="grid">
<a class="card" href="0.13/"><small>LEGACY / CLASSIC · NO LONGER MAINTAINED</small><strong>Caravel 0.13.0</strong>
<span>Original HOME / ACTION workflow and legacy engines</span>
<span><strong>No longer maintained · nicht mehr gepflegt</strong><br>Frozen at 0.13.0; no further fixes or builds. Downloads remain available.</span>
<span>Installer · Emulator · Editor · Tutorials</span></a>
{modern}</div>
{academy}
<div class="warning"><strong>Important: Caravel 1.x is a breaking release.</strong>
<p>FORMAT 2 cannot open older projects or backup files. Before upgrading, export and verify a complete backup with your matching Classic editor. Keep the original firmware, editor and backup for a return to Classic. Firmware reinstall alone does not restore old projects.</p></div>
{note}
<p><a href="https://github.com/bonszaimusic/caravel/releases">GitHub Releases</a> · <a href="https://github.com/bonszaimusic/caravel">Source and documentation</a></p>
</main></body></html>\n'''


def classic_landing() -> str:
    return '''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Caravel Classic — no longer maintained</title>
<style>:root{color-scheme:dark;font:18px/1.6 system-ui;background:#110e09;color:#f3e6d0}
body{max-width:760px;margin:40px auto;padding:20px}a{color:#ffbd58}
.warning{border-left:4px solid #ffaa29;padding:12px 24px;background:#21190f}</style></head>
<body><main><h1>Caravel Classic 0.13.0</h1>
<div class="warning"><strong>No longer maintained · nicht mehr gepflegt</strong>
<p>Classic is frozen at 0.13.0. There will be no further fixes or builds, including no 0.13.1.
Fixes continue in Caravel 1.x. Existing Classic downloads remain available.</p></div>
<p><a href="release.html">Original 0.13.0 release page</a></p>
<ul><li><a href="webapp/installer/">Classic installer</a></li>
<li><a href="webapp/editor/">Classic editor</a></li>
<li><a href="emulator/">Classic emulator</a></li>
<li><a href="tutorials/">Classic tutorials</a></li></ul>
<p>Classic and Caravel 1.x use incompatible project, preset and backup formats.
Keep a complete Classic backup and the matching firmware and editor before upgrading.
Reinstalling firmware alone does not restore your old data.</p>
<p><a href="../versions/">Choose your generation</a></p></main></body></html>\n'''


def build(classic: Path, modern: Path | None, out: Path,
          classic_manifest: Path = frozen_classic.MANIFEST) -> None:
    frozen_classic.verify(classic, classic_manifest)
    classic = classic.resolve(strict=True)
    modern = modern.resolve(strict=True) if modern is not None else None
    out = out.absolute()
    if out.exists() or out.is_symlink():
        raise ValueError("output already exists: refuse overwrite")
    out = out.resolve()
    if any(out == src or out.is_relative_to(src) or src.is_relative_to(out)
           for src in (classic, modern) if src is not None):
        raise ValueError("output cannot contain or overlap input site")
    klass = verify_site(classic, CLASSIC)
    newer = None
    if modern:
        inst = (modern / "webapp/installer/index.html").read_text(encoding="utf-8")
        match = re.search(r'"version": "(1\.[0-9]+\.[0-9]+)"', inst)
        if not match:
            raise ValueError("v1 input must be a published 1.x site")
        newer = verify_site(modern, match[1])
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".caravel-dual-", dir=out.parent) as temporary:
        stage = Path(temporary) / "site"
        shutil.copytree(classic, stage / "classic")
        if (classic / "release.html").exists():
            raise ValueError("Classic release.html collides with preserved original landing")
        (stage / "classic/index.html").rename(stage / "classic/release.html")
        (stage / "classic/index.html").write_text(classic_landing(), encoding="utf-8")
        if modern:
            shutil.copytree(modern, stage / "v1")
            rewrite_v1_links(stage / "v1")
            # The full verified 1.x site now serves default /caravel/... paths.
            # Legacy archive bytes and old-only deep links remain accessible.
            default_routes.activate(classic, modern, stage)
            # Do not accept a copied but non-functional default site.
            installed = verify_site(stage, newer["version"])
            if installed != newer:
                raise ValueError("default 1.x release identity diverges")
        else:
            # Fail-safe when no verified 1.x release exists: keep Classic at
            # the default paths rather than exposing an unverified installer.
            for entry in classic.iterdir():
                if entry.name == "index.html":
                    continue
                if entry.name in ("classic", "0.13", "v1", "versions", "versions.json"):
                    raise ValueError("Classic entry collides with generation namespace")
                if entry.is_dir():
                    shutil.copytree(entry, stage / entry.name)
                else:
                    shutil.copyfile(entry, stage / entry.name)
            (stage / "index.html").write_text(landing(False), encoding="utf-8")
            shutil.copytree(classic, stage / "0.13")
            if (stage / "0.13/release.html").exists():
                raise ValueError("Classic alias release.html collision")
            (stage / "0.13/index.html").rename(stage / "0.13/release.html")
            (stage / "0.13/index.html").write_text(classic_landing(), encoding="utf-8")
            default_routes.namespace_html_links(stage / "0.13", "0.13")
        (stage / "versions").mkdir()
        (stage / "versions/index.html").write_text(
            default_routes.version_chooser(landing(bool(modern))), encoding="utf-8")
        versions = {"classic": {"path": "classic/", **klass},
                    "0.13": {"path": "0.13/", **klass},
                    "v1": {"path": "v1/", **newer} if newer else None,
                    "default": {"path": "", **(newer or klass)}}
        (stage / "versions.json").write_text(json.dumps(versions, indent=2) + "\n", encoding="utf-8")
        stage.rename(out)
    print(f"Dual site verified: Classic {CLASSIC}; v1 {newer['version'] if newer else 'not published'}; {out}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--classic", type=Path, required=True)
    parser.add_argument("--v1", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    try:
        build(args.classic, args.v1, args.out)
    except (ValueError, OSError, UnicodeError) as error:
        parser.exit(1, f"dual-site verification failed: {error}\n")


if __name__ == "__main__":
    main()
