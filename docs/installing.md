<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# Installing Caravel

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

Beginning with 0.3.4, Caravel uses the fixed package identity `FM-1_999`. Previous releases retain their old identities. Testing the new identity and installing/leaving the preview on real hardware are still pending.

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

To go back to the stock firmware, use **Return to official V15** on the installer page and select the unmodified official `FM-1.fwsc`
from M-VAVE. The browser validates the file locally; nothing is sent to a server. Caravel projects do not open in the official V15.

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

A package built from source ([BUILDING.md](../BUILDING.md)) installs the same way. For an **unreleased test package**, generate a local browser preview and serve it over `localhost` (Web MIDI needs a secure context):

```sh
./build.sh --binary-version 0.3.4
python3 web/make_site.py build/caravel-0.3.4.fwsc 0.3.4 /tmp/caravel-site --preview
cd /tmp/caravel-site && python3 -m http.server 8000
# open http://localhost:8000/webapp/installer/ in Chrome
```

Or download the private CI artifact named `caravel-0.3.4-browser-preview-...` from the development pull request, extract it and run:

```sh
cd preview-site && python3 -m http.server 8000
# open http://localhost:8000/webapp/installer/ in Chrome
```

Connect the FM-1 via USB and back up projects/settings before attempting to flash. This is an unverified preview, not a release. The public web installer continues to offer only the latest **published** version.
