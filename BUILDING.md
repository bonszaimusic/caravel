# Building Caravel

Caravel is a fork of Felucca 1.6.1 (see [NOTICE](NOTICE)). Internal file and macro names keep their Felucca spelling
(`felucca.c`, `FELUCCA_*`, `build/felucca.bin`); the package and release files are named `caravel*`.

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `felucca.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `caravel.fwsc` | the installable package (app + loader) |

## Get the source

The MIT Felucca-Engines submodule (`firmware/engines/`, [upstream](https://github.com/hugelton/Felucca-Engines))
provides shared DSP/interface code and the active TRIO, WHEEL and NOISE engines. Its original ANALOG/LOFI variants
are also included; Caravel instead uses local GPL/JIANT-derived ANALOG/LOFI variants (see LICENSING.md).
Clone with `--recurse-submodules`:

```
git clone --recurse-submodules https://github.com/bonszaimusic/caravel.git
```

In a clone made without it, run `git submodule update --init` once. GitHub's "Download ZIP" and the source archives
it adds to a release leave `firmware/engines/` empty; each release also has `caravel-X.Y.Z-source.tar.gz`, the whole
source with the submodule's files in it. The build stops with "firmware/engines is empty" when they are missing.

## Prerequisites (macOS)

- Python 3 with Pillow and fontTools: `pip3 install Pillow fonttools` (the UI font and icons are
  rasterised at build time)
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- The pinned JieLi AC79 SDK (repository-default Apache-2.0; see LICENSING.md for scope/notices). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  git -C ~/fw-AC79_AIoT_SDK rev-parse HEAD
  # Must be d179b4484759423312073f5fbb232501aa491047.
  ```

  Release/binary-version builds reject unexpected boot/configuration blob hashes. A hash match identifies
  the reference bytes; it is not a blanket licence/third-party-rights attestation.

- Node.js (optional, for the web tests).
- Emscripten (optional, `emcc`: the browser emulator, `web/emu/`).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

## Verified local release artifacts

`./build.sh --release 0.6.0` builds identity `FM-1_999` and version `v0.6.0`, then
assembles and checks `build/release-0.6.0/`. This production mode requires clean,
committed source and initialized submodules at their recorded pins. It rejects
unexpected SDK bytes and unsafe build-output links before writing.

The folder contains the installable package, app and wrapped loader; complete
`caravel-0.1.0-source.tar.gz` including all engine files; `LICENSE`, `LICENSING.md`,
`NOTICE`, `CREDITS.md`, sample `ATTRIBUTION.txt`, and individual licence text files.
`release-build.json` records the exact source identity, explicit version override,
SDK reference and artifact hashes. `SHA256SUMS` covers every shipped file except
itself. Artifacts use a flat layout so GitHub release downloads can be checked
without rebuilding a directory hierarchy:

```sh
python3 tools/release_artifacts.py 0.1.0 --verify-only
# In a downloaded/extracted release directory:
sha256sum -c SHA256SUMS
```

Do not publish an installable package without its matching source and required
licences/notices. Producing these artifacts does not allocate USB IDs, authorize a
release, establish hardware acceptance or provide blanket legal clearance.

### Rebuild from the complete source archive

A source archive contains the engine files but deliberately no Git metadata.
With the prerequisites above, `./build.sh --binary-version 0.1.0` rebuilds the same
version/product app, loader and package without requiring Git history. It performs
the same target/SDK checks, but does **not** assemble a verified release folder or
authorize redistribution. `--binary-version` and `--release` are mutually exclusive.

### Build a release-bound site locally

After a verified release build, optionally build the emulator from that same
checkout with Emscripten available, then generate a new/empty output directory:

```sh
EMU_PREVIEW=0 EMU_VERSION=v0.1.0 sh web/emu/build.sh
python3 web/make_site.py build/release-0.1.0/caravel-0.1.0.fwsc 0.1.0 build/site \
  --release-dir build/release-0.1.0 --emulator build/emu
```

Omit `--emulator` if no emulator build is available. The generator checks the full
flat manifest, source archive, source identity, notices and release-build record;
it copies the exact matching source archive beside the package and links it from
the installer/emulator. `--check-only` validates inputs without creating output.
For local development only, `--preview` accepts a package without release evidence
and labels the site unverified; do not deploy that output as a verified release.

