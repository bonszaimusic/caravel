<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->

# Help with Caravel 1.0

Caravel **1.0.0 candidate** · BUILD / PLAY controls.

> **In 60 seconds** Check view, track, level and routing first. WAIT may mean CHORD EXT needs a source chord; FULL keeps the old loop.

## 1. Find the right view

HOME held one second switches PLAY and BUILD; HOME tapped opens the LEVELS explanation in PLAY or the track overview in BUILD. SAVE opens PROJECT. Double-tap GLO in BUILD for the menu. STEP, DETAIL and AUTOMATION intentionally keep the old grid.

## 2. No sound?

Check selected track, level, mute, engine and MIDI channel. Turn off ARP or LATCH for a simple key test. With CHORD EXT, WAIT/WAIT BUS can intentionally produce no note: play a source chord or enable HARMONY. A source never starts a silent follower by itself.

## 3. Capture did not take?

Start transport first. Play live panel keys or MIDI input, then tap REC in PLAY. ARP/SEQ output and loop playback do not fill the live ring. FULL means the complete take did not fit; the old loop is retained. CAPTURE loops are not saved in projects.

## 4. Old project or refused save?

OLD DATA FOUND is expected for pre-1.0 formats. Restore those with their original firmware and matching backup tool, not in 1.0. PROJECT TOO LARGE / NOT SAVED means the compressed performance capsule exceeded its existing 3,328-byte budget; reduce the setup and save again. Keep the previous saved copy until a successful save is confirmed.

## 5. Firmware recovery

Use the [FAQ](../faq.md) for USB detection and recovery. UC25HQ80 flash is recognized in code but has not been tested on a real unit by this project. m-bootloader is documented as an external recovery tool; it is not built into Caravel.

[Basics](basics.md) · [Installation](../installing.md)
