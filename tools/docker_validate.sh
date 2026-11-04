#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
#
# The CI's test jobs in a local linux/amd64 container: no GitHub Actions, nothing installed on the host but Docker. The steps come from the workflow
# files themselves (tools/docker_validate/run_steps.py runs their `run:` steps in order), so the local check cannot drift from the CI.
#
#   tools/docker_validate.sh action [SRC_DIR]          job `wasm` of action-banks-android.yml (the ACTION / ENSEMBLE / JOURNEY / STASIS suites, the C/WASM emulator and its Node tests)
#   tools/docker_validate.sh host SHARD [SRC_DIR]      job `prepare` of validate-060.yml (the target build and the host fixtures), then one shard of job `host`
#                                                      (storage_usb synthesis_midi ui_storage effects cpu_dual engines_samples sanitizers web_target)
#   tools/docker_validate.sh host SHARD,SHARD,... [SRC_DIR]   several shards after one preparation (comma separated)
#   tools/docker_validate.sh host all [SRC_DIR]        the preparation once, then every shard one after the other (stops at the first failing shard)
#   tools/docker_validate.sh image                     only build the image
#
# SRC_DIR is a checkout (or git worktree) of the commit to check (default: the checkout this script is in; submodules checked out). It is mounted
# read-only and copied into the container without build/; the log of a run is SRC_DIR/build/docker-validate/<command>.log. The image
# (tools/docker_validate/Dockerfile) holds Python, clang, Node 22, Emscripten 3.1.64, the JieLi toolchain, the pinned SDK files and DaisySP; it is
# built on first use and again when its Dockerfile changes. The Docker VM wants about 8 GB of RAM. Not done here: the release workflows' gates
# (they read GitHub runs of the exact commit), publishing and Pages.
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
cmd=${1:-}; [ -n "$cmd" ] || { awk 'NR >= 5 && /^#/ { print; next } NR >= 5 { exit }' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
shard=""
case "$cmd" in
    action|image) shift;;
    host) shard=${2:-}; [ -n "$shard" ] || { echo "host needs a shard" >&2; exit 2; }; shift 2;;
    *) echo "unknown command: $cmd" >&2; exit 2;;
esac
src=$(cd "${1:-$here}" && pwd)
docker=$(command -v docker || true)
[ -n "$docker" ] || docker=/Applications/Docker.app/Contents/Resources/bin/docker
[ -x "$docker" ] || { echo "docker not found: install Docker Desktop" >&2; exit 3; }
if ! "$docker" info >/dev/null 2>&1; then
    case "$(uname -s)" in Darwin) open -a Docker 2>/dev/null || true;; esac
    i=0; while ! "$docker" info >/dev/null 2>&1; do i=$((i + 1)); [ $i -le 36 ] || { echo "the Docker daemon does not answer" >&2; exit 3; }; sleep 5; done
fi
tag="caravel-validate:$(cat "$here/tools/docker_validate/Dockerfile" | shasum | cut -c1-12)"
if ! "$docker" image inspect "$tag" >/dev/null 2>&1; then
    echo "== building the image $tag (a few minutes, once)"
    "$docker" build --platform linux/amd64 -t "$tag" "$here/tools/docker_validate"
fi
[ "$cmd" != image ] || exit 0
[ -f "$src/tools/build.py" ] || { echo "$src is not a Caravel checkout" >&2; exit 2; }
[ -n "$(ls -A "$src/firmware/engines" 2>/dev/null)" ] || { echo "firmware/engines is empty: git submodule update --init" >&2; exit 2; }
out="$src/build/docker-validate"; mkdir -p "$out"
if [ "$cmd" = action ]; then
    steps='python3 tools/docker_validate/run_steps.py .github/workflows/action-banks-android.yml wasm'
    log="$out/action.log"
else
    rs="python3 tools/docker_validate/run_steps.py .github/workflows/validate-060.yml"
    shards=$(echo "$shard" | tr ',' ' ')
    [ "$shard" != all ] || shards="storage_usb synthesis_midi ui_storage effects cpu_dual engines_samples sanitizers web_target"
    steps="$rs prepare"
    for sh in $shards; do steps="$steps && $rs host --matrix shard=$sh"; done
    log="$out/host-$shard.log"
fi
# ThreadSanitizer cannot map its shadow memory under the x86-64 emulation of Apple silicon ("unexpected memory mapping"): those three tests (shard
# cpu_dual) stay a CI job there (the step's own TSAN: '1' is dropped via DROP_ENV). FORCE_TSAN=1 tries anyway.
nt=""
if [ "$(uname -m)" = arm64 ] && [ -z "${FORCE_TSAN:-}" ]; then nt="-e NO_TSAN=1 -e DROP_ENV=TSAN"; echo "(ThreadSanitizer tests skipped: not possible under the emulation on Apple silicon)"; fi
echo "== $cmd $shard of $(git -C "$src" log -1 --format='%h %s' 2>/dev/null | cut -c1-80)  (log: $log)"
"$docker" run --rm --platform linux/amd64 $nt -v "$src":/src:ro -v "$out":/out "$tag" bash -c "
    set -o pipefail; rsync -a --exclude build /src/ /work/; cd /work
    export SANITIZE=1 ASAN_OPTIONS=detect_leaks=0
    [ -n \"\${NO_TSAN:-}\" ] || export TSAN=1
    ( $steps ) 2>&1 | tee /out/run.log; rc=\${PIPESTATUS[0]}
    mkdir -p /out/artifacts && cp -R build/validation /out/artifacts/ 2>/dev/null || true
    exit \$rc" 2>&1 | tee "$log"
