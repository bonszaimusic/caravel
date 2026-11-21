<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Installing Caravel


> **Caravel 1.0 candidate:** start fresh with FORMAT 2. Back up before updating using the old matching firmware/tool; pre-1.0 backups are not imported into 1.0. Keep the old firmware and backup for a return. [Current formats, controls, limits and device checklist](release-100.md). Historical recovery observations below keep their original version scope. UC25HQ80 is recognized but not tested on a real unit by this project; m-bootloader remains an external, documented tool, not integrated firmware.

**Connection or update problem?** See the [Troubleshooting & FAQ](faq.md), including
[USB detection](faq.md#usb-device-not-recognized), [stock-return error -5](faq.md#stock-return-error--5)
and [M-Flasher / m-bootloader](faq.md#recovery-tools).

Two ways to put a Caravel package (`.fwsc`) on an FM-1: the **web installer** in a browser, or `tools/fm1_install.py` from a terminal.
Both run the same update over USB-MIDI and check the installed identity afterwards. Hardware acceptance of Caravel on a real FM-1 is
still pending: read the notes below first.

## Read this first

- Installing custom firmware is at your own risk, without warranty to the extent permitted by law. A failed update can leave the FM-1
  unusable or need a Transporter for attempted recovery: [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).
- Returning to the official V15 attempts an official reinstall; it does not guarantee factory condition, data restoration, warranty
  coverage or recoverability. Keep the official package and backups of your projects and settings.
- The firmware's USB IDs are test IDs; see the note under *Installing* in the [README](../README.md) and [NOTICE](../NOTICE).
- Projects and settings that use Caravel-only features need Caravel or newer; see the compatibility notes in the README.
- Hardware acceptance on a real FM-1 is pending; host tests and the emulator are not hardware measurements.
- The web installer shows these warnings on its page and, for the way back to V15, saves a complete backup first. The command-line
  installer prints a short notice, asks for confirmation (unless `--yes`) and makes **no** backup: save your projects and settings
  yourself before you use it.

## Which release

Every release on [GitHub Releases](https://github.com/bonszaimusic/caravel/releases) has the package `caravel-X.Y.Z.fwsc`, the
firmware app alone (`caravel-X.Y.Z-app.bin`), the update loader, the complete source (`caravel-X.Y.Z-source.tar.gz`) and
`SHA256SUMS` with the licence texts. To install a specific version, take the package of that release. The latest release is the
one the web installer offers.

Check the download against `SHA256SUMS` of the same release before installing:

```
shasum -a 256 caravel-0.1.0.fwsc        # compare with the line in SHA256SUMS (macOS; sha256sum on Linux)
```

## Web installer

Open the [web installer](https://bonszaimusic.github.io/caravel/webapp/installer/) in Chrome or Edge (it needs Web MIDI), connect the
FM-1 over USB and follow the page. It installs the package of the published release. The
[web editor](https://bonszaimusic.github.io/caravel/webapp/editor/) and the
[browser emulator](https://bonszaimusic.github.io/caravel/emulator/) are on the same site.

Before the **Install** button is enabled, the browser now checks the exact package
size and SHA-256 from the built site's release metadata, verifies the FM-1 product
identity, decodes the UFW/flash headers, checks the application CRC and flash
layout, and validates the OTA loader header and CRC. It refuses to start *or
resume* an update if the package fails. These checks also apply to locally built
preview sites, with hashes pinned to the local build. A digest embedded in the
same site as its package is a consistency check, **not** a cryptographic signature
of the publisher; verified releases are checked against source/build records
when the site is generated.

This prevents known malformed or mismatched files reaching the update process.
It does **not** prevent a power cut, an interrupted USB transfer during writing,
or a firmware application that cannot boot. Keep your backups and official V15.


To go back to the stock firmware, use **Return to official V15** on the installer page and select the unmodified official `FM-1.fwsc`
from M-VAVE. The browser validates the file locally; nothing is sent to a server. Caravel projects do not open in the official V15.

### M-Flasher / ROM UBOOT (Caravel 1.x only)

For a **running Caravel 1.x** device, open the 1.x web installer in Chrome or Edge
and expand **Recovery → Enter UBOOT for M-Flasher**. Authorize Web MIDI/SysEx
and confirm the action. The page requires protocol 18 and sends only the
Caravel UBOOT request. **No firmware package is required; this button never
writes, erases, verifies or restores flash.** A successful browser send does
not confirm the USB mode transition.

Afterward, start [M-Flasher](https://github.com/jvitkauskas/m-flasher)
and check whether the chip's USB download-mode device appears (`WL80UBOOT1.00`, USB ID
`4c4a:8057`, measured on a real FM-1; other chip revisions may report
`WL82 UBOOT1.00` as in the M-Flasher guide). M-Flasher's older MIDI scan can fail to recognize
Caravel by name while the firmware is running; once ROM UBOOT has been
entered, it uses a separate USB interface. Alternatively, with the
firmware running **and playback stopped**, hold **OCT- and OCT+ together
for approximately five seconds** to enter UBOOT. Holding them at
power-on runs calibration instead.

Back up before any **subsequent** flash/restore action in M-Flasher;
that external operation may erase data even though the Caravel mode switch
does not. If the device is already in ROM UBOOT, the browser's Web MIDI
control cannot find it. This feature does not install m-bootloader,
change USB identities, permit Classic firmware updates or bypass any
Caravel package checks. Hardware detection is system-dependent; see
the [FAQ](faq.md#uboot-and-the-normal-update-loader).

## Command line

Needs Python 3 and the MIDI libraries:

```
git clone --recurse-submodules https://github.com/bonszaimusic/caravel.git
cd caravel
pip3 install mido python-rtmidi
```

Download the package of the release you want next to it and install it:

```
curl -LO https://github.com/bonszaimusic/caravel/releases/download/v0.1.0/caravel-0.1.0.fwsc
python3 tools/fm1_install.py caravel-0.1.0.fwsc
```

(Replace `0.1.0` in both places with the version you want.) Without a clone, `tools/fm1_install.py` alone is enough; it needs only the
two Python packages.

| Command | What it does |
| --- | --- |
| `python3 tools/fm1_install.py PACKAGE.fwsc` | installs the package, asks for confirmation |
| `python3 tools/fm1_install.py --info` | prints the identity of the connected FM-1 |
| `python3 tools/fm1_install.py FM-1.fwsc` | the official V15 file from M-VAVE: back to the stock firmware |
| `--port NAME` | uses the MIDI port whose name contains NAME |
| `--yes` | does not ask for confirmation |
| `--force` | installs a package without the update loader's marker (normally not needed) |

If the FM-1 is still in update mode because an earlier install was cut off, running the install again finishes the write.

Exit codes: 0 done, 1 cancelled or another error, 2 bad arguments or package, 3 FM-1 not found, 4 connection lost or the device
stopped, 5 timeout (no loader or no restart), 6 wrong model or another identity after the install.

## Your own build

A package built from the source ([BUILDING.md](../BUILDING.md)) installs the same way. To use the web installer for it, build a local
copy of the site and open it from `localhost` (Web MIDI needs a secure context):

```
python3 web/make_site.py build/caravel.fwsc dev /tmp/caravel-site
cd /tmp/caravel-site && python3 -m http.server 8000
# open http://localhost:8000/webapp/installer/
```

## 0.6.0 profile and upgrade

The public 0.6.0 package contains HALL/STASIS and omits ROOM, SPRING and the
SLICE engine. The SLICER track effect remains. Back up first; incompatible
projects offer CANCEL / TEST, preserve their original slots and can be saved as
a separate copy. See [the STASIS tutorial](tutorials/home-and-stasis.md).

Versioned builds use the fixed FM-1_999 identity prepared in 0.3.4; existing
published releases retain their original packages and identities. HPM1 performance
projects need matching backup tools. Preserve a pre-upgrade backup for downgrades.
The loader marker remains unchanged. Test the final version label and USB identity
before publication; D3 hardware acceptance does not automatically certify every
version-labelled rebuild.
