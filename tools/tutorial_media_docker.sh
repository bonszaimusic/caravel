#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
#
# Run one of the tutorial media tools in the linux/amd64 image of tools/docker_validate.sh (Python with Pillow, clang, the generated headers):
#   tools/tutorial_media_docker.sh start_tutorial_frames.py        the START HERE screens and overview cards from the firmware renderer
#   tools/tutorial_media_docker.sh tutorial_device.py              the device graphics and the README banner
# The checkout is mounted read-write: build/ (git-ignored) and docs/tutorials/img/ are written. Nothing is installed on the host.
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
tool=${1:?usage: tutorial_media_docker.sh TOOL.py [args]}; shift
tag="caravel-validate:$(cat "$here/tools/docker_validate/Dockerfile" | shasum | cut -c1-12)"
docker=$(command -v docker || echo /Applications/Docker.app/Contents/Resources/bin/docker)
"$docker" image inspect "$tag" >/dev/null 2>&1 || { echo "build the image first: tools/docker_validate.sh image" >&2; exit 3; }
"$docker" run --rm --platform linux/amd64 -v "$here":/work -w /work "$tag" bash -c '
  [ -f build/gen/ui_fonts.h ] || python3 -c "import sys; sys.path.insert(0, \"tools\"); import build; build.generate()" >/dev/null
  python3 tools/'"$tool"' "$@"' _ "$@"
