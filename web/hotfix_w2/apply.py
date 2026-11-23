#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Fail-closed W2 web-only installer overlay; never modify firmware or releases."""
import argparse
import hashlib
import html
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

# The releases this overlay is approved for: version -> (SHA-256, size) of caravel-<version>.fwsc.
# 1.1.0 is the release the W2 installer was reviewed on; 1.1.1 carries the same web installer text over
# (its installer protocol is unchanged: the USB update fix is firmware and loader only).
RELEASES = {
    '1.1.0': ('1e1a126ca1d7570f8fb917d381afaa6b78885fb3b08baa084775f3a948a570ba', 610138),
    '1.1.1': ('5998df51551eb487ef60bd5ad08c11725f0ffa049fb543ba7a58359a1b075fe4', 610193),
}
VERSION = '1.1.0'
FW_SHA, FW_SIZE = RELEASES[VERSION]
FW_NAME = 'caravel-' + VERSION + '.fwsc'
PATH = 'v1/webapp/installer/index.html'
APPROVED_BLOBS = {
    'index_pkg.html': '9dd110983422993a0fcd1f651bf8c9720bacb195',
    'fm1ota.js': '7be5eec30718cce6615c63a388a9deb40c3e1251',
}


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def select_release(version):
    """Bind the overlay to one approved release (default 1.1.0); anything else is refused."""
    global VERSION, FW_SHA, FW_SIZE, FW_NAME
    require(version in RELEASES, 'W2 is not approved for release ' + str(version))
    VERSION = version
    FW_SHA, FW_SIZE = RELEASES[version]
    FW_NAME = 'caravel-' + version + '.fwsc'


def read(path):
    path = Path(path)
    require(path.is_file() and not path.is_symlink(), f'not a regular file: {path}')
    return path.read_bytes()


def digest(data):
    return hashlib.sha256(data).hexdigest()


