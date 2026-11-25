# STASIS 2.0: technical background of the V5 source prototype (stages 0 to 2)

**1.2.0 note:** this page keeps the technical background of the V5 prototype. For what Caravel 1.2.0 does and how to use it, read [release-120.md](release-120.md). In 1.2.0 the STASIS 2.0 profile (`CARAVEL_STASIS2=1`, `CARAVEL_STASIS_EXPAND=1`, `CARAVEL_REVERB_MINIMAL=1`) is the production profile of `tools/build.py` and the emulator; `CARAVEL_STASIS2=0` builds the 1.1.1 behaviour for comparison. The grain sizes are now 23/46/93/186 ms, and FEED, DRY and HALL exist (see below).

The prototype itself was a private development stage; hardware behaviour of 1.2.0 is described in the release page.

## Audio

- The existing 128 KiB source allocation is one 65536-sample PCM16 ring; the flag-on build excludes the old ADD/BLEND secondary capture writer and merger.
- Two fixed 180°-opposed granular heads with complementary 50% overlap remain; MOTN scatters their start points across the REGION window. No new codec, free density, four-head scheduler or hidden extra RAM bank.
- `stasis_region.h` plans a real source-age interval and writer-safe forward/reverse grain anchors from `POS`, `LENGTH`, valid history, existing quality and grain size. The runtime distinguishes FILL, WAIT, FIT and LIMIT.
- FREEZE OFF fades old grain audio and retains the existing ring's history for subsequent LIVE writes; explicit long C5 NEW SOURCE fades, then discards source history and safely prefills. The HALL tail is not reset. This is not yet measured on FM-1 or guaranteed click-free.

## Persisted native project REGION extension

The opt-in V5 codec in `project_format2.c` writes FORMAT-2 **marker `0xA6`** only when `POS`, `LENGTH`, `GRAIN`, `QUALITY` or `REVERSE` differs from its default. The usual zero-marker projects stay byte-identical on the unchanged codec path; existing `0xA5` AFTER/TONE payloads remain readable and unmodified.

`stasis_region_wire.h` packs 19 bits into *previously unused* high nibbles of four HARMONY root bytes and the top three unused bits of the fourth HARMONY duration byte. Existing root low nibbles, all four duration low two-bit fields, the 21-bit AFTER/TONE payload, the old `G_SYNC/G_INFO/G_ENGGO` globals, the native project length, hash and object addresses do not change. A6 is rejected by old/flag-off firmware (deliberate version guard; export/use older formats only through a verified migration). `project_capture`, `project_restore_runtime` and `project_init_runtime` save, restore and reset all five source controls.

The fixed-size native format has a compile-time `PROJ_STORE_SIZE <= 3328` guard for the opt-in profile; real packed HPM2 backup/compression and older-project/editor/import roundtrips **remain to be run**. No claims are made about automatic downgrade/export compatibility.

## PLAY UI and BUILD UI

PLAY FX/SPACE in the opt-in preview uses five groups, ordered **SENDS → FILTER → TEXTURE → REGION → TANK**:
- TEXTURE: `GRAIN`, `STR`, `MOTN`, `TONE` (23/46/93/186 ms grains).
- REGION: `POS`, `LENGTH`, `REVERSE`, read-only `SOURCE` state (FILL/WAIT/FIT/LIMIT).
- TANK: `TYPE`, `FEED` (grains into the hall), `DRY` (per track) and `HALL` (what the hall returns). FEED and HALL run as 0 / 100 / 300 % and are not saved; DRY is saved with the track.
The controls use the existing BUILD variables and stored A6 fields, no shadow copy. While TYPE=HALL, TEXTURE/REGION are inactive and TANK remains available to select STASIS. BUILD retains GRAIN and the appended REGION page with independent navigation.
PLAY FX-held and TYPE=STASIS: **B4 FREEZE toggle, C5 NEW SOURCE only after ≥800 ms continuous hold, D5 RELEASE**. C5's old PHASER shortcut and D5's old VERB shortcut are masked under STASIS, as are the former F5/G5 STASIS slots. The HOME-ACTION STASIS bank cannot trigger destructive NEW instantly. LCD FX legends have distinct highlighted/filled-inverted cells in both render paths; real hardware key LEDs remain monochrome.

## Reproducible tests and limits

On a complete private checkout after generating `build/gen` with the supported toolchain:

```sh
sh tools/stasis2_probe.sh
EMU_STASIS2=1 EMU_PREVIEW=1 sh web/emu/build.sh
node tests/stasis_v5_wasm_test.mjs build/emu/felucca.wasm
python3 tools/validate_all.py inventory
python3 tools/validate_all.py ui_storage
```

These changes include `tests/stasis_region_math_test.c` (live/writer/quality/length bounds), `tests/stasis_region_wire_test.c` (exhaustive 19-bit codec), `tests/format2_project_test.c` A6+legacy fixtures, native project/menu checks, actual FX DSP stereo-continuity and a physical button-path/WASM model check. The standalone region geometry (18 quality×grain×LIVE/FREEZE cases) and 19-bit wire test (294912 pos×length×quality×grain×reverse combinations) were compiled and executed independently with ASan/UBSan, but **the full firmware/host DSP/WASM/project path is not verified on this revision**.

Before merge/release: real JieLi target in normal, one-core and no-RAM-DSP modes, `.text/.bss/.pool/.ramtext` sizes, flag-off PCM parity, full FORMAT-2 old-A5/new-A6 editor/backup import/export, 240×240 12px screenshot and contrast/font checks, 10-minute FM-1 four-track+CAPTURE+AFTER audio ISR peak ≤80% and 0 underruns, 440Hz pitch ≤1cent, clean FREEZE/NEW/noise transition and HALL A/B checks. The exact target software and hardware evidence must be recorded separately.