The Pages workflow checks out the selected release tag, verifies the complete
release download against it, and compiles that source's emulator with the recorded
version override. It does not rebuild a different target binary from main. The job
refuses private-repository deployment. Changing repository visibility, enabling
Actions/Pages or publishing a release remains a separate operator decision.

Build options (environment, `0` or `1`; defaults in `firmware/src/felucca.c`, `core.h` and `icons.c`):

| Flag | Default | |
| --- | --- | --- |
| `FELUCCA_FLASH` | 1 | settings, presets and projects in flash |
| `FELUCCA_OTA` | 1 | update entry (needs `FELUCCA_FLASH`) |
| `FELUCCA_CDC` | 1 | USB serial console |
| `FELUCCA_CDC_DEFAULT` | 1 | `0`: the console is built in but left out of USB from boot (as MENU > USB SERIAL OFF) |
| `FELUCCA_UAC` | 1 | USB audio input (the master output, stereo, 44.1 or 48 kHz) |
| `FELUCCA_UAC_48K` | 1 | `0`: the USB audio input at 44.1 kHz only, as up to 1.0.5.2 (`firmware/src/usb.c`) |
| `FELUCCA_UAC_AS_FIRST` | 1 | the audio control header lists the audio streaming interface before the MIDI one, so the USB audio driver of older macOS (up to 15) finds it; `0`: 1.0 to 1.5's order, byte for byte (`firmware/src/usb.c`) |
| `FELUCCA_CPU2` | 1 | the second core is built in (MENU > SYSTEM > 2ND CORE, OFF by default); `0`: none of it, 1.6's one-core code and RAM (`firmware/hal/fm1_cpu1.h`, `firmware/src/fx.c`) |
| `FELUCCA_UART` | 1 | TRS MIDI IN |
| `FELUCCA_SLICE` | 1 | the SLICE engine |
| `FELUCCA_ICONS` | 1 | parameter icons on the knob cards |
| `FELUCCA_FM4` | 0 | the retired DIGITAL engine (4-operator FM) instead of its FM6 conversion |

`FELUCCA_USB_LAYOUT` (`0` to `3`, default `0`) picks other USB descriptor layouts for testing; see
`firmware/src/usb.c`.


## Build profiles and machine-readable metadata

The 0.6.0 release profile is tracked in [metadata/build-profiles.json](metadata/build-profiles.json).
Target builds default to `CARAVEL_REVERB_MINIMAL=1` and `FELUCCA_SLICE=0`: HALL/STASIS
with HOME, Pair, macros and performance storage. The public emulator explicitly
uses the same effect/engine profile. Native legacy goldens retain their separate
full ROOM/SPRING/SLICE reference. STASIS shares the ordinary delay allocation;
tempo DELAY cannot run with it. See [the STASIS guide](docs/tutorials/README.md)
for controls, safe project substitutions and the remaining hardware checks.

## Prerelease DSP in executable RAM

The 0.7.0-rc6 target defaults to `CARAVEL_AUDIO_RAM=1`. It moves selected
DRONE, SEM, HADES, STASIS and HALL functions into `.audio_text` in the existing
24 KiB RAMTEXT window. The flash-off driver remains in `.ram_text` with its
strict no-call check. Worker stacks follow the code and retain their full size;
the separate 8 KiB debug window, data/bss and pool budgets remain unchanged.

LLVM optimization runs before DSP placement so the old compiler still inlines
small XIP helpers. `tools/audio_ram_ir.py` then places seven retained roots and
uses volatile local function pointers for calls across the pi32v2 branch range.
The stack slot is allocated once per caller, including calls in loops.
`tools/check_audio_ram.py` checks linked addresses, exact packaged code bytes,
the target startup-copy loop/fence, executable guard window and worker stacks.

`CARAVEL_AUDIO_RAM=0 sh build.sh --binary-version 0.7.0-rc6` retains XIP DSP
placement for development comparisons. The distributed prerelease uses the
default RAM variant. This switch is a build option, not a device setting or a
claim of measured hardware improvement. Host/WASM retain their normal placement.

## Samples

