# Caravel 0.6.0 — STASIS 060 performance

This is the complete project used for the feature Short: Track 1 SEM + SLICER, Track 2 deep SEM, Track 3 a stable driven DRONE, plus 12 HOME controls and the T1/T2 Pair. Project name: STASIS 060. Requires Caravel 0.6.0 with HOME/PAIR/STASIS support.

## Import on the FM-1

1. Connect the FM-1 by USB. Stop playback. Close other MIDI editors.
2. Install Python 3, then run: `python3 -m pip install mido python-rtmidi`.
3. In this folder run `python3 caravel_backup.py ports` to check the Caravel MIDI ports.
4. Import into a project slot you can replace. Example for D: `python3 caravel_backup.py import D Caravel_060_STASIS_HOME.hpm`. The tool asks before writing. If ports are ambiguous use its --input and --output options.
5. On the FM-1 open SAVE > PROJECT, select D and load it. The import writes the chosen saved slot; it does not flash firmware. The chosen slot is replaced with a project for ALL FOUR TRACKS, including sounds, HOME and Pair.

Use another A–D slot in the command if needed. Save an existing target project separately first if you want to keep it.

## What the file contains

Caravel_060_STASIS_HOME.hpm: 10000-byte HPM1 interchange project. This exact file passed the real firmware's simulated USB BACKUP_PUT import, flash readback and project-load path. Three HOME rows, fixed Track 3 controls, SEM/SEM/DRONE and Pair T1/T2 were verified. No physical FM-1 was connected during this check.

The HPM is a STARTING SETUP, not a recording of all knob movements. The note-only MIDI file uses channel 1 for the SEM Pair, channel 3 for Drone and 90 BPM. It sends no CC or SysEx and does not trigger HOLD/BLEND/STOP. Play it to the Caravel MIDI output with MIDI clock/start/stop sending disabled, then perform these gestures.

## HOME on Track 1

| Row | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| 1 | Pair MORPH | T3 LEVEL | T2 SEM CUT | Pair LEVEL |
| 2 | T1 SLICER | PATTERN | RATE | DEPTH |
| 3 | T1 SEM CUT | T2 REV | T1 REV | T3 DIST |

SELECT changes the row. Keep Track 1 selected for these controls. A preset saves a sound; a PROJECT saves this whole HOME/Pair setup.

## Moves (time relative to the 30-second performance)

| Time | Control | Move |
| --- | --- | --- |
| 10–13 s | HOME row 1, K1 MORPH | 100 → 35% T2 |
| 13.5–15 s | HOME row 2, K4 DEPTH | 0 → 44% |
| 15.5–17 s | FX REVERB, K3 STR | 8 → 47% |
| 17.2 s | FX REVERB, K2 CAPT | LIVE → HOLD |
| 18–19.2 s | HOME row 3, K1 CUT | 1.0 kHz → 322 Hz |
| 20 s | FX LAYER, BLEND / TIME 3 s | Turn K4 right, then OCT+ |
| 21.1–22.1 s | HOME row 3, K1 CUT | 322 Hz → 1.9 kHz |
| 22.4–23.3 s | HOME row 2, K4 DEPTH | 44 → 25% |
| 23.6–24.5 s | FX REVERB, K3 STR | 47 → 63% |
| 24.8–25.6 s | Physical MASTER | ~88 → 100% pot position |
| 27.7 s | PLAY/STOP | Release STASIS into HALL |
| 28.8–29.8 s | Physical MASTER | 100% → zero |

Move one control at a time, with a soft start and finish. CAPT, MOTN and LAYER actions are on their actual FX pages; the video does not pretend they are HOME cells.

## Audio and visuals

The Short uses a native render of the original Caravel 0.6.0 DSP, not a hardware recording. The main 30-second audio is the approved V4b performance with constant +6.33 dB export gain. The device is a 3D visualization; LCD images come from the real firmware renderer. Details and timing are reproducible, while a manual turn need not match the digital curve sample-for-sample.

Project source: https://github.com/bonszaimusic/caravel
M-VAVE and FM-1 are trademarks of their owners. Caravel is an independent firmware project.
