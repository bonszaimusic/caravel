# Caravel 0.9.0

**Random Morph and Morph Journey are per track.** Every track now has its own four
snapshot slots (up to 32 targets each, its own HOME controls), its own morph engine and
its own journey with its own BARS, DEPTH, SCOPE, SHAPE, WAIT and direction. Morphs and
journeys of different tracks run side by side; a RANDOM morph holds only its own track's
journey; a control two tracks move belongs to the track that started last; SCOPE ALL
makes the RANDOM keys act on every track. A track whose HOME pins more than 32 targets
cannot start a RANDOM morph. The JOURNEY bank now follows RANDOM, small boxes show what
each track does, and **JOURNEY SETUP** (hold BANK there) sets SCOPE THIS / ALL and
TRANSP FREE / CONT / START (CONT and START follow the sequencer's bars: Stop freezes a leg,
a tempo or external clock change moves it, START begins again at each Start).
[Tutorial](https://github.com/bonszaimusic/caravel/blob/v0.9.0/docs/tutorials/random-morph-journey.md).

**The SAX sample set is retired** to free flash. A sound, user preset or project that
used SAX now plays PIANO; the GRAIN preset GLITCH uses PIANO as its source.

**Back up before installing: projects use a new format (3).** Projects from every earlier
version load; those from 0.7.1 to 0.8.0 are migrated into per-track slots, at most 32
targets per slot (more are dropped and the load says so). **0.8.0 does not read a project
saved by 0.9.0: the slot shows as empty there** (nothing is deleted; saving over it in
0.8.0 replaces it). Going back means restoring a backup.

Also: the OVERWRITE? dialog of a project SAVE no longer flickers while a morph or journey
runs (also in 0.8.0), RANDOM moves the FX filter only between -70 and +70, FM6's patch
selector is never captured (issue #35).

**What is tested.** Host tests with the real firmware, a stress suite (10.65 million
checks), a simulated external MIDI clock with ideal pulses, and the complete validation of
the exact source. About 25 KB of flash are free again (0.8.0: 352 B). **Not tried on an
FM-1 before publication.** Not verified: a controller, a real external MIDI clock, long
real-device runs and the CPU cost of the new transport counter.

Download `Caravel-0.9.0-FM1-Testpaket.zip` for the local installer, firmware,
verified source and notices, or `Caravel-0.9.0-Emulator.html` for the offline
emulator. Firmware: `caravel-0.9.0.fwsc`. INFO must show `v0.9.0` after reboot.
[Details](https://github.com/bonszaimusic/caravel/blob/v0.9.0/docs/release-090.md).

Publication requires exact-source target/resource, native/USB/storage, audio,
sanitizer, WASM and complete validation. These do not establish physical FM-1
acceptance. HALL/STASIS still omit ROOM, SPRING and the SLICE
engine; SLICER remains. Tempo DELAY is suspended while STASIS owns its buffer.
