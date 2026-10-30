<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Troubleshooting & FAQ

USB connections, firmware changes and recovery for the M-VAVE FM-1.
Last reviewed: **30 October 2026**. Reported recoveries are individual observations;
they do not establish a universal fix or the cause of a fault.

[Web installer](https://bonszaimusic.github.io/caravel/webapp/installer/) ·
[Installation guide](installing.md) ·
[Report a problem](https://github.com/bonszaimusic/caravel/issues/new/choose)

**Before installing or restoring firmware:** save your projects, presets and settings
while communication still works. Keep the backup outside the FM-1. Do not unplug
USB or switch off during writing. Recovery can change or erase stored data.

## Find your symptom

| What you see | Start here |
| --- | --- |
| Windows reports an unknown USB device or a failed device-descriptor request | [USB device not recognized](#usb-device-not-recognized) |
| The computer sees MIDI, but the browser says FM-1 not found | [FM-1 not found](#fm-1-not-found) |
| Caravel's installer works, but another installer does not find the unit | [Changing firmware](#changing-firmware) |
| Return to official firmware stops with Update Failed -5 | [Stock return error -5](#stock-return-error--5) |
| You need to understand OCT-/OCT+, M-Flasher or m-bootloader | [Recovery tools](#recovery-tools) |

## USB device not recognized

**Symptom:** Windows Device Manager shows an unknown USB device, possibly with
“Device descriptor request failed”. Charging and sound can still work.

1. Use a known working **data cable** directly into the computer. Try another
   port without a hub. Charging alone does not confirm a working data path.
2. With no update in progress, compare starting the FM-1 with USB already
   connected and connecting it after Caravel has started.
3. If available, check another computer or an Android phone with USB host/OTG
   support and a suitable data connection. Record whether the operating system
   sees any USB device; a browser's “not found” message is less specific.
4. If Caravel runs but USB remains unavailable, see [UBOOT](#uboot-and-the-normal-update-loader).
   Record what the computer detects in that mode.

**Reported recovery — [issue #22](https://github.com/bonszaimusic/caravel/issues/22#issuecomment-6522017444):**
With Caravel 0.3.3, a user reported unsuccessful Windows and Android attempts.
After repeating OCT-/OCT+ and changing to a front PC port with a **USB-C-to-C**
connection, M-Flasher detected the FM-1 and restored stock firmware. A subsequent
black screen cleared after reconnecting the same C-to-C path and switching on again.
The issue was closed after recovery. Port, cable path and retry count changed
together: this does **not** prove that C-to-C is required, or that repeated attempts
will fix every unit. The original cause remains unconfirmed.

## FM-1 not found

If the operating system recognizes the MIDI device, try the
[Caravel installer](https://bonszaimusic.github.io/caravel/webapp/installer/) in
Chrome or Edge, allow MIDI/SysEx access, and close other browser tabs and apps
that use MIDI. Private browsing is not a required recovery step.

If the operating system cannot recognize the device at all, first follow
[USB device not recognized](#usb-device-not-recognized).
If the unit is in UBOOT, the normal Web MIDI installer is not the matching tool.

**macOS audio-only problem:** if MIDI works but USB audio is missing, try
**MENU → SYSTEM → USB SERIAL → OFF**, exit the menu and reconnect.
This removes the optional developer serial interface while retaining USB-MIDI.
It is not an established fix for every descriptor or detection failure.

## Changing firmware

**Symptom:** Caravel's installer can communicate with the FM-1, but another
firmware installer cannot find it. This differs from an operating-system USB failure.
Different installers may recognize different device names or firmware identities;
check the destination project's supported installation path.

In a [community report](https://www.facebook.com/groups/1672219544562534/posts/1766323391818815/),
one user selected a previously saved official **V15 FM-1.fwsc** in Caravel's
**Return to official V15** flow, restored V15, then successfully installed Carnac.
This is a reported workaround, not proof that every firmware change requires V15.
The thread's separate error -5 report did not confirm the same successful recovery.

Use the exact official package requested by the installer; do not rename a newer
package to make it appear to be V15. Back up first: official firmware does not
open Caravel projects. If the return fails with -5, follow the next entry.

## Stock return error -5

**Known open investigation — [issue #4](https://github.com/bonszaimusic/caravel/issues/4).**
The reported sequence is verification reaching request 4, followed by
**Update Failed -5**; Caravel itself continues working.

The maintainer traced that refusal to a comparison of the package's first
16 KiB against the device's flash head, before staging or flash writing.
A valid package hash alone does not establish a matching device head.
Successful Caravel-to-Caravel updates do not prove stock restoration will work.

Keep the complete log, installed Caravel version and exact official package
identity/hash. Follow the current diagnostic instructions in issue #4.
There is **no confirmed fix for the affected unit** in that issue as of the review
date. Do not bypass the head check or treat the diagnostic build as a stock-return fix.

M-Flasher uses a different recovery path, but it is not a confirmed solution
for the specific -5 report. Its availability depends on USB download-mode detection.

## UBOOT and the normal update loader

They are different connection modes:

| Mode | How it is used |
| --- | --- |
| Normal Caravel / its USB-MIDI update loader | The web installer starts the update handshake. An interrupted loader write can be resumed with the same installer. |
| Chip USB download mode (UBOOT) | Recovery tools such as M-Flasher communicate outside Caravel's normal MIDI implementation. The normal web installer may not find it. |

With **Caravel already running**, hold **OCT- and OCT+ together for about
5 seconds**. A countdown appears, then Caravel requests UBOOT; this transition
alone does not erase projects. Inspect the new USB entry on the computer.
The upstream M-Flasher guide describes the download-mode device as **WL82 UBOOT1.00**.

Holding both octave buttons **during power-on** opens Caravel's hardware
calibration instead. The running-firmware shortcut cannot help if Caravel never
reaches its main loop. A blank screen alone does not confirm UBOOT or a permanent failure.

## Recovery tools

| Tool | What it is |
| --- | --- |
| [M-Flasher](https://github.com/jvitkauskas/m-flasher) | An independent Windows/macOS/Linux application for whole-flash backups, restoration and firmware installation through the chip's USB download mode. |
| [m-bootloader](https://github.com/jvitkauskas/m-bootloader) | An optional replacement bootloader installed on the FM-1. Its power-on REC shortcut enters download mode; PLAY opens its recovery menu. These keys require m-bootloader to have been installed. |

M-Flasher saves a backup before writing and verifies writes. It still needs the
device to be reachable in download mode; it cannot communicate through an
undetected USB connection. Installing m-bootloader changes the boot area and is
a separate operation, not a prerequisite for every M-Flasher recovery.

Follow the upstream [M-Flasher usage guide](https://github.com/jvitkauskas/m-flasher/blob/main/docs/usage.md)
and check the proposed write and settings-erasure options before confirming.
Keep its full-flash backup. Both tools are community projects, not M-VAVE products;
Caravel does not install them automatically or guarantee recovery.

## What to include in a bug report

- Caravel version/build; computer OS or Android model/version; browser.
- Cable/adapter type, direct port or hub, and checks already attempted.
- Exact OS device name/error and hardware IDs, in normal mode and UBOOT if reached.
- Whether the octave-button countdown and UBOOT appeared; full installer log
  and the stage where it stopped.
- Package version and SHA-256 for a failed restore. Redact personal paths or identifiers;
  do not attach raw flash dumps, boot/key data or private projects publicly.

Add results to the matching existing issue, or [open a new issue](https://github.com/bonszaimusic/caravel/issues/new/choose).