The CC0 instrument samples that the SAMPLE engine uses are in `assets/samples-cc0/`
(Versilian Studios, see `ATTRIBUTION.txt` there). `tools/fetch_cc0.py` downloads them
again from the source repositories. Without that folder the build still works, with no
instrument sets in the SAMPLE engine and no PIANO in SLICE (the PIANO set's middle C): they play a plain sine
at the note's pitch instead, and the screen says NO SAMPLE.

## Browser emulator

```
./build.sh
web/emu/build.sh
python3 -m http.server -d build/emu 8790     # open http://localhost:8790/
```

`web/emu/felucca_web.c` builds the firmware's sources as the host tests do, against a simulated FM-1, and
Emscripten compiles it to WebAssembly (`build/emu/felucca.wasm`, with `index.html`, `worklet.js` and the font in `fonts/`). The page
keeps the flash's storage sectors (projects, user presets, settings) in the browser. Not emulated: USB (audio,
MIDI out, the editor, updates), TRS MIDI and the user sample slots. Build it as above and serve `build/emu/` (no hosted copy under the Caravel name yet).

Every CI run on `main` and on pull requests also builds the emulator of that commit and keeps it as the artifact
`caravel-emulator-unverified-<commit>` for 14 days (GitHub account needed to download). Unzip it and serve the folder as above
(`python3 -m http.server -d <folder> 8790`). It is an unverified preview of that commit, not a release.

## Tests

```
tests/run_tests.sh
```

Runs the host tests and, with Node.js, the web page tests (and, with Emscripten too, the browser emulator's). Run it after `./build.sh`
(it uses `build/` and needs `AC79_SDK` set as for the build). The suites cover flash storage,
user presets, projects of every format, backup, the keys and knobs, MIDI (USB, TRS, clock,
control), USB audio, the update entry and loader, the command-line installer, the UI (the real
drawing code against stubs: every screen in every palette is rendered and checked for clipped or
overlapping text; PNGs land in `build/ui_new/`), every engine (DRUM, NOISE, PHYS, FM6, SLICE, the
DIGITAL conversion), the chord keys, the modulation matrix, the FX layer, the reverbs, the SLICER
and swing. With `DAISYSP` pointing at a DaisySP checkout, the PHYS models are also compared with
their floating-point originals; without it that test is skipped. Felucca-Engines' own test
(`firmware/engines/tests/run.sh`) runs too, and its standalone host is built once more with Felucca's tables
against the same golden renders.

The regression suite (`tests/regress.c`) renders every engine and preset and compares a
hash of each render with `tests/golden.txt`; it also checks levels, voices and the CPU
cost (`tests/cpu_baseline.txt`, `tests/target_budget.txt`). After an intended change of
the sound, `GOLDEN_UPDATE=1 sh tests/run_tests.sh` rewrites the hashes; `BUDGET_UPDATE=1`
does the same for the cost files. For ordinary validation, unset both variables entirely:
setting either to `0` still enables its truthy update path. The offline release/site
fixture suites run in disposable repositories and never overwrite audio goldens.

## Install

Installing a release, with checks and the way back to the official firmware: [docs/installing.md](docs/installing.md). In short:

Use the web installer in Chrome or Edge, built into a site with `web/make_site.py` (below; no hosted copy under the
Caravel name yet). It installs the released package.

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/caravel.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Or, to install your own build from the web installer, make a local copy of the site and open it from `localhost`
(Web MIDI needs a secure context):

```
python3 web/make_site.py build/caravel.fwsc dev /tmp/felucca-site
cd /tmp/felucca-site && python3 -m http.server 8000
# open http://localhost:8000/webapp/installer/
```

Installing firmware is at your own risk. If an install fails and the FM-1 no longer
starts, recovery needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## 0.6.0 target profile

Ordinary target builds enable HOME, Pair, hardware performance storage and the
validated worker path. The device setting still defaults to second core OFF.
HALL/STASIS and the SLICE-off engine selection are the production defaults.
Native legacy tests choose their compile-time flags separately. The browser
emulator uses the same HOME/Pair/storage paths with CPU2=0; its timing cannot
measure FM-1 CPU load. Run `python3 tools/check_050.py` after the target build,
in addition to the inherited test suite. Existing RAM/POOL reserve limits remain.
