<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Caravel Pages: current and frozen generations

This describes the **review candidate** default-URL routing produced by
`web/dual_site.py`. It is not an instruction to deploy an unverified
release. The generator uses two already verified, independently complete
static release sites and does not build or change firmware.

## URLs after an explicitly approved Pages deployment

| Path | Content |
| --- | --- |
| `/caravel/` | Current, verified Caravel 1.x landing page |
| `/caravel/emulator/` | Current Caravel 1.x emulator and its WASM/assets |
| `/caravel/webapp/installer/` | Current Caravel 1.x installer with its breaking-update backup/acknowledgement guard |
| `/caravel/webapp/editor/` | Current Caravel 1.x editor |
| `/caravel/tutorials/` | Current, publicly released 1.x tutorials |
| `/caravel/v1/...` | Complete, version-line-stable 1.x site; existing 1.x links continue to work |
| `/caravel/0.13/...` | Complete archived 0.13.0 site, including emulator, matching editor/installer, tutorials and firmware |
| `/caravel/classic/...` | Legacy archived 0.13.0 alias, retained |
| `/caravel/versions/` | Generation chooser with direct links to both versions |
| `/caravel/versions.json` | Existing `classic` and `v1` metadata plus `0.13` alias and `default` |

A default route contains **the actual 1.x files**, not a thin HTML redirect;
therefore WASM, editor modules, installer binaries and Academy deep links work
under both `/caravel/` and `/caravel/v1/`. These paths share the origin and
the same `caravel-v1-*` browser storage keys. Older 0.13's `felucca-*`
keys remain isolated; never rename or migrate those stores implicitly.

Previously published **version-unique** old binary/download paths remain
byte-identical under the root where they do not collide with a current 1.x
file; they are also always present under `/0.13/` and `/classic/`.
A root `/firmware/SHA256SUMS`, which exists in *both* generations, belongs
to **current 1.x**. Read `/0.13/firmware/SHA256SUMS` for the classic inventory.
Old-only HTML URLs get a narrow same-origin redirect to their corresponding
`/0.13/` path and preserve query and hash. If an HTML URL exists in both
generations, the default path intentionally means 1.x; use explicit
`/0.13/` to see the older page. Classic HTML absolute site links are
rewritten *only in the derived version-namespaced site* so navigation does
not silently jump to 1.x. Frozen input snapshots and binary assets are never
rewritten.

## Verification and release boundaries

The build refuses symlinks, missing/invalid firmware, mismatched package
hashes or versions, a missing/ineffective 1.x installer acknowledgement guard,
and unavailable real generation-discovery behavior. The assembled default
1.x site is re-verified with the same safety checks after composition.
`tests/default_routes_test.py` checks route promotion, legacy binary
identity, archives, HTML links and redirects; `tests/dual_site_test.py`
checks release identity and actual dual-site composition. The public Pages workflow runs these tests before composing the archive. Its
version selector refuses legacy-only releases: a manually retriggered 0.x
workflow must not silently replace the current 1.x default site.

**No public deployment occurs merely because this source exists in
`caravel-dev`.** Publication requires reviewed synchronization to the
public distribution and explicit owner sign-off for the ensuing Pages deploy.

Before that deployment, use a release-generated full 1.x site plus the
hash-pinned frozen 0.13.0 archive and run the complete Pages build and
browser checks: default/root and `/v1/` emulator startup, tutorial deep
links and hash/query navigation, each installer/editors' actual generation
handshake, a 0.13 editor backup and recovery with the correct archive,
explicit warnings for FORMAT 2 upgrades, and all download routes.
Confirm no unpublished Academy lessons were exported. The public Pages build also applies its approved v1.1.0 W2 recovery-installer overlay to `/v1/`; afterward it copies the same verified installer to the default `/webapp/installer/` path and re-verifies both. The private development workflow lacks this public W2 step, so only the separately reviewed public workflow must be used.

GitHub Pages does not give this static generator a server-side redirect
table. The compatibility behavior is implemented as copied site assets and
small HTML redirects for old-only pages. URLs that previously resolved to
different **same-name** resources in Classic and 1.x cannot keep both meanings
at the same root path. The explicit archives are authoritative for those cases.

Rollback: redeploy the last approved Pages artifact from the last
known-good release; do not change firmware or force-update either repository
to undo a site-only URL mapping.
