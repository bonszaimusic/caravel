# Caravel 0.7.1

**Keep your four Random Morph destinations.** S1–S4 parameter snapshots now
travel with projects, RESTORE LAST and USB backups. Four slots retain 128
pinned targets each. A4 short selects, A4 held captures, A3 morphs to the slot
and A2 freezes/resumes. Save A–D; STOP commits saves made during playback.

The installer checks package size/SHA-256, product, layout and app/loader CRC
before install/resume. DIVE is excluded. STASIS audio, running morphs and
RETURN origins stay volatile.

**Back up before installing.** Old HPM1 projects load with empty slots. New
7,200-byte v2 projects need 0.7.1 or later; 0.7.0 cannot read them. Fixed flash
space and atomic dual copies remain. Incompressible data is rejected before
writing with `PROJECT TOO LARGE / NOT SAVED`.

Download `Caravel-0.7.1-FM1-Testpaket.zip` for the local installer, firmware,
verified source and notices, or `Caravel-0.7.1-Emulator.html` for the offline
emulator. Firmware: `caravel-0.7.1.fwsc`. INFO must show `v0.7.1` after reboot.
[Install, snapshot workflow and rollback details](https://github.com/bonszaimusic/caravel/blob/v0.7.1/docs/release-071.md).

Publication requires exact-source target/resource, native/USB/storage, audio,
sanitizer, WASM and complete validation. These do not establish physical FM-1
installation, power-cut, save/reboot/recall or USB install/resume acceptance
for 0.7.1; none is claimed. HALL/STASIS still omit ROOM, SPRING and the SLICE
engine; SLICER remains. Tempo DELAY is suspended while STASIS owns its buffer.
