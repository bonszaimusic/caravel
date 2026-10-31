# Caravel 0.7.2

**A candidate fix for the stock return (-5, issue #4).** *Return to official V15*
used to require the official package's first 16 KiB to match your unit's flash
head byte for byte, and stopped with *Update Failed -5* when they differed. It now
checks that the package's boot area is valid (header, boot record, container,
CRCs, load address, FM-1 board tag) and still refuses a damaged one before
anything is erased.

**What is tested.** The check ran against M-VAVE's V15 package on the host with
valid, damaged and differing device heads, and in the public build and
validation. On one FM-1 whose boot area equals V15's, the 0.7.2 build installed,
went straight back to the official V15 and ran normally. **It is not yet
confirmed on a unit that showed -5.** Back up first; the return can erase projects.

Also: a divide by zero when the BPM is briefly 0 is fixed, and FM6 operators
with AMS other than 0 no longer go silent or full scale (imported voices only;
factory voices are unchanged). CI-only changes do not touch the firmware.

No project-format, storage, audio-engine or USB changes; 0.7.1 projects load as
before. Download `Caravel-0.7.2-FM1-Testpaket.zip` for the local installer,
firmware, verified source and notices, or `Caravel-0.7.2-Emulator.html` for the
offline emulator. Firmware: `caravel-0.7.2.fwsc`. INFO must show `v0.7.2` after reboot.
[Details](https://github.com/bonszaimusic/caravel/blob/v0.7.2/docs/release-072.md).

Publication requires exact-source target/resource, native/USB/storage, audio,
sanitizer, WASM and complete validation. These do not establish physical FM-1
acceptance beyond what is stated above. HALL/STASIS still omit ROOM, SPRING and the SLICE
engine; SLICER remains. Tempo DELAY is suspended while STASIS owns its buffer.
