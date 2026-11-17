# Caravel 1.0.0

Prepared for validation; **not published, not installed by this task, not tested on an FM-1. No sound was heard.** The live website and latest published release remain 0.13.0 until a separately approved publication.

Classic 0.13.0 is frozen and no longer maintained: no 0.13.1 or further fixes. Existing Classic downloads remain available until the owner decides their retention period. Corrections continue only in 1.x.

## Before updating

Back up using the firmware and backup tool that currently read your device. Keep that firmware, its installer and the backup together. 1.0 starts fresh: **pre-1.0 backups are not imported** and there is no automatic migration. A backup preserves the old system's data for use with that old system; it is not a 1.0 restore file.

Projects now use FUNW with the HPM2 v6 performance container; user presets use UPB2 and performance flash records IFP2. The editor protocol is 18, with separate generation-2 framing for every request. Frozen Classic clients cannot edit 1.x or start its software update/recovery entry. Backup LIST 1 and LIST 2 describe different generations; restore must match the connected firmware. Old data is reported as OLD DATA FOUND, with explicit trial/save-copy handling where supported, rather than converted or silently overwritten.

## What changed

- Six engines: **ANALOG, PHYS, SEM, HADES, DRONE and ENSEMBLE**. SAMPLE, GRAIN, LOFI, FM6, PHASE, VOICE, TRIO, NOISE, WHEEL and DRUM are removed from the shipped profile. Song/Chain and Scope are removed.
- Start in **PLAY**, with a one-time LEVELS introduction. Hold HOME one second for BUILD/PLAY; tap it for the LEVELS explanation or the BUILD track overview. EDIT selects SOUND. SAVE opens PROJECT. BUILD uses the ring pages; STEP, DETAIL and AUTOMATION retain their grids.
- **CAPTURE enabled by default.** It records live panel keys and USB/TRS MIDI IN before harmonic mapping: pitch, velocity, timing and physical Note-Off. REC takes 1/2/4/8 bars and overdubs; four loops are independent. No audio, pedal/CC automation, ARP/SEQ output or recursive recording. Loops are temporary and are not saved/backed up. A loop has at most 64 events including releases; FULL refuses an entire take and retains the old loop.
- **CHORD EXT:** SOURCE OFF/HARM/T1–T4, EXT 7 / 7+9 / 7+9+13, OCT −2…+2, and read-only actual NOTES. Live sources collect real panel/MIDI Note-Ons with a 41-control-block gap and 138-block cap. They remember the last recognized chord; ambiguous gestures keep it. They never start a silent follower. They exclude generated ARP/SEQ/CAPTURE output, self-sources, EXT-source cycles and PAIR followers. A legato change with only two new tones can be ambiguous because held tones from the preceding chord are not added.
- **RETRIG OFF/HARM/SCALE/BOTH. HARM/BOTH now applies to all eligible tracks**, including SOURCE OFF. This is an intentional behavior change for held harmony-following notes; OFF/SCALE retain their separate differential references. ARP/SEQ consult the context on their next event.
- FILTER on the FX layer and PLAY arcs shows the effective selected-track value. Dim means the stored filter, bright means a macro intervention. First turn picks up the track value. Unlatched FX HOLD returns FILTER on release; ordinary PLAY arcs stay. The macro is never written to the stored track filter.
- ENV held plus a knob in BUILD suppresses page advance on release. LFO held permits momentary changes; OCT+ accepts them.
- PAIR project loads normalize SPREAD to zero on both member tracks, including projects imported over USB. Bad capsules, unsupported engine/voice data and malformed pairs remain rejected.

## Limits and validation

The compressed performance capsule still has a **3,328-byte budget**. PROJECT TOO LARGE / NOT SAVED refuses a save and retains the previous copy. The pinned engine submodule is required for a reproducible source checkout. m-bootloader remains documentation-only. UC25HQ80 (`0xB36014`) is recognized but **not tested on a real unit by this project**.

Host and emulator checks are being finalized for the exact candidate source; their receipt and package SHA-256 belong with the final test package. Host instruction counts are not FM-1 cycles. Device audio deadlines, arrival jitter, clock behavior, UI feel, power cycling, the DRONE level and dense CAPTURE playback still require the checklist below. GitHub-hosted CI is not available in this preparation; Docker results must be named separately, without claiming hosted CI passed.

## Hardware acceptance checklist

Record the installed package SHA-256, device/flash identity and each result. Stop and report a failure with the exact gesture.

1. Back up 0.13.0 or Felucca, install the candidate using the matching installer and verify INFO shows v1.0.0. Test both upgrade origins on suitable hardware; do not erase the only backup.
2. On a fresh settings image, verify PLAY and LEVELS appear once. Fully power off/on; LEVELS must not repeat. Check an already-seen image as well.
3. Inspect BUILD header/strip for flicker, SAVE/PROJECT, MIXER, GLO and menu. Check the retained STEP/DETAIL/AUTOMATION grids.
4. Set different stored filters on two tracks. Compare dim/bright FX values, first-turn pickup, return to the anchor, unlatched release, LATCH and ordinary PLAY persistence. Listen for unintended jumps and verify only the selected track is affected.
5. Hold ENV, turn a knob and release without page advance. Try LFO momentary, release to return, then OCT+ to accept.
6. Try HARM and each T1–T4 source from the panel, USB and TRS, with simultaneous and rolled chords, inversions, release before recognition, sustain, ambiguous dyads and legato. Test WAIT, source changes, retrigger, STOP/panic and no stuck follower notes. Judge whether the collection delay is usable.
7. For CAPTURE, exercise all four loops with dense live panel and MIDI chords, repeated notes, sustain, REC/overdub/full refusal, pause/resume, clock START/STOP/loss, track changes and PAIR. Listen for missing notes or releases. Measure block/REC maxima with the console CAPTURE counters under the intended device load; a host timing ratio is insufficient.
8. Listen to DRONE gain and layered levels, with dry/wet comparisons. Test the second-core option separately if enabled.
9. Save/load projects, stop to commit a live save, power cycle, test autosave and a complete FORMAT 2 backup/restore with the matching tool. Confirm an old-format restore is refused. Confirm a malformed import leaves saved data unchanged.

Public push, tag, release assets and live-site updates need separate owner approval after review of the exact package and these results.
