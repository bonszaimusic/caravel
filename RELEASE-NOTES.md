# Caravel 0.8.0

**Morph Journey.** A fifth ACTION bank, after LIVE, chains your Random Morph
snapshots S1–S4 into one long, hands-free movement: the sound morphs to a slot,
rests, moves on to the next slot and loops. A1 PLAY/PAUSE, A2 direction, A3 NEXT,
A4 tap WAIT, A4 hold saves. Leg length and curve are RANDOM SETUP's BARS and
SHAPE. It keeps running when you change bank, and MIDI Learn can map it to a
controller. A RANDOM morph on another track only holds it; it goes on by itself afterwards. Hold BANK in the JOURNEY bank to leave tracks out.
Snapshot slots are shared by the whole project: their owners are shown, and A4 held in RANDOM's SETUP deletes one. [Step-by-step tutorial](https://github.com/bonszaimusic/caravel/blob/v0.8.0/docs/tutorials/random-morph-journey.md).

**A MIDI CC now wins over a running morph for every target.** LFO2 and DRUM-X
controls kept being overwritten by the morph, because the ownership check
compared a truncated id.

**Performance settings travel with the project.** BARS, DEPTH, SCOPE, SHAPE,
WAIT and the direction are saved with each project next to S1–S4. **Back up before
installing: 0.7.x and earlier refuse a project saved by 0.8.0** (`PERF PROJECT
INVALID`). Older projects load with the defaults.

**What is tested.** Host tests with the real firmware, a stress suite (2.57 million
checks), and the complete validation of the exact source. Flash is tight: **1,116 B
are left in the app slot** (0.7.2: 5,600 B). **Not verified:** a controller on a real
FM-1, external MIDI clock (leg lengths follow the tempo setting), long real-device
runs and target timing. No hardware-tested claim is made for the journey.

Download `Caravel-0.8.0-FM1-Testpaket.zip` for the local installer, firmware,
verified source and notices, or `Caravel-0.8.0-Emulator.html` for the offline
emulator. Firmware: `caravel-0.8.0.fwsc`. INFO must show `v0.8.0` after reboot.
[Details](https://github.com/bonszaimusic/caravel/blob/v0.8.0/docs/release-080.md).

Publication requires exact-source target/resource, native/USB/storage, audio,
sanitizer, WASM and complete validation. These do not establish physical FM-1
acceptance beyond what is stated above. HALL/STASIS still omit ROOM, SPRING and the SLICE
engine; SLICER remains. Tempo DELAY is suspended while STASIS owns its buffer.
