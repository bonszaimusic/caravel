<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Release validation map

Use this map to find the **smallest relevant source or test area**. It is a navigation guide, not a substitute for acceptance tests, release evidence or human approval.

## Start with the change

1. Record the **full source commit** and the precise changed paths. Separate firmware, build/package, web runtime, documentation and test-only changes.
2. Inspect only the affected files and tests first. Preserve complete test code, raw logs and source history; **summarize, do not discard, evidence**.
3. Compare tested firmware/build-input blobs with the candidate. A documentation-only change does not by itself require another whole firmware regression run. Repeat affected tests, and repeat the full suite if inputs changed, proof is missing or findings require it.
4. A package built from another commit is **not** a build of the new release candidate. Verify source record, firmware SHA-256, installer/site contents and reproducible build inputs separately.

## Test and code entry points

| Task | Read first | Targeted validation |
| --- | --- | --- |
| Firmware/voices, MIDI, CAPTURE, FORMAT 2 | `firmware/src/`, `firmware/engines/` | `tests/`, `tools/validate_all.py` |
| All host shards | `tests/run_tests.sh` | `bash tests/run_tests.sh` (when full rerun is warranted) |
| Tutorial text, images and routing | `docs/tutorials/topics.json`, `web/tutorial_pages.py` | `python3 web/tutorial_pages.py --check-source --check-readme`; `python3 -m unittest discover -s tests -p tutorial_pages_test.py -v` |
| Pinned 1.x documentation assembly | `web/v1_docs.py` | `python3 -m unittest discover -s tests -p v1_docs_test.py -v` |
| Frozen Classic 0.13.0 | `web/frozen_classic.py`, `web/classic_manifest.json` | `python3 -m unittest discover -s tests -p frozen_classic_test.py -v` |
| Dual-generation website | `web/dual_site.py` | `python3 -m unittest discover -s tests -p dual_site_test.py -v` |
| Release-site runtime generation guard | `web/check_generation_site.mjs` | `node web/check_generation_site.mjs <verified-site-dir>` |
| Firmware packaging and reproducibility | [BUILDING.md](../BUILDING.md) | Use the exact-source build, verify SDK/submodule pins and output hashes |

## Evidence and release gates

Keep one short current-state record with: **source SHA, test type, outcome, measured limitations/skips, artifact SHA-256, remaining gates and links to raw evidence**. Do not paste complete logs into every handoff. Host/emulator success is not FM-1 hardware, audio, USB, clock or backup/restore acceptance.

The complete history, tests and detailed product notes remain in [1.0 release notes](release-100.md), [build guide](../BUILDING.md) and the relevant source files. Classic and 1.x assets must remain version-isolated. No deployment, public tag or release is authorized by this document.