def blobsha(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


def inventory(root):
    require(root.is_dir() and not root.is_symlink(), 'invalid site root')
    result = {}
    for parent, dirs, files in os.walk(root, followlinks=False):
        for name in dirs + files:
            entry = Path(parent) / name
            require(not entry.is_symlink() and (entry.is_dir() or entry.is_file()),
                    f'symlink or special site entry: {entry}')
        for name in files:
            entry = Path(parent) / name
            result[entry.relative_to(root).as_posix()] = digest(read(entry))
    return dict(sorted(result.items()))


def module(src):
    src = re.sub(r'^export\s+', '', src, flags=re.M)
    return re.sub(r'^import .*?;\n', '', src, flags=re.M)


def parse_metadata(original):
    match = re.search(r'const meta = (\{[^;]+\});', original)
    require(match is not None, 'no installer JSON metadata')
    data = json.loads(match.group(1))
    require(data.get('version') == VERSION and data.get('release_verified') is True
            and data.get('breaking') is True and data.get('sha256') == FW_SHA
            and data.get('size') == FW_SIZE and data.get('pkg') == '../../firmware/' + FW_NAME,
            'existing installer is not bound to approved v1.1.0 release')
    link = re.search(r'<a id="matching-source" href="([^"]+)"', original)
    source_url = '../../firmware/caravel-' + VERSION + '-source.tar.gz'
    require(data.get('source') == source_url and link is not None
            and html.unescape(link.group(1)) == source_url,
            'missing matching release source archive link')
    return data, source_url


def source_commit(root):
    run = subprocess.run(['git', '-C', str(root), 'log', '-1', '--format=%H',
                          '--', 'web/hotfix_w2/index_pkg.html'],
                         capture_output=True, text=True, check=True)
    sha = run.stdout.strip()
    require(re.fullmatch(r'[0-9a-f]{40}', sha), 'cannot pin public matching web source')
    return sha


def overlay(site, source, tagged, release, rollback, receipt, public_commit=None):
    site, source, tagged, release = map(lambda x: Path(x).resolve(strict=True),
                                         (site, source, tagged, release))
    require(site.is_dir() and source.is_dir() and tagged.is_dir() and release.is_dir(),
            'required source directory missing')
    rollback, receipt = Path(rollback).absolute(), Path(receipt).absolute()
    require(not rollback.exists() and not receipt.exists(), 'rollback/receipt already exist')
    require(rollback.parent.is_dir() and receipt.parent.is_dir(), 'output directory missing')
    require(not rollback.parent.is_symlink() and not receipt.parent.is_symlink(),
            'unsafe output ancestor')
    require(all(not (site == p or site in p.parents or p in site.parents)
                for p in (rollback, receipt)), 'output overlaps site')
    snapshot = inventory(site)
    require(PATH in snapshot, 'v1 installer not found in dual-generation site')
    original = read(site / PATH).decode('utf-8')
    meta, firmware_source = parse_metadata(original)
    fwsite = read(site / 'v1/firmware' / FW_NAME)
    fwrel = read(release / FW_NAME)
    require(len(fwsite) == FW_SIZE and digest(fwsite) == FW_SHA and fwsite == fwrel,
            'published firmware was modified')
    source_name = 'caravel-' + VERSION + '-source.tar.gz'
    require(read(site / 'v1/firmware' / source_name) == read(release / source_name),
            'published source archive was modified')
    source_files = {name: read(source / 'web/hotfix_w2' / name)
                    for name in APPROVED_BLOBS}
    for name, expected in APPROVED_BLOBS.items():
        require(blobsha(source_files[name]) == expected,
                'unreviewed web source: ' + name)
    public_commit = public_commit or source_commit(source)
    require(re.fullmatch(r'[a-f0-9]{40}', public_commit), 'web source commit must be pinned')
    template = source_files['index_pkg.html'].decode('utf-8')
    for mark in ('/*LIB*/', '/*META*/', '@VERSION@'):
        require(template.count(mark) == 1, 'invalid W2 template marker ' + mark)
    library = '\n'.join(module((source_files['fm1ota.js'] if name == 'fm1ota.js'
                                else read(tagged / 'web' / name)).decode('utf-8'))
                        for name in ('generation.js', 'fm1pkg.js', 'fm1ota.js', 'fm1backup.js'))
    installer = template.replace('/*LIB*/', library).replace('/*META*/', json.dumps(meta))
    installer = installer.replace('@VERSION@', VERSION)
    installer, count = re.subn(r'(<a id="matching-source" href=")[^"]*(")',
                                lambda m: m[1] + html.escape(firmware_source, quote=True) + m[2], installer)
    require(count == 1 and installer.count('</main>') == 1, 'source link or main element mismatch')
    source_url = 'https://github.com/bonszaimusic/caravel/tree/' + public_commit + '/web/hotfix_w2'
    if VERSION == '1.1.0':
        stamp = ('<p id="site-hotfix" class="small">Firmware v1.1.0 · website W2 · '
                 '<a href="' + html.escape(source_url, quote=True) + '">Matching W2 web source</a>.'
                 ' Firmware package unchanged.</p>')
    else:                      # a later release: its firmware is its own, only the installer page text is W2's
        stamp = ('<p id="site-hotfix" class="small">Firmware v' + VERSION + ' · installer page with the W2 recovery '
                 'button · <a href="' + html.escape(source_url, quote=True) + '">Matching W2 web source</a>.</p>')
    installer = installer.replace('</main>', stamp + '\n</main>')
    require('WL80UBOOT1.00' in installer and 'WL82 UBOOT1.00' in installer
            and 'Enter UBOOT (no flash write)' in installer,
            'W2 UBOOT content missing')
    built = installer.encode('utf-8')
    require(digest(built) != snapshot[PATH], 'installer did not change')
    shutil.copytree(site, rollback)
    require(inventory(rollback) == snapshot, 'rollback copy differs from existing website')
    try:
        (site / PATH).write_bytes(built)
        after = inventory(site)
        require(set(after) == set(snapshot)
                and [p for p in after if after[p] != snapshot[p]] == [PATH],
                'W2 modified more than the installer')
        require(digest(read(site / 'v1/firmware' / FW_NAME)) == FW_SHA,
                'firmware SHA256 drift after W2')
        report = {'schema': 1, 'revision': 'W2', 'firmware': 'v' + VERSION,
                  'firmware_sha256': FW_SHA, 'public_web_source_commit': public_commit,
                  'firmware_source_url': firmware_source,
                  'approved_blobs': APPROVED_BLOBS, 'original': snapshot,
                  'overlay': after, 'changed': [PATH],
                  'rollback_verified': True, 'mflasher_integration_tested': False}
        receipt.write_text(json.dumps(report, sort_keys=True, indent=2) + '\n')
    except Exception:
        (site / PATH).write_bytes(original.encode('utf-8'))
        require(inventory(site) == snapshot, 'could not restore site after failure')
        raise
    print('W2 VERIFIED: only ' + PATH + ' changed; original site backed up; private receipt outside site')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('site', 'source', 'tagged', 'release', 'rollback', 'receipt'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--public-commit')
    parser.add_argument('--version', default='1.1.0')
    args = parser.parse_args()
    try:
        select_release(args.version)
        overlay(args.site, args.source, args.tagged, args.release,
                args.rollback, args.receipt, args.public_commit)
    except Exception as exc:
        sys.exit('W2 SITE RELEASE BLOCKED: ' + str(exc))


if __name__ == '__main__':
    main()
