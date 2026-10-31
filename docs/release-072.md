<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Caravel 0.7.2

## The stock return (-5)

Going back to M-VAVE's firmware with **Return to official V15** used to stop with
**Update Failed -5** on some units. The running firmware compared the official
package's first 16 KiB with the unit's own raw flash head and refused the update,
before anything was staged or erased, when they differed. A matching package hash
shows the file is the official one; it says nothing about your unit's head.

0.7.2 replaces that comparison with a structural check of the package's own boot
area: the flash header and boot record, the boot container at its fixed place,
their CRCs, the load address and, where present, the FM-1 board tag. A package
that fails any of it is still refused with -5 before anything is erased.
Unchanged: the package size check, the recognition of the update loader, the
staging and the final confirmation from the host. Updates from Caravel to Caravel
and the installer's own preflight work as before. The check follows
[felucca-bsp afd83ed](https://github.com/hugelton/felucca-bsp/commit/afd83ede7296de787a7d4cf90e6103b7ed9ac02e).

**If you saw -5:** update to 0.7.2 the normal way, **back up first** (the return
can erase projects and the FM6 voice bank), then try *Return to official V15* again
with the unmodified official `FM-1.fwsc`. If it still stops with -5, please write
the number and your installed version in [issue #4](https://github.com/bonszaimusic/caravel/issues/4)
or a new issue. Felucca 1.7.0 as an intermediate step remains a working route.

## Other fixes

- A divide by zero in the beat length when the BPM is briefly 0 (Felucca #282).
- FM6 operators with AMS other than 0 went silent or full scale, even with AMD at 0
  (Felucca #304). Factory voices have AMS 0 and are unchanged; voices imported with
  AMS set now follow the LFO.
- CI only: drafts and documentation-only changes no longer start the complete
  validation; workflow and report names no longer carry 0.6.0.

## Install

Back up first. Use the [web installer](https://bonszaimusic.github.io/caravel/webapp/installer/)
or download `Caravel-0.7.2-FM1-Testpaket.zip` from the
[release](https://github.com/bonszaimusic/caravel/releases/tag/v0.7.2). After reboot
INFO must show `v0.7.2`. Firmware: `caravel-0.7.2.fwsc`. Matching offline emulator:
`Caravel-0.7.2-Emulator.html`. See the [FAQ](faq.md) and the
[installation guide](installing.md) for USB discovery, OCT-/OCT+ and recovery.

## What is tested, and what is not

- Host tests of the update entry with M-VAVE's V15 package and a Caravel package:
  a valid package is accepted whether the device's head is blank, scrambled or
  different; damaged CRCs, load address, bank count, payload position and another
  board's tag are refused with nothing erased; an interrupted transfer or a missing
  confirmation leaves the staging area clean. The old check fails exactly the
  different-head cases.
- Public build and complete validation of the exact source.
- One FM-1 whose boot area equals V15's: the 0.7.2-based test build installed,
  went directly back to the official V15 and ran normally, then Caravel was
  reinstalled and the backup restored.

**Not tested:** a unit that actually showed -5; V16 or other update writers (the
recognition of M-VAVE's loader is unchanged); power loss during the update. Treat
the return as a candidate fix and keep your backup.

No project-format, storage, audio-engine or USB changes; the S1–S4 snapshots and
0.7.1 projects work as before.
