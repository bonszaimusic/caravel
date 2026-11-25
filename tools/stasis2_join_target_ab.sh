#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# PRIVATE reproducible source-join ROM/RAM/Pool build comparison.
# This builds 3 layouts x 2 scope flags x 3 join variants = 18 target binaries.
# Requires the validated local JieLi toolchain and its pinned Docker runtime.
# Never publishes, tags, flashes, merges or edits a release branch.
set -eu
cd "$(dirname "$0")/.."
test -f build/gen/felucca_tables.h || {
  echo 'Generate build/gen on an authenticated local checkout first' >&2
  exit 2
}
test -n "${JIELI_TOOLCHAIN:-}" || {
  echo 'JIELI_TOOLCHAIN must point to the validated local SDK' >&2
  exit 2
}
out='build/stasis-v5/join-target'
mkdir -p "$out"
printf 'source HEAD: %s\n' "$(git rev-parse HEAD)" > "$out/source.txt"
printf 'version: 1.1.1 (private binary build, not release)\n' >> "$out/source.txt"
printf '%s\n' 'profile,scope,join,lut,bin_bytes,sha256' > "$out/summary.csv"
for config in 'default 1 1' 'one-core 0 1' 'no-ram-dsp 1 0'; do
  set -- $config
  profile=$1; cpu2=$2; ram=$3
  for scope in 0 1; do
    for mode in '0 0' '1 0' '1 1'; do
      set -- $mode
      join=$1; lut=$2
      tag="${profile}-scope${scope}-join${join}-lut${lut}"
      echo "Building $tag (no firmware publish)"
      # Use numeric 0/1 to guarantee that a disabled experiment stays
      # disabled; the fx.c guards require BOTH defined and value==1.
      # The optional LUT candidate requires the JOIN=1 experiment.
      CARAVEL_STASIS2=1 CARAVEL_STASIS_EXPAND=1 \
      CARAVEL_STASIS_SCOPE_V6="$scope" \
      CARAVEL_STASIS_SOURCE_JOIN_EXPERIMENT="$join" \
      CARAVEL_STASIS_SOURCE_JOIN_LUT_EXPERIMENT="$lut" \
      FELUCCA_CPU2="$cpu2" CARAVEL_AUDIO_RAM="$ram" \
        python3 tools/build.py --binary-version 1.1.1 > "$out/$tag.log" 2>&1 || {
          echo "Target build FAILED: $tag (see $out/$tag.log)" >&2
          exit 1
        }
      cp build/felucca.bin "$out/$tag.bin"
      python3 - "$out/$tag.bin" "$profile" "$scope" "$join" "$lut" "$out/summary.csv" <<'PY'
import hashlib, pathlib, sys
file=pathlib.Path(sys.argv[1]); data=file.read_bytes()
digest=hashlib.sha256(data).hexdigest()
with open(sys.argv[6], 'a', encoding='utf-8') as f:
    f.write(f"{sys.argv[2]},{sys.argv[3]},{sys.argv[4]},{sys.argv[5]},{len(data)},{digest}\n")
PY
    done
  done
done
echo "Completed all 18 private binary builds. See $out/summary.csv and *.log"
echo 'Static check_perf_audio status and pool/RAM numbers are in each build log.'
echo 'These are build/size gates only: no measured ISR latency, CPU reserve, audio sound or FM-1 signoff.'
