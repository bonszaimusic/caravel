# Caravel 1.2.0 (release candidate)

Caravel 1.2.0 builds on 1.1.1 and brings **STASIS 2.0** into the standard build, with the PLAY and BUILD screens that go with it. The full list is in [docs/release-120.md](docs/release-120.md).

- **One source, five PLAY groups.** TEXTURE (GRAIN, STR, MOTN, TONE), REGION (POS, LENGTH, REVERSE, SOURCE) and TANK (TYPE, FEED, DRY, HALL) join SENDS and FILTER. The V7 scope shows the recorded peaks, the real grain heads and the region.
- **DRY per track, HALL and FEED.** DRY 0 leaves only STASIS; FEED and HALL are 0 / 100 / 300 %.
- **Grains 23 / 46 / 93 / 186 ms**, and MOTN scatters them across the REGION window.
- **BUILD FX pages in the PLAY look**, with the BUILD header unchanged.

## Project format: read this before saving

REGION settings that differ from the defaults are saved with the project (marker A6). **1.1.1 and earlier cannot read such a project.** Projects with default REGION settings are unchanged, and 1.2.0 reads every 1.1.1 project. FEED, HALL and the SOURCE state are not saved.

## What was and was not checked

Host tests and the emulator pass locally; a test build ran on one FM-1 without flicker or problems and sounded good. **Not measured:** the CPU load against 1.1.1, and the hosted CI. The A4 test for HQ HOLD to LIVE still fails (a seam when FREEZE is released in HQ; not audible in tests).

---

# Caravel 1.1.0 (older) — 2026-11-21

Caravel 1.1.0 builds on 1.0.0 (FORMAT 2, six engines, BUILD / PLAY). The full list is in [docs/release-110.md](docs/release-110.md); in short:

- **HARM:** a route map of the HARMONY links in BUILD (one colour per role), and in PLAY a chord selector (K2 CHORD), a bars-left countdown (K3 LEFT, up to 16 bars, then HOLD) and a harmony position line under the header. A track that plays from the HARMONY bus or CHORD EXT edits the global HARMONY key from ROOT.
- **Orbit look:** one colour rule (cyan = value of the selected track, orange = global), round arcs, pearl thumbs, ELASTIC drawn as a rubber band.
- **LFO held** makes the FX macros and the STASIS TONE momentary too.
- **LATCH STACK** is not available for chords from the HARMONY bus or CHORD EXT (they latch as NEW).
- **CHORD EXT** takes the keys physically held when the gap ends.
- Also in this release: CAPTURE QUANT (hold REC, K2 QUANT), PLAY SAVE into PROJECT and the other changes of the private candidate (see the commit history).

## Project format: read this before saving

**AFTERBURNER and the STASIS TONE are now saved with the project** (1.0.0 kept them for the session). The project size, offsets and checksum do not change. A project saved with non-default AFTERBURNER or TONE values is **refused by 1.0.0 firmware**; projects with default values are byte-identical to 1.0.0. Back up first, as for every update.

## What was and was not checked

Host tests and the browser emulator pass locally (`tools/docker_validate.sh host all`, including the CHORD EXT fuzzers and the mutation checks). A test build of this tree was installed on an FM-1 and the owner reported the hardware test as successful; no measurements were taken (CPU, RAM headroom 4,800 B on the target build as computed by the build checks). The hosted CI could not run (budget). Not covered: ThreadSanitizer under emulation, two tests that do not build yet (`capture_post_quant`, `stasis_project_menu_ui`, listed as open in `tools/validate_format2.py`), and the old 0.6.0 workflow `action-banks-android.yml`, which no longer matches the current UI.

---

# Caravel 1.0.0 — 2026-11-17 (previous release)

Caravel 1.0.0 includes AFTERBURNER and the corrected HARMONY CHORDS page. The owner approved publication after reviewing the RC. Automated software checks passed; a complete FM-1 audio/load/update/restore acceptance is not documented.

Classic 0.13.0 is frozen and no longer maintained: no 0.13.1 or further fixes. Existing Classic downloads remain available until the owner decides their retention period. Corrections continue only in 1.x.

## Before updating

