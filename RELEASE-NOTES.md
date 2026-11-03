# Caravel 0.10.1

A fix release on 0.10.0 (the ENSEMBLE engine); nothing else changed.

**Fixed:** in RANDOM the ship on the sea chart sailed between the snapshots without any change of a parameter. The selected slot
(the one the ship heads for, and the one GO and the `Sx >` key name) was set by every leg of any track's journey, so a journey on another track
(or JOURNEY scope ALL) moved the selected track's ship and the labels. It now follows only the selected track's journey. Reproduced on the host
(journey_hold_test, a new section that failed before); the same in 0.9.0 and 0.10.0. The cause found is the one the test reproduces; whether it is the only way the ship can move
without a journey on the selected track has not been shown.

**Projects:** no format change (still format 3); a drop-in update of 0.10.0. Back up before installing, as always.

**What is tested.** The host journey suites with the new section and the complete validation of the exact source; the margins of the three layouts are checked by the release gates.
**The fix was not tried on an FM-1 before publication** (the symptom was seen on the device by the maintainer; the cause was reproduced on the host). Everything said about
0.10.0 on hardware still applies, and no judgement of how ENSEMBLE or the presets sound has been recorded.

Download `Caravel-0.10.1-FM1-Testpaket.zip` for the local installer, firmware,
verified source and notices, or `Caravel-0.10.1-Emulator.html` for the offline
emulator. Firmware: `caravel-0.10.1.fwsc`. INFO must show `v0.10.1` after reboot.
[Details](https://github.com/bonszaimusic/caravel/blob/v0.10.1/docs/release-0101.md).
