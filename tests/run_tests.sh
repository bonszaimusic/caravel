#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# FORMAT 2: same registered commands as tools/validate_all.py / CI.
set -eu
cd "$(dirname "$0")/.."
for shard in storage_usb synthesis_midi ui_storage effects cpu_dual engines_samples sanitizers web_target; do
    python3 tools/validate_all.py "$shard"
done
python3 tools/validate_all.py report
