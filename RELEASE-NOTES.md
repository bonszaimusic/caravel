# Caravel 0.10.0

**A new engine: ENSEMBLE** (shown as ENSEMB, the last engine in the list). Strings, choir and dark
texture: three oscillator registers into one filter that morphs between strings, a resonant
choir formant and a rough dark timbre, and a built-in three-tap ensemble that sits inside the
track (before DIST, INSERT, FILTER and the sends, so reverb, HALL and STASIS hear it). Eight
controls (REG, MORPH, TONE, DRIFT, ENS, RATE, TEXT, MIX), at most six voices per track, MIDI CC 74
is TONE. **Fourteen factory presets**, each for a different job (string machine, choir, drone,
glass, swell, rough pulse, solo, deep ensemble, moving pad; PROFILE A has its controls set by
measurement to a reference profile, no audio of it is stored), and **five Pair Morph layer recipes**
with piano, bells, electric piano and a DRONE bed. The presets differ as measured signals; whether they
sound good has not been judged.

**Projects:** no format change. **A project that uses ENSEMBLE on any track shows as an empty slot
in 0.9.0** (host test with real flash images; nothing is deleted, saving over it in 0.9.0 replaces
it). Back up before installing. The version now reads `v0.10.0`; the installer and the release
scripts accept the two-digit minor number.

**What is tested.** Host tests with the real firmware code (three ENSEMBLE suites, the existing
regression, UI, editor, Pair, HOME and project suites) and the complete validation of the exact
source. @@SIZES_SHORT@@ CPU only as static counts. **Not tried on an FM-1 and not listened to on the
device**; the recipes and the CPU cost on hardware are not verified.

Download `Caravel-0.10.0-FM1-Testpaket.zip` for the local installer, firmware,
verified source and notices, or `Caravel-0.10.0-Emulator.html` for the offline
emulator. Firmware: `caravel-0.10.0.fwsc`. INFO must show `v0.10.0` after reboot.
[Details](https://github.com/bonszaimusic/caravel/blob/v0.10.0/docs/release-0100.md).
