#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
# Run the workflow's local checks, without GitHub Actions.
#   tools/docker_validate.sh host fast [SRC_DIR]  all shards, 30 fuzz seeds/profile
#   tools/docker_validate.sh host all [SRC_DIR]   full candidate gate, >=141 seeds
#   tools/docker_validate.sh host SHARD[,SHARD...] [SRC_DIR]
#   tools/docker_validate.sh action [SRC_DIR]     existing ACTION/WASM workflow
#   tools/docker_validate.sh image               build target and native images
# VALIDATE_SHARDS controls isolated concurrent shards (default 1); VALIDATE_JOBS
# controls concurrent compiles (default <=8). CPU/memory limits are shared.
# Preparation uses amd64; host checks use the Docker server's native architecture.
# Inputs are read-only and artifacts/timings live under build/docker-validate/.
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
exec "${PYTHON:-python3}" "$here/tools/docker_validate/local_host.py" "$@"
