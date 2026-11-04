#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
#
# Sizes of the three firmware layouts (one core, no RAM DSP, default) built locally in a linux/amd64 container, with the same
# commands as .github/workflows/layouts-check.yml: no GitHub Actions, nothing installed on the host but Docker.
#
#   tools/docker_layouts.sh [SRC_DIR [OUT_DIR]]
#
#   SRC_DIR   a checkout (or git worktree) of the commit to measure; mounted read-only, copied into the container without build/
#             (default: the checkout this script is in; submodules must be checked out, as BUILDING.md says)
#   OUT_DIR   where the summary and the three full build logs go (default: SRC_DIR/build/docker-layouts)
#   EXTRA_ENV extra build switches for every layout, e.g. EXTRA_ENV="ENS_STEREO=1 CARAVEL_FOO=1" tools/docker_layouts.sh
#
# The container installs Python, Pillow/fontTools, clang and the libraries Pillow needs, downloads JieLi's Linux toolchain from its package
# server and the three pinned AC79 SDK files from Gitee (SHA-256 checked, as in the workflow), so it needs the network and a few minutes
# per layout (x86-64 under emulation on Apple silicon). Output per layout: `image N B`, RAM .data+.bss, the pool and the target costs.
# The measured numbers are the flash/RAM of the layout; they are not CPU measurements. BUILDING.md describes the other way to build on a Mac
# (toolchain and SDK installed on the host, each tool run in a container by tools/build.py).
set -eu
here=$(cd "$(dirname "$0")/.." && pwd)
src=$(cd "${1:-$here}" && pwd)
out="${2:-$src/build/docker-layouts}"
mkdir -p "$out"; out=$(cd "$out" && pwd)
docker=$(command -v docker || true)
[ -n "$docker" ] || docker=/Applications/Docker.app/Contents/Resources/bin/docker
[ -x "$docker" ] || { echo "docker not found: install Docker Desktop" >&2; exit 3; }
if ! "$docker" info >/dev/null 2>&1; then
    case "$(uname -s)" in Darwin) open -a Docker 2>/dev/null || true;; esac
    i=0; while ! "$docker" info >/dev/null 2>&1; do i=$((i + 1)); [ $i -le 36 ] || { echo "the Docker daemon does not answer" >&2; exit 3; }; sleep 5; done
fi
[ -f "$src/tools/build.py" ] || { echo "$src is not a Caravel checkout" >&2; exit 2; }
[ -n "$(ls -A "$src/firmware/engines" 2>/dev/null)" ] || { echo "firmware/engines is empty: git submodule update --init" >&2; exit 2; }

"$docker" run --rm -i --platform linux/amd64 -e EXTRA_ENV="${EXTRA_ENV:-}" -v "$src":/src:ro -v "$out":/out ubuntu:22.04 bash -s <<'INNER' 2>&1 | tee "$out/summary.log"
set -u
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null 2>&1 && apt-get install -y -qq python3 python3-pip curl xz-utils git clang build-essential ca-certificates rsync libraqm0 libfribidi0 libharfbuzz0b >/dev/null 2>&1 || echo "APT FAILED"
pip3 install -q Pillow fonttools >/dev/null 2>&1 || echo "PIP FAILED"
mkdir -p /work && rsync -a --exclude build /src/ /work/ && cd /work || exit 1
sh tools/get_toolchain.sh /opt/jieli >/dev/null 2>&1 || { echo "TOOLCHAIN FAILED"; exit 2; }
echo "toolchain: $(ls /opt/jieli | grep jieli-linux | head -1)"
root=/opt/ac79-sdk/cpu/wl82/tools; mkdir -p $root/cfg
base="https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/raw/AC79NN_SDK_V1.2.1_2023-12-13/cpu/wl82/tools"
for f in uboot.boot cfg_tool.bin cfg/eq_cfg_hw.bin; do curl -fLsS --connect-timeout 8 --max-time 60 --retry 2 -o $root/$f $base/$f || echo "SDK FETCH FAILED $f"; done
python3 - <<'PY' || exit 2
import hashlib, pathlib
r = pathlib.Path("/opt/ac79-sdk/cpu/wl82/tools")
exp = {"uboot.boot": "4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4",
       "cfg_tool.bin": "276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b",
       "cfg/eq_cfg_hw.bin": "41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d"}
ok = all((r / n).exists() and hashlib.sha256((r / n).read_bytes()).hexdigest() == s for n, s in exp.items())
print("SDK files match the pinned SHA-256" if ok else "SDK FILES DO NOT MATCH THE PINNED SHA-256")
raise SystemExit(0 if ok else 1)
PY
export JIELI_TOOLCHAIN=/opt/jieli/toolchain AC79_SDK=/opt/ac79-sdk CC="cc -include stdint.h" FELUCCA_SLICE=0 CARAVEL_REVERB_MINIMAL=1 BUDGET_UPDATE=1 AUDIO_RAM_REQUIRE_RUNTIME=1
VER=$(sed -n 's/^  VER: //p' .github/workflows/layouts-check.yml | head -1)
echo "commit: $(git -C /work log -1 --format=%h 2>/dev/null || echo unknown)  version text: $VER  extra: ${EXTRA_ENV:-none}"
for layout in "one core:FELUCCA_CPU2=0" "no RAM DSP:CARAVEL_AUDIO_RAM=0" "default layout:"; do
  name=${layout%%:*}
  echo "== $name"
  env ${layout#*:} ${EXTRA_ENV:-} python3 tools/build.py --binary-version "$VER" > /tmp/build.log 2>&1 || echo "BUILD FAILED"
  grep -E "image [0-9]+ B|ram_text|c1_text|audio_text|region|overflow|error|Error|target: (ens_|sem_render|hades_render|drone_cell_render|mix_part|track_render)" /tmp/build.log | head -20
  cp /tmp/build.log "/out/build-$(echo $name | tr ' ' '_').log"
done
echo DONE
INNER
