# Credits

Caravel is bonszaimusic's fork of Felucca 1.6.1, which is Leo Kuroshita's ([@kurogedelic](https://github.com/kurogedelic),
[Hügelton Instruments](https://hugelton.com)); see [NOTICE](NOTICE). Caravel is not affiliated with Hügelton Instruments. It uses and
thanks the work below; each keeps its own licence (see [LICENSE](LICENSE), [LICENSING.md](LICENSING.md) and [LICENSES/](LICENSES/)).

- **bonszaimusic:** Caravel, the changes to Felucca 1.6.1 (renaming, the two-group menus, SEM, HADES,
  DRONE, DRUM-X integration, LFO2, MORPH, voice reservations, clock fixes), as summarized in CHANGELOG.md and the Caravel baseline commit
- **[Hügelton Instruments](https://hugelton.com)** (Leo Kuroshita, [@kurogedelic](https://github.com/kurogedelic)):
  Felucca itself; the PHASE engine's waveforms (a C port of the oscillator of
  [CrispyZebra](https://github.com/hugelton/CrispyZebra), GPL-3.0); the DRUM voices and kits; the Hügelton Sample
  Pack (the drum samples, GPL-3.0-only, not CC0); the [Fukiai](https://github.com/hugelton/Fukiai) icon
  font ([MIT](LICENSES/MIT-Fukiai.txt)); the ANALOG, LOFI, TRIO, WHEEL and NOISE engines
  ([Felucca-Engines](https://github.com/hugelton/Felucca-Engines), [MIT](LICENSES/MIT-Felucca-Engines.txt))
- Fonts: [Inter Tight](https://github.com/rsms/inter-tight) by The Inter Project Authors, [SIL OFL 1.1](LICENSES/OFL-InterTight.txt);
  the browser emulator's labels: [DotGothic16](https://github.com/fontworks-fonts/DotGothic16) by The DotGothic16 Project Authors, [SIL OFL 1.1](LICENSES/OFL-DotGothic16.txt)
- Samples: [Versilian Studios](https://versilian-studios.com/) [VSCO-2 Community Edition](https://github.com/sgossner/VSCO-2-CE) and [VCSL](https://github.com/sgossner/VCSL), CC0 1.0: the SAMPLE sets, also SLICE's PIANO and PIANO HD ([attribution](assets/samples-cc0/ATTRIBUTION.txt))
- VOICE engine: after [klattsch](https://github.com/tgies/klattsch) by Tony Gies (MIT); formant data from Klatt (1980) and Hillenbrand et al. (1995)
- PHYS engine: models ported from [DaisySP](https://github.com/electro-smith/DaisySP) by Electrosmith and Emilie Gillet ([MIT](LICENSES/MIT-DaisySP.txt)) and from Emilie Gillet's [eurorack](https://github.com/pichenettes/eurorack) code ([MIT](LICENSES/MIT-Rings.txt))
- FM6 engine: msfa from [Dexed](https://github.com/asb2m10/dexed) by Google Inc. and Pascal Gauthier ([Apache-2.0](LICENSES/Apache-2.0-msfa.txt))
- Second core: its start, mailbox and guards from [X0X](https://github.com/charlesvestal/fm1-x0x) by [charlesvestal](https://github.com/charlesvestal)
  with the dual-core findings of [Melodee](https://github.com/keremimo/melodee) by [keremimo](https://github.com/keremimo); its audio split,
  measurements and hardening after [Jangada](https://github.com/zednaked/jangada) by [zednaked](https://github.com/zednaked) (all GPL-3.0)
- Browser emulator: after [X0X](https://github.com/charlesvestal/fm1-x0x) by [charlesvestal](https://github.com/charlesvestal) (GPL-3.0), a Felucca fork whose browser build showed the way
- Package format and boot files: [JieLi AC79 SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK) ([Apache-2.0](LICENSES/Apache-2.0.txt); three of its files are in every package, none in this tree)
- Contributions: [keremimo](https://github.com/keremimo) (white-key scales, #2), [ChanceTheMaker](https://github.com/ChanceTheMaker)
  (TRS MIDI, bend, sustain and clock, palettes, favourites, editor display settings: #8, #10, #11, #12),
  [andreahaku](https://github.com/andreahaku) (sample recording and trim, #29; SLICE manual slices and tests, #27, #22),
  [spinkham](https://github.com/spinkham) (the boot fix for 0.9 projects, #111; the emulator's MIDI knobs, #182),
  [ugai](https://github.com/ugai) (the Try page's horizontal knob drag, #240),
  [zednaked](https://github.com/zednaked) (ratchets, #100),
  [jasonpersinger](https://github.com/jasonpersinger) (the ROOM reverb click fix, #121),
  [renebohne](https://github.com/renebohne) (MIDI Program Change, #179),
  [gospodindilly-ops](https://github.com/gospodindilly-ops) (the second core, #247)

- LOFI BYTE/FLOAT formula helpers: ported from [JIANT](https://github.com/juanjiant-bit/JIANT/tree/d5f99ee2d4277adefcdf8f892e34133d33ec27a6), GPL-3.0-only; source retains Leo Kuroshita / Hügelton attribution and formula acknowledgements (Tejeez, xpansive, viznut). The Felucca integration preserves the original five LOFI waves.

- Custom MORPH / PHASE feedback: adapted from `juanjiant-bit/JIANT` at `d5f99ee2d4277adefcdf8f892e34133d33ec27a6` (GPL-3.0-only), retaining Leo Kuroshita / Hügelton Instruments attribution. Felucca FOLD, SYNC/SUB, filters and legacy defaults are retained.

- Custom DRUM-X: fixed-point synthesis and factory A/B sounds adapted from [JIANT](https://github.com/juanjiant-bit/JIANT/tree/aaa73f0465cfe4519d586fe86f077a2ea45b3404), GPL-3.0-only, retaining Leo Kuroshita / Hügelton Instruments attribution. Custom integration uses track-local kits, fixed MIDI Learn targets and compatible engine-specific storage.
  DRUM-X history: the idea follows Sonic Charge's Microtonic (a tuned oscillator with pitch envelope and FM, filtered noise, two
  patches and a MORPH); JIANT (juanjiant-bit, a Felucca fork) built it as "FELUCCA TONIC" in fixed point for the FM-1; Caravel
  adapted it (track-local kits, fixed MIDI Learn targets, compatible storage) and continues it. None of these projects is affiliated with Caravel.