Back up using the firmware and backup tool that currently read your device. Keep that firmware, its installer and the backup together. 1.0 starts fresh: **pre-1.0 backups are not imported** and there is no automatic migration. A backup preserves the old system's data for use with that old system; it is not a 1.0 restore file.

Projects now use FUNW with the HPM2 v6 performance container; user presets use UPB2 and performance flash records IFP2. The editor protocol is 18, with separate generation-2 framing for every request. Frozen Classic clients cannot edit 1.x or start its software update/recovery entry. Backup LIST 1 and LIST 2 describe different generations; restore must match the connected firmware. Old data is reported as OLD DATA FOUND, with explicit trial/save-copy handling where supported, rather than converted or silently overwritten.

The web installer also supports upgrading the known pre-release 1.0 development firmware with protocol 17. It first validates and downloads a complete FORMAT-2 backup using a read-only connection, then asks for confirmation before updating to the current 1.x firmware. Normal editors remain restricted to protocol 18; unknown revisions and incompatible device layouts are refused. Confirm the two checkboxes in the installer; no typed `UPDATE` word is required. The final confirmation after backup remains.

## What changed

- Six engines: **ANALOG, PHYS, SEM, HADES, DRONE and ENSEMBLE**. SAMPLE, GRAIN, LOFI, FM6, PHASE, VOICE, TRIO, NOISE, WHEEL and DRUM are removed from the shipped profile. Song/Chain and Scope are removed.
- Start in **PLAY**, with a one-time LEVELS introduction. Hold HOME one second for BUILD/PLAY; tap it for the LEVELS explanation or the BUILD track overview. EDIT selects SOUND. SAVE opens PROJECT. BUILD uses the ring pages; STEP, DETAIL and AUTOMATION retain their grids.
- **CAPTURE enabled by default.** It records live panel keys and USB/TRS MIDI IN before harmonic mapping: pitch, velocity, timing and physical Note-Off. REC takes 1/2/4/8/16/32 bars and overdubs; all four loops have independently remembered LENGTH selections. No audio, pedal/CC automation, ARP/SEQ output or recursive recording. Loops are temporary and are not saved/backed up. A loop has at most 128 events per track including releases; FULL refuses an entire take and retains the old loop. A complete FM-1 acceptance is not documented.
- **CHORD EXT:** SOURCE OFF/HARM/T1–T4, EXT 7 / 7+9 / 7+9+13, OCT −2…+2, and read-only actual NOTES. Live sources collect real panel/MIDI Note-Ons with a 41-control-block gap and 138-block cap. They remember the last recognized chord; ambiguous gestures keep it. They never start a silent follower. They exclude generated ARP/SEQ/CAPTURE output, self-sources, EXT-source cycles and PAIR followers. A legato change with only two new tones can be ambiguous because held tones from the preceding chord are not added.
- **RETRIG OFF/HARM/SCALE/BOTH. HARM/BOTH now applies to all eligible tracks**, including SOURCE OFF. This is an intentional behavior change for held harmony-following notes; OFF/SCALE retain their separate differential references. ARP/SEQ consult the context on their next event.
- **FX FILTER is track-local and persistent** (#62 / draft #63): both BUILD FX K1 and PLAY SPACE/FILTER change the selected track's stored `P_FILT`. Its cutoff stays with that track when selection changes or the FX key is released; it is not a momentary global `perf_k[0]` override. CRUSH, THROW and DEPTH retain their independent live-macro behavior. FX + OCT− explicitly clears only the selected track's filter.
- ENV held plus a knob in BUILD suppresses page advance on release. LFO held permits momentary changes; OCT+ accepts them.
- PAIR project loads normalize SPREAD to zero on both member tracks, including projects imported over USB. Bad capsules, unsupported engine/voice data and malformed pairs remain rejected.

## Limits and validation

**AFTERBURNER:** In BUILD, tap FX once, then turn SELECT to AFTER
(after REVERB). Quick repeated FX taps lock its held layer instead.
K1 switches it ON, K2 sets nominal low-frequency RT60 from 10 to 60 seconds,
K3 sets AMT, K4 sets DAMP. It extends the HALL/STASIS wet return and preserves
its own tail across reverb-type changes. It uses no synth voice. It starts OFF;
these controls are currently session-only and reset on INIT or LOAD rather
than being stored in FORMAT 2. There is no adaptive CPU-priority controller in
this release. Test 10/30/60 seconds with four tracks and dense CAPTURE on the
FM-1; if load is excessive, use the matching build with
`CARAVEL_AFTERBURNER=0`, which removes its DSP, buffers and AFTER page.

The compressed performance capsule still has a **3,328-byte budget**. PROJECT TOO LARGE / NOT SAVED refuses a save and retains the previous copy. The pinned engine submodule is required for a reproducible source checkout. m-bootloader remains documentation-only. UC25HQ80 (`0xB36014`) is recognized but **not tested on a real unit by this project**.

Software validation on the executable-equivalent RC passed: all 8 FORMAT-2 groups (327 steps including compilation), 846 ASan/UBSan fuzz cases, all 11 negative mutation controls and three positive controls. All three target layouts passed with AFTER enabled and disabled. The shipped emulator was separately checked with AFTER ON at 60 seconds and four CAPTURE loops of 120 events each. The release rebuild compares firmware and WASM bytes with this tested RC; documentation and deployment changes receive targeted checks.

Standard target: app 466,392 bytes, static BSS 68,224 / 73,728 bytes, pool 353,180 / 368,640 bytes. These are static budgets, not measured device deadlines. Host timings do not establish FM-1 cycles, MIDI jitter, audio quality or power-cycle safety. No GitHub-hosted firmware CI or device listening pass is claimed.

## Hardware acceptance checklist

Record the installed package SHA-256, device/flash identity and each result. Stop and report a failure with the exact gesture.

1. Back up 0.13.0 or Felucca, install 1.0.0 using the matching installer and verify INFO shows v1.0.0. Test both upgrade origins on suitable hardware; do not erase the only backup.
2. On a fresh settings image, verify PLAY and LEVELS appear once. Fully power off/on; LEVELS must not repeat. Check an already-seen image as well.
3. Inspect BUILD header/strip for flicker, SAVE/PROJECT, MIXER, GLO and menu. Check the retained STEP/DETAIL/AUTOMATION grids.
4. Set different stored filters on two tracks. Verify BUILD FX K1 and PLAY SPACE/FILTER both modify only the selected track's persistent `P_FILT`; track switching, unlatched FX release and PLAY/BUILD transitions do not move or reset the cutoff. FX + OCT− clears only the current track. Test project save/load and listen for unintended jumps.
5. Hold ENV, turn a knob and release without page advance. Try LFO momentary, release to return, then OCT+ to accept.
6. Try HARM and each T1–T4 source from the panel, USB and TRS, with simultaneous and rolled chords, inversions, release before recognition, sustain, ambiguous dyads and legato. Test WAIT, source changes, retrigger, STOP/panic and no stuck follower notes. Judge whether the collection delay is usable.
7. For CAPTURE, exercise all four loops with dense live panel and MIDI chords, repeated notes, sustain, REC/overdub/full refusal, pause/resume, clock START/STOP/loss, track changes and PAIR. Listen for missing notes or releases. Measure block/REC maxima with the console CAPTURE counters under the intended device load; a host timing ratio is insufficient.
8. Listen to DRONE gain and layered levels, with dry/wet comparisons. Test the second-core option separately if enabled.
9. Save/load projects, stop to commit a live save, power cycle, test autosave and a complete FORMAT 2 backup/restore with the matching tool. Confirm an old-format restore is refused. Confirm a malformed import leaves saved data unchanged.

The published package includes checksums, the full source and component licences. Classic remains frozen, with its old download URLs preserved.

Loader-Fortsetzung: Web-Installer und CLI starten den erkannten Update-Loader mit dem passenden Einstieg für Protokoll 18 sowie dem kompatiblen Classic-Einstieg. Die Firmware-Sperre für alte Editor-/Updatezugriffe bleibt erhalten. Ein schwarzer Bildschirm im Loader ist keine laufende PLAY-/BUILD-Oberfläche; der Installer muss den Transfer und anschließenden Neustart bestätigen.
