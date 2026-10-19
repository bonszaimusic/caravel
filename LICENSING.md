# Caravel licensing

Caravel is free software, licensed under the GNU General Public License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). That covers the code and its own assets. A few
bundled or ported parts keep their own licences; they are listed below. MIT, Apache-2.0
and SIL OFL texts are in `LICENSES/`; CC0 sample provenance and references are in
`assets/samples-cc0/ATTRIBUTION.txt`.

Caravel is a modified version of **Felucca 1.6.1** (Leo Kuroshita, Hügelton Instruments,
<https://github.com/hugelton/Felucca>, tag v1.6.1, commit `a53ecf4`). The origin, the changes and the non-affiliation are
stated in `NOTICE`.

Copyright (C) 2026 bonszaimusic (Caravel and its changes)<br>
Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (Felucca 1.6.1, the base)

The source is at https://github.com/bonszaimusic/caravel.

Files that Caravel changed keep their original copyright and `SPDX-License-Identifier` headers; the changes are recorded in
this repository's git history. Copyright notices cover the contributions they identify, not
sole authorship of every added file.

## What is GPL-3.0-only

Every file in this tree that carries an `SPDX-License-Identifier: GPL-3.0-only` header:

- the firmware: `firmware/` (app, HAL, update loader; not `firmware/engines/`, MIT: below)
- the build script and tools: `build.sh`, `tools/`
- the web pages (installer, editor, the browser emulator `web/emu/`) and their tests: `web/` (not the Fukiai and DotGothic16 fonts, below)
- the host tests: `tests/`

Three source files are ports and keep the licence of their originals:
`firmware/src/phys_dsp.c` (DaisySP, MIT), `firmware/src/phys_symp.c` (Rings, MIT) and
`firmware/src/fm6_core.c` (msfa, Apache-2.0). The firmware built with them is GPL-3.0-only as
a whole.

The unchanged Felucca-Engines submodule (`firmware/engines/`,
<https://github.com/hugelton/Felucca-Engines>) is Hügelton Instruments' MIT-licensed work:
its ANALOG, LOFI, TRIO, WHEEL and NOISE variants, engine interface (`felucca_engine.h`),
shared DSP helpers (`dsp.c`) and table generator (`firmware/engines/LICENSE`, also
`LICENSES/MIT-Felucca-Engines.txt`). Caravel uses its TRIO, WHEEL, NOISE and shared code.
The active ANALOG and LOFI variants are instead local `firmware/src/eng_analog.c` and
`firmware/src/eng_lofi.c`: they carry GPL-3.0-only and JIANT adaptation notices, not the
submodule's MIT licence. The firmware as a whole is GPL-3.0-only.
Caravel's complete source includes that submodule: clone with `--recurse-submodules`, and
give it along with the rest when you share the source. Each release also offers
`caravel-X.Y.Z-source.tar.gz`, the whole source with the submodule's files in it (GitHub's
own source archives leave the submodule empty).

You may use, study, change and share Caravel under the GPL. If you distribute Caravel, or
firmware derived from it, you must also give your recipients its complete corresponding
source under the same licence. That includes devices that ship with modified Caravel inside.

## Hügelton Instruments' own work

All by Hügelton Instruments (Leo Kuroshita), in this tree:

| What | Licence | Where |
| --- | --- | --- |
| Felucca 1.6.1, the base of Caravel: firmware, tools, web pages, tests | GPL-3.0-only | `LICENSE` |
| The PHASE engine's waveforms: a C port of the oscillator of CrispyZebra (<https://github.com/hugelton/CrispyZebra>) | GPL-3.0 | `firmware/src/eng_phase.c` |
| The DRUM voices and kit | GPL-3.0-only | `firmware/src/drum_voice.c`, `firmware/src/eng_drum.c` |
| The Hügelton Sample Pack: Felucca's drum sounds, made by `tools/gen_waves.py`, from which the SLICE engine's BREAK is built (not CC0) | GPL-3.0-only | `tools/gen_waves.py` |
| The controls picture | GPL-3.0-only | `docs/controls.jpg` |
| The Fukiai icon font (<https://github.com/hugelton/Fukiai>): the firmware's icons (rasterised at build time by `tools/gen_aa_icons.py`) and the web editor's | MIT | `web/fukiai.ttf`, `LICENSES/MIT-Fukiai.txt`, `web/FUKIAI-LICENSE.txt` |
| The unchanged Felucca-Engines variants (including the original ANALOG and LOFI), engine interface and shared DSP helpers; not Caravel's local GPL/JIANT-derived ANALOG and LOFI | MIT | `firmware/engines/`, `LICENSES/MIT-Felucca-Engines.txt` |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Inter Tight font by The Inter Project Authors: the UI text, rasterised into the firmware at build time (`tools/gen_aa_font.py`; the generated tables are not offered as a font, and the font declares no Reserved Font Name) | SIL OFL 1.1 | `assets/fonts/InterTight[wght].ttf`, `LICENSES/OFL-InterTight.txt` (also `assets/fonts/OFL.txt`) |
| Instrument samples (Versilian Studios VSCO-2 Community Edition, VCSL): the SAMPLE sets, also the SLICE engine's PIANO (the PIANO set's middle C) | CC0 1.0 | `assets/samples-cc0/`, provenance in `ATTRIBUTION.txt` there |
| DaisySP by Electrosmith, Corp and Emilie Gillet (<https://github.com/electro-smith/DaisySP>): the PHYS engine's modal and string models and the resonator, ported to fixed point | MIT | `firmware/src/phys_dsp.c`, `LICENSES/MIT-DaisySP.txt` |
| Rings by Emilie Gillet (<https://github.com/pichenettes/eurorack>): the PHYS engine's sympathetic strings, ported to fixed point | MIT | `firmware/src/phys_symp.c`, `LICENSES/MIT-Rings.txt` |
| msfa by Google Inc. and Pascal Gauthier, from Dexed (<https://github.com/asb2m10/dexed>): the FM6 engine's synthesis, ported to integer C (Dexed itself is GPL-3.0; only msfa is used; the FM6 factory patches are Felucca's own) | Apache-2.0 | `firmware/src/fm6_core.c`, `LICENSES/Apache-2.0-msfa.txt` |
| klattsch by Tony Gies (<https://github.com/tgies/klattsch>): design reference for the VOICE engine; no code copied. Formant data from Klatt (1980) / Hillenbrand et al. (1995) | MIT (klattsch) | credit only |
| X0X by charlesvestal (<https://github.com/charlesvestal/fm1-x0x>), a Felucca fork: the design of the browser emulator (the worklet, the device clock driven by its audio, the exports); its files credit it in their headers | GPL-3.0 | `web/emu/` |
| The second core: its start, mailbox and guards from X0X by charlesvestal (<https://github.com/charlesvestal/fm1-x0x>, with the dual-core findings of Melodee by keremimo, <https://github.com/keremimo/melodee>); its audio split, measurements and hardening after Jangada by zednaked (<https://github.com/zednaked/jangada>); each credited in its files | GPL-3.0 | `firmware/hal/fm1_cpu1.{h,S}`, `firmware/src/fx.c`, `tests/c1_host.h`, `tests/dualcore_test.c` |
| DotGothic16 font by The DotGothic16 Project Authors (<https://github.com/fontworks-fonts/DotGothic16>): the browser emulator's knob labels, served with its page (cut to printable ASCII; the font declares no Reserved Font Name) | SIL OFL 1.1 | `web/emu/fonts/DotGothic16-subset.woff`, `LICENSES/OFL-DotGothic16.txt` (also `web/emu/fonts/OFL.txt`) |
| JieLi AC79 SDK by JieLi Technology: three of its files go into every `.fwsc` package (below); none are in this tree | Apache-2.0 (repository-default coverage; below) | `LICENSES/Apache-2.0.txt` |

On the device, HOME held > ABOUT opens the information screen; turning PRESETS scrolls on into
CREDITS, a short list of these authors, licences and source URLs.

## JieLi SDK files in the packages

A `.fwsc` package made by `tools/build.py` (with `tools/fm1pkg_make.py`; this is the package the
web installer installs) holds three unmodified files from the JieLi AC79 SDK
(<https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK>, `cpu/wl82/tools/`). They are read from your SDK
checkout at build time (see BUILDING.md); no SDK files are in this tree.

| File in the package | What it is |
| --- | --- |
| `uboot.boot` | the first-stage boot loader (SPL) |
| `cfg_tool.bin` | the chip configuration block |
| `eq_cfg_hw.bin` | the default hardware EQ table |

Verified on 2026-10-19 against official tag `AC79NN_SDK_V1.2.1_2023-12-13`, commit
`d179b4484759423312073f5fbb232501aa491047`: all three official Git blob IDs and SHA-256
hashes match the expected build inputs. The commit's [root LICENSE](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/blob/d179b4484759423312073f5fbb232501aa491047/LICENSE)
is Apache-2.0 and expressly permits redistribution in Source or Object form, subject to
its conditions. No scoped exclusion was found for these files. This supports repository-default
Apache-2.0 coverage; no binary redistribution prohibition was established.

No root or `cpu/` NOTICE was found at this commit. The SDK does contain two nested
uC-HTTPc NOTICE files from Silicon Labs; their applicability to these three boot/config
blobs has not been established. Repository-default coverage is not a guarantee of all
embedded third-party rights, nor does it relicense every component of the SDK.

Before package distribution, the maintainer should review this coverage basis. Redistribution
must retain the Apache-2.0 text (`LICENSES/Apache-2.0.txt`) and any applicable copyright,
attribution, NOTICE and modification notices. If the owner needs stronger assurance, seek
vendor confirmation for these exact blobs and any embedded third-party obligations.
Caravel's own code stays GPL-3.0-only.

## No vendor material

No M-VAVE material is part of Caravel. The firmware links no vendor code and contains no vendor
data, and the packages hold only Caravel and the three SDK files above. The installer's
"Return to official V15" holds only the official file's size and SHA-256: you select the
official firmware file you downloaded yourself, and it is checked and installed in your browser,
never uploaded or redistributed.

## Contributions

Contributions to Caravel, including the local GPL/JIANT-derived ANALOG and LOFI variants
in `firmware/src/`, are welcome under GPL-3.0-only. Changes to the MIT components in
`firmware/engines/` go to the Felucca-Engines repository under the MIT licence; this includes
its original ANALOG and LOFI variants, not Caravel's local versions. Keep existing credits
and licence notices.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. Caravel is a fork under its own name and uses
them only to state where the code comes from.

"Caravel" is the name of this project; no trademark is claimed or registered for it, and nobody should use it to suggest
affiliation with Hügelton Instruments or M-VAVE.

"M-VAVE" and "FM-1" are trademarks of their respective owners. Caravel is independent
firmware that runs on FM-1 hardware. It is not affiliated with, endorsed by or supported
by those owners.

## Radio

Caravel never enables the Bluetooth / Wi-Fi radio of the hardware.
