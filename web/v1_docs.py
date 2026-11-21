#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""Add only pinned 1.x candidate guides to an existing verified private site.

No firmware/emulator/Classic build, deployment, or source-tree mutation. The
source commit must equal the site's release-build.json source commit. Only the
1.x landing changes: candidate status, guide entry, and the current editor blurb.
"""
import argparse
import hashlib
import html
import json
import os
from pathlib import Path
import posixpath
import re
import shutil
import subprocess
import tempfile
from urllib.parse import unquote, urlsplit

GUIDES = ("start-here", "basics", "engine-analog", "engine-phys", "engine-sem", "engine-hades", "engine-drone", "engine-ensemble",
          "seq-arp", "capture", "home-performance", "harmony-live", "elastic-first-steps", "elastic-linked-springs",
          "elastic-ambient-recipes", "filter-fx", "midi-sync", "help", "home-and-stasis", "random-morph-journey")
DIAGRAMS = ("01_audio_routing", "02_sem", "03_hades", "04_drone", "05_ensemble", "06_analog", "07_phys")
ART = "docs/tutorials/1-0-audio-engine-diagrams/"
PAGES = {**{f"docs/tutorials/{name}.md": f"tutorials/{name}/index.html" for name in GUIDES},
         "docs/release-110.md": "tutorials/release-notes/index.html",
         "docs/engines/overview.md": "tutorials/engines/index.html",
         "docs/engines/tips.md": "tutorials/engine-tips/index.html",
         ART + "README.md": "tutorials/audio-engine-diagrams/index.html"}
PIN = re.compile(r"[0-9a-f]{40}")
STYLE = """:root{color-scheme:dark;font:17px/1.65 system-ui;background:#110e09;color:#f3e6d0}
body{max-width:1040px;margin:auto;padding:24px}a{color:#ffbd58}nav a{margin-right:18px}
.status{border-left:4px solid #ffaa29;background:#21190f;padding:14px 20px}h1,h2{line-height:1.25}
img{max-width:100%;height:auto}figure{margin:24px 0}figcaption,.reference{font-size:.85rem;color:#cabd9f}
table{border-collapse:collapse;width:100%}th,td{border:1px solid #66533b;padding:8px;text-align:left}
pre{overflow:auto;padding:14px;background:#21190f}code{background:#21190f}blockquote{border-left:2px solid #66533b;padding-left:20px}
footer{margin-top:32px;border-top:1px solid #66533b;padding-top:16px}li{margin:.35em 0}"""
OLD_EDITOR_BLURB = "Edit sounds, sequences, banks and samples from the computer."
NEW_EDITOR_BLURB = "Edit sounds, sequences and user presets from the computer."


def digest(data):
    return hashlib.sha256(data).hexdigest()


def tree(root):
    """Exact byte inventory; reject links/special files before copying."""
    result = {}
    if root.is_symlink() or not root.is_dir():
        raise ValueError("site must be a regular directory")
    for parent, dirs, files in os.walk(root, followlinks=False):
        for name in dirs + files:
            path = Path(parent) / name
            if path.is_symlink() or not (path.is_dir() or path.is_file()):
                raise ValueError("linked/special site entry refused")
        for name in files:
            path = Path(parent) / name
            result[path.relative_to(root).as_posix()] = digest(path.read_bytes())
    return dict(sorted(result.items()))


def read_source(repo, commit, name):
    entry = subprocess.check_output(["git", "-C", str(repo), "ls-tree", commit, "--", name], text=True).strip()
    if not entry or entry.split()[0] != "100644" or entry.split("\t", 1)[1] != name:
        raise ValueError("missing/nonregular selected source: " + name)
    payload = subprocess.check_output(["git", "-C", str(repo), "show", f"{commit}:{name}"])
    return payload, entry.split()[2]


def source_path(current, target):
    path = posixpath.normpath(posixpath.join(posixpath.dirname(current), unquote(target)))
    if path.startswith("../") or path.startswith("/") or "\\" in path:
        raise ValueError("unsafe documentation reference")
    return path


def title(text):
    return next((line[2:].strip() for line in text.splitlines() if line.startswith("# ")), "Candidate guide")


def relative(current, target):
    return posixpath.relpath(target, posixpath.dirname(current))


def page_shell(label, body, output, commit, published=False, version="1.0.0"):
    esc = html.escape
    nav = [("Candidate guides", "tutorials/index.html"), ("Academy", "emulator/index.html#stage-1"), ("Installer", "webapp/installer/index.html"),
           ("Editor", "webapp/editor/index.html"), ("Emulator", "emulator/index.html")]
    result = (f'<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
            f'<title>{esc(label)} — Caravel 1.x candidate</title><style>{STYLE}</style></head><body><nav>'
            + " ".join(f'<a href="{relative(output, target)}">{text}</a>' for text, target in nav)
            + '</nav><aside class="status"><strong>Caravel 1.x candidate — not published</strong>'
              '<br>Private candidate documentation; publication review remains open.'
              '<br>FM-1 hardware acceptance is still open. Host and source checks are not device or listening tests.'
              '<br>FORMAT 2 requires matching projects, presets and backups. Keep the complete Classic backup saved before upgrading.</aside>'
            + f'<main><h1>{esc(label)}</h1>{body}</main><footer>Documentation source <code>{commit}</code>. '
            + f'<a href="{relative(output, "tutorials/SOURCE.json")}">Source and byte-preservation receipt</a> · '
            + f'<a href="{relative(output, f"firmware/caravel-{version}-source.tar.gz")}">Matching candidate source archive</a>'
              '</footer></body></html>\n')
    if published:
        result = result.replace("Candidate guides", "Guides").replace("Caravel 1.x candidate", f"Caravel {version}")
        result = result.replace(" — not published", "")
        result = result.replace("Private candidate documentation; publication review remains open.", "Release documentation.")
        result = result.replace("Matching candidate source archive", "Matching source archive")
    return result


def candidate_landing(text, published=False, version="1.0.0"):
    if text.count("<main>") != 1 or text.count(OLD_EDITOR_BLURB) != 1:
        raise ValueError("candidate landing markers changed")
    notice = ('<section class="box risk" id="v1-candidate-status"><span class="tag">PRIVATE CANDIDATE</span>'
              '<h2>Caravel 1.x candidate — not published</h2>'
              '<p>FM-1 hardware acceptance is still open. Publication requires separate approval.</p>'
              '<p><a href="tutorials/">TUTORIALS — current 1.x candidate guides and engine diagram drafts</a></p></section>')
    if published:
        notice = ('<section class="box" id="v1-release-status"><span class="tag">CARAVEL ' + version + '</span>'
                  '<h2>Six engines, BUILD / PLAY and AFTERBURNER</h2>'
                  '<p><a href="emulator/#stage-1"><strong>NEW · CARAVEL ACADEMY — Stage 1 is live:</strong> Strings before anything else</a>. '
                  'Interactive lessons in the browser emulator; more stages open together with the YouTube series.</p>'
                  '<p><a href="tutorials/">TUTORIALS — 1.x guides and engine diagrams</a> · '
                  '<a href="tutorials/release-notes/">Release notes and compatibility</a></p>'
                  '<p>Back up with the old matching tools before upgrading. Pre-1.0 data cannot be imported.</p></section>')
    return text.replace("<main>", "<main>\n" + notice, 1).replace(OLD_EDITOR_BLURB, NEW_EDITOR_BLURB)


def render(text, source, output, assets):
    esc = html.escape
    def reference(match):
        image, label, url = match[1], match[2], html.unescape(match[3])
        parts = urlsplit(url)
        if parts.scheme:
            if parts.scheme not in ("http", "https") or image:
                raise ValueError("unsupported external documentation reference")
            return f'<a href="{esc(url, quote=True)}">{label}</a>'
        if url.startswith("#"):
            return f'<a href="{esc(url, quote=True)}">{label}</a>'
        path = source_path(source, parts.path)
        if image:
            if path not in assets:
                raise ValueError("image outside selected 1.x assets: " + path)
            return f'<img src="{relative(output, assets[path])}" alt="{label}" loading="lazy">'
        target = PAGES.get(path) or assets.get(path)
        if target:
            return f'<a href="{relative(output, target)}">{label}</a>'
        # Do not republish older engine/installation/FAQ pages as current 1.x docs.
        return f'<span class="reference">{label} (source reference: <code>{esc(path)}</code>; see the matching source archive)</span>'

    def inline(line):
        line = esc(line)
        line = re.sub(r"(!?)\[([^\]]+)\]\(([^)\s]+)\)", reference, line)
        line = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", line)
        return re.sub(r"`([^`]+)`", r"<code>\1</code>", line)

    lines = text.splitlines(); result = []; at = 0
    while at < len(lines):
        line = lines[at].strip(); at += 1
        if not line or line.startswith("<!--") or line.startswith("# "):
            continue
        if line.startswith("```"):
            code = []
            while at < len(lines) and not lines[at].startswith("```"):
                code.append(lines[at]); at += 1
            at += 1; result.append("<pre>" + esc("\n".join(code)) + "</pre>"); continue
        heading = re.match(r"(#{2,6}) (.+)", line)
        if heading:
            level = len(heading[1]); result.append(f"<h{level}>{inline(heading[2])}</h{level}>"); continue
        if line.startswith("|"):
            rows = [line]
            while at < len(lines) and lines[at].strip().startswith("|"):
                rows.append(lines[at].strip()); at += 1
            rows = [row for row in rows if not re.fullmatch(r"\|[\s|:-]+\|", row)]
            result.append("<table>" + "".join("<tr>" + "".join(f'<{"th" if i == 0 else "td"}>{inline(cell.strip())}</{"th" if i == 0 else "td"}>'
                          for cell in row.strip("|").split("|")) + "</tr>" for i, row in enumerate(rows)) + "</table>"); continue
        if re.match(r"(?:- |\d+\. )", line):
            ordered = bool(re.match(r"\d+\. ", line)); items = [line]
            while at < len(lines) and re.match(r"(?:- |\d+\. )", lines[at].strip()):
                items.append(lines[at].strip()); at += 1
            tag = "ol" if ordered else "ul"
            result.append(f"<{tag}>" + "".join("<li>" + inline(re.sub(r"^(?:- |\d+\. )", "", item)) + "</li>" for item in items) + f"</{tag}>"); continue
        if line.startswith(">"):
            result.append("<blockquote>" + inline(line.lstrip("> ")) + "</blockquote>"); continue
        paragraph = [line]
        while at < len(lines) and lines[at].strip() and not re.match(r"(?:#{1,6} |\||>|- |\d+\. |```|<!--)", lines[at].strip()):
            paragraph.append(lines[at].strip()); at += 1
        result.append("<p>" + inline(" ".join(paragraph)) + "</p>")
    return "\n".join(result)


def build(site, source, commit, out, published=False):
    if not PIN.fullmatch(commit):
        raise ValueError("full source commit required")
    if Path(site).is_symlink() or Path(source).is_symlink():
        raise ValueError("linked site/source root refused")
    site, source, out = Path(site).resolve(), Path(source).resolve(), Path(out).absolute()
    if out.exists() or out.is_symlink():
        raise ValueError("output exists; refuse overwrite")
    out = out.resolve()
    if any(out == root or out.is_relative_to(root) or root.is_relative_to(out) for root in (site, source)):
        raise ValueError("output overlaps input")
    before = tree(site)
    record = json.loads((site / "firmware/release-build.json").read_text())
    if record.get("source", {}).get("commits", {}).get(".") != commit or not re.fullmatch(r"1\.[0-9]+\.[0-9]+", str(record.get("version"))):
        raise ValueError("site/source commit or version mismatch")
    if (site / "tutorials").exists():
        raise ValueError("refuse to replace existing tutorials")
    sources = {}; assets = {}
    for name in PAGES:
        data, blob = read_source(source, commit, name); sources[name] = (data, blob)
    for name, (data, _) in list(sources.items()):
        for url in re.findall(r"!\[[^\]]*\]\(([^)\s]+)\)", data.decode()):
            path = source_path(name, url)
            if not (re.fullmatch(r"docs/tutorials/img/one-[a-z-]+\.png", path) or
                    path in {ART + diagram + ext for diagram in DIAGRAMS for ext in (".svg", ".png")}):
                raise ValueError("noncandidate image selected: " + path)
            assets[path] = "tutorials/assets/" + path.removeprefix("docs/tutorials/")
    for diagram in DIAGRAMS:
        for ext in (".svg", ".png"):
            name = ART + diagram + ext; assets[name] = "tutorials/assets/" + name.removeprefix("docs/tutorials/")
    for name in assets:
        sources[name] = read_source(source, commit, name)
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".v1-docs-", dir=out.parent) as temporary:
        stage = Path(temporary) / "site"; shutil.copytree(site, stage)
        (stage / "index.html").write_text(candidate_landing((site / "index.html").read_text(encoding="utf-8"), published, record["version"]), encoding="utf-8")
        receipt = {"schema": 1, "kind": "published-1.x-documentation" if published else "private-1.x-candidate-documentation", "source_commit": commit,
                   "hardware_acceptance": "open", "publication_review": "owner-approved" if published else "open", "not_a_deployment": True,
                   "sources": {}, "original_site_sha256": before}
        for name, (data, blob) in sorted(sources.items()):
            target = assets.get(name) or "tutorials/source/" + name
            path = stage / target; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data)
            receipt["sources"][name] = {"git_blob": blob, "sha256": digest(data), "bytes": len(data), "copy": target}
        receipt["selected_source_sha256"] = digest(json.dumps(receipt["sources"], sort_keys=True,
                                                              separators=(",", ":")).encode("utf-8"))
        for name, output in PAGES.items():
            text = sources[name][0].decode("utf-8")
            body = render(text, name, output, assets)
            if name == ART + "README.md":
                gallery = '<h2>Original diagram drafts</h2><p>Source-reviewed teaching schematics, not circuit diagrams or device measurements. Final release-SHA and browser review remain open.</p>'
                for diagram in DIAGRAMS:
                    png, svg = (assets[ART + diagram + ext] for ext in (".png", ".svg"))
                    gallery += f'<figure><img src="{relative(output, png)}" alt="{diagram}" loading="lazy"><figcaption>Documentation draft · <a href="{relative(output, svg)}">Original SVG</a> · <a href="{relative(output, png)}">Original PNG</a></figcaption></figure>'
                body = gallery + body
            path = stage / output; path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(page_shell(title(text), body, output, commit, published, record["version"]), encoding="utf-8")
        academy = ('<li><a href="' + relative("tutorials/index.html", "emulator/index.html") + '#stage-1"><strong>Caravel Academy (interactive, in the emulator):</strong> '
                   'Stage 1 · Strings before anything else</a> — more stages open together with the YouTube series</li>') if published else ""
        links = "<ul>" + academy + "".join(f'<li><a href="{relative("tutorials/index.html", target)}">{html.escape(title(sources[name][0].decode()))}</a></li>' for name, target in PAGES.items()) + "</ul>"
        (stage / "tutorials/index.html").write_text(page_shell(f"Caravel {record['version']} guides" if published else "Caravel 1.x candidate guides", links, "tutorials/index.html", commit, published, record["version"]), encoding="utf-8")
        after = tree(stage)
        if any(after.get(name) != value for name, value in before.items() if name != "index.html"):
            raise ValueError("an existing site file changed")
        receipt["original_site_files_unchanged"] = len(before) - 1
        receipt["landing_index_sha256"] = {"before": before["index.html"], "after": after["index.html"],
                                            "reason": "release status, tutorials entry, six-engine editor description" if published else "private candidate status, tutorials entry, six-engine editor description"}
        receipt["unchanged_runtime_subtrees"] = {}
        for prefix in ("firmware/", "webapp/editor/", "emulator/"):
            selected = {name: sha for name, sha in before.items() if name.startswith(prefix)}
            receipt["unchanged_runtime_subtrees"][prefix] = {"files": len(selected),
                "sha256": digest(json.dumps(selected, sort_keys=True, separators=(",", ":")).encode())}
        receipt["added_files_sha256"] = {name: value for name, value in after.items() if name not in before}
        (stage / "tutorials/SOURCE.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
        stage.rename(out)
    if tree(site) != before:
        raise ValueError("input site changed during assembly")
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("site", "source", "out"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--published", action="store_true", help="assemble release documentation after publication approval")
    args = parser.parse_args()
    try:
        receipt = build(args.site, args.source, args.commit, args.out, args.published)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"1.x documentation assembly refused: {error}\n")
    print(json.dumps({"source_commit": receipt["source_commit"], "original_site_files_unchanged": receipt["original_site_files_unchanged"],
                      "source_files": len(receipt["sources"]), "added_files": len(receipt["added_files_sha256"]) + 1}, sort_keys=True))


if __name__ == "__main__":
    main()
