# Caravel 0.11.0

**ENSEMBLE in stereo.** SPRD (the track's SPREAD, EDIT > MORE) is now the width of the ENSEMBLE engine: six delay taps, three for the left channel and three for the right. SPRD 0, MONO and LEGATO are the mono ensemble of 0.10.1,
bit for bit. **The factory pads load with a width** (VINTAGE STRINGS, GLASS CHOIR, DUSK ORCHESTRA, ABYSS PAD, PROFILE A, SLOW SWELL and RUNNING PAD at 127, STRING MACHINE and DEEP DRONE at 112, BREATH CHOIR and HUGE ENSEMBLE at 96;
GLASS SHIMMER, ROUGH PULSE and SOLO DRIFT stay mono); your own presets and projects keep the width they store.

**Pair Morph:** a member may now have SPRD above 0 on any engine (before: `SPREAD OFF`). The morph weight scales the member's sum and its side signal.

**What it changes in the sound.** The mono sum is about 2 to 3.5 dB lower because the two channels are only partly alike, and the reverb return of a wide pad is clearly lower (about 6 dB in the measured case, part of it unexplained): raise REV or LEVEL if a pad
sounds drier. About 80 more host instructions per ENSEMBLE track with a width; 21,164 B of flash stay free (0.10.1: 21,944 B); RAM and pool are unchanged.

**Projects:** no format change. A project with a stereo ENSEMBLE pad loads on 0.10.1 and plays it mono (the width stays stored). Back up before installing.

**What is tested.** Host tests with the real firmware code (the ENSEMBLE suites, all 225 ordered engine pairs with SPRD on both members, the regression with 11 ENSEMBLE renders renewed on purpose, the editor mock), the complete validation, and a local Linux container run of the
ACTION job and the host shards. **On hardware:** the maintainer reports having checked everything on an FM-1 with a release-candidate test build (0.11.0-rc3, the code of this release apart from the version text); no per-item findings are recorded. The maintainer had also listened to host renders of the
stereo ENSEMBLE and judged them good. Not measured: the CPU on the device; part of the reverb-return loss is not explained.

Download `Caravel-0.11.0-FM1-Testpaket.zip` for the local installer, firmware,
verified source and notices, or `Caravel-0.11.0-Emulator.html` for the offline
emulator. Firmware: `caravel-0.11.0.fwsc`. INFO must show `v0.11.0` after reboot.
[Details](https://github.com/bonszaimusic/caravel/blob/v0.11.0/docs/release-0110.md).
