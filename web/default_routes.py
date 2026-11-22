#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Publish a verified 1.x site at the default Caravel URLs without losing Classic.

Inputs are already verified by dual_site.py. This module only copies/rewrites
static site CONTENT. It never rebuilds firmware, flashes an FM-1 or deploys.
"""
from pathlib import Path
from urllib.parse import quote
import html
import json
import re
import shutil

BASE = "/caravel/"
ORIGIN = "https://bonszaimusic.github.io" + BASE
NAMESPACES = frozenset(("classic", "0.13", "v1", "versions", "versions.json"))
HTML_LINK = re.compile(r'''(?P<key>\b(?:href|src|action)\s*=\s*)(?P<q>["'])(?P<url>[^"']+)(?P=q)''', re.I)


def _remap(url: str, namespace: str) -> str:
    if url.startswith(ORIGIN):
        remainder = url[len(ORIGIN):]
        base = ORIGIN
    elif url.startswith(BASE):
        remainder = url[len(BASE):]
        base = BASE
    else:
        return url
    # Version-specific links, the public source repo and the version chooser
    # are intentional cross-generation/navigation links, not legacy links.
    if re.split(r"[/?#]", remainder, maxsplit=1)[0] in NAMESPACES:
        return url
    return base + namespace + "/" + remainder


def namespace_html_links(root: Path, namespace: str) -> int:
    """Fix Classic HTML links anchored to formerly-Classic default paths.

    Preserve all binary files byte-for-byte. Only static HTML href/src/action
    attributes are rewritten; never rewrite JavaScript or CSS source bodies.
    """
    count = 0
    for path in root.rglob("*.html"):
        source = path.read_text(encoding="utf-8")
        def sub(match):
            nonlocal count
            old = match.group("url")
            new = _remap(old, namespace)
            count += old != new
            return match.group("key") + match.group("q") + new + match.group("q")
        new = HTML_LINK.sub(sub, source)
        if new != source:
            path.write_text(new, encoding="utf-8")
    return count


def _archive_url(rel: Path) -> str:
    parts = rel.parts
    # Directory-indexed URLs keep their familiar trailing slash.
    if parts[-1] == "index.html":
        parts = parts[:-1]
    return BASE + "0.13/" + "/".join(quote(part, safe="") for part in parts) + ("/" if parts else "")


def redirect_html(target: str) -> str:
    """A same-origin, static Pages redirect preserving queries and fragments."""
    uri = json.dumps(target).replace("<", "\\u003c")
    safe = html.escape(target, quote=True)
    return ("<!doctype html>\n<meta charset=\"utf-8\">\n"
            '<meta name="robots" content="noindex">\n'
            f'<meta http-equiv="refresh" content="0;url={safe}">\n'
            f'<script>location.replace({uri}+location.search+location.hash)</script>\n'
            f'<p>Moved to <a href="{safe}">Caravel Classic 0.13.0</a>.</p>\n')


def preserve_old_root_urls(classic: Path, stage: Path) -> tuple[int, int]:
    """Keep non-colliding Classic downloads, redirect old-only HTML deep links.

    A path shared by both generations MUST use the current version at root.
    Explicit /0.13/ and /classic/ always serve the archived original.
    """
    files = redirects = 0
    for src in classic.rglob("*"):
        if not src.is_file():
            continue
        rel = src.relative_to(classic)
        if rel.parts[0] in NAMESPACES or rel == Path("index.html"):
            continue
        dst = stage / rel
        if dst.exists():
            if dst.is_dir():
                raise ValueError(f"classic file collides with current directory: {rel}")
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        if src.suffix.lower() in (".html", ".htm"):
            dst.write_text(redirect_html(_archive_url(rel)), encoding="utf-8")
            redirects += 1
        else:
            shutil.copyfile(src, dst)
            files += 1
    return files, redirects


def add_versions_link(path: Path) -> None:
    """Put an explicit Classic escape hatch on the live 1.x landing only."""
    content = path.read_text(encoding="utf-8")
    nav = ('<nav aria-label="Caravel generations" style="margin:2rem auto;padding:1rem;'
           'border:1px solid #aa852f;border-radius:8px">'
           '<strong>Caravel 1.x</strong> · '
           '<a href="/caravel/versions/">All versions</a> · '
           '<a href="/caravel/0.13/">Classic 0.13.0 and old backups</a>'
           '</nav>')
    if 'href="/caravel/versions/"' in content:
        return
    if "</main>" in content:
        content = content.replace("</main>", nav + "\n</main>", 1)
    elif "</body>" in content:
        content = content.replace("</body>", nav + "\n</body>", 1)
    else:
        raise ValueError("current landing has no </main> or </body> for version navigation")
    path.write_text(content, encoding="utf-8")


def activate(classic: Path, modern: Path, stage: Path) -> tuple[int, int]:
    """Make verified 1.x the default site and add a full /0.13/ alias.

    /classic/ and /v1/ are already staged by dual_site.build. We do not
    overwrite them, and we never change their WASM, firmware or backup data.
    """
    for src in (classic, modern):
        for reserved in NAMESPACES:
            if (src / reserved).exists():
                raise ValueError(f"site collides with generation namespace: {src / reserved}")
    if not (stage / "classic").is_dir() or not (stage / "v1").is_dir():
        raise ValueError("missing complete version namespaces")
    # Exact source copies are verified upstream; the default now hosts the
    # current release files, including its WASM and relative runtime assets.
    for entry in modern.iterdir():
        dest = stage / entry.name
        if dest.exists():
            raise ValueError(f"default route collides with reserved path: {entry.name}")
        if entry.is_dir():
            shutil.copytree(entry, dest)
        elif entry.is_file():
            shutil.copyfile(entry, dest)
        else:
            raise ValueError(f"unsupported site entry: {entry}")
    shutil.copytree(classic, stage / "0.13")
    if (stage / "0.13/release.html").exists():
        raise ValueError("Classic alias release.html collision")
    (stage / "0.13/index.html").rename(stage / "0.13/release.html")
    (stage / "0.13/index.html").write_text(
        (stage / "classic/index.html").read_text(encoding="utf-8"), encoding="utf-8")
    # /0.13/ and /classic/ have distinct roots and stable relative assets.
    # Only rewrite absolute HTML links that formerly returned to the Classic
    # default URLs. The frozen input itself is NEVER changed.
    namespace_html_links(stage / "classic", "classic")
    namespace_html_links(stage / "0.13", "0.13")
    add_versions_link(stage / "index.html")
    return preserve_old_root_urls(classic, stage)


def version_chooser(html_page: str) -> str:
    """The existing chooser uses root-relative links; host it at /versions/."""
    if "<head>" not in html_page:
        raise ValueError("version chooser lacks <head>")
    return html_page.replace("<head>", f'<head><base href="{BASE}">', 1)
