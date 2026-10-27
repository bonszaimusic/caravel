# STASIS V0.6 — stable-pitch, evolving mystical hall textures

Technical reference for Caravel 0.6.0, developed on `experiment/stasis-texture-v0`.
The public package and emulator use HALL/STASIS with SLICE omitted. The SLICER
track insert remains. The complete automated suites gate publication; actual
FM-1 worst-case load and long-term hardware stability remain separate checks.

## Design change prompted by listening

The pitch sweeps in V0.3–V0.5 were not a useful musical motion. V0.6 replaces moving read speed with **unity-speed, overlap-crossfaded grains**. Grain taps move **only when their crossfade weight is zero**. MOTN couples *which delay reflection is chosen next*, opposing stereo low-pass sweeps, and gentle HALL damping variation.

The existing HALL FDN produces the long wash; no separate HALL or STASIS menu is needed. Room, Spring, and abandoned Abyss remain absent from `CARAVEL_REVERB_MINIMAL=1`.

## Single FX → REVERB page

| Reverb type | Knob 1 | Knob 2 | Knob 3 | Knob 4 |
|---|---|---|---|---|
| HALL | TYPE | SIZE | DAMP | — |
| STASIS | TYPE | CAPT | STR | MOTN |

- **CAPT LIVE/HOLD**: LIVE records the internal REV send. HOLD freezes the most recently recorded sound after a safe prefill. No separate FRZ page or button.
- **STR 0–127**: foreground texture level; 0 leaves essentially the sustained reverb, high values make captured grains prominent.
- **MOTN 0–127**: combined music-oriented macro. Controls the density and stereo movement of *new* delay tap selections and filter and reverb damping modulation. No pitch shifting or reverse-only knob; original pitch is approximately maintained.
- **HALL** retains its original SIZE/DAMP controls with their prior stored IDs.

**STOP behavior:** Pressing PLAY/STOP when the transport is running stops normally and additionally releases STASIS HOLD; if transport is already stopped but CAPT is HOLD, pressing PLAY/STOP releases STASIS instead of starting the sequencer. Captured grains fade out while the HALL feedback continues its natural tail. The next PLAY resumes fresh recording. MIDI STOP follows the same seq_stop path.

## Audition

1. Set a SEM pad or FM chord track REV SEND around 110–127, select REVERB TYPE STASIS.
2. Start with CAPT LIVE, STR 95–110, MOTN 35–55. Play for at least ~0.7 s.
3. Change CAPT to HOLD. A captured chord should keep its pitch and develop slowly through filter, delay reflections and HALL.
4. Increase MOTN to 100–127. Listen for variation and stereo shifts, **not an up/down pitch slide**.
5. Hit STOP. CAPT should visibly return to LIVE, the granular cloud should fade, and the HALL tail should finish naturally. Press PLAY to capture new material.

Ordinary tempo DELAY is disabled while STASIS uses the shared 128-KiB ring. The input is Caravel's **internal REV send**, not external microphone/USB audio.

## ADD and BLEND held textures

Keep **REVERB > CAPT HOLD**, then open **FX > LAYER**. The four knobs show
**MODE / AMT / TIME / ADD or BLEND**. Turning knobs only selects settings;
**OCT+ explicitly starts** the operation. The action is selected on page entry.

1. Play the new sound through its track's **REV send** for at least about 0.6 s.
   Other tracks with a REV send also contribute; mute or lower their sends if needed.
2. Select **ADD** and an **AMT** (default 25%). Press OCT+. A 200-ms crossfade
   incorporates that proportion of the newly captured sound into the existing
   texture. Complementary old/new gains prevent cumulative buffer overload.
3. Alternatively choose **BLEND** and **TIME 1–30 s** (default 6 s). Press OCT+.
   The held texture transitions to a snapshot of the current sound over that time;
   AMT is ignored. It then holds the new texture. This is one transition per press.
4. **WAIT** means no sufficiently recent input is available. The existing texture
   remains unchanged; play a sound or press OCT+ to cancel. During **MIX**, OCT+
   returns smoothly to the original over 200 ms. **STORE** completes the mixture
   without a discontinuity in about 0.75 s; wait for it before another operation.

Both operations finish with CAPT still **HOLD**. STOP and HOLD-to-LIVE apply their
usual release to the audible mixture. The capture is volatile audio: project saves
retain MODE/AMT/TIME, not recorded textures, and there is no per-layer undo.
AMT zero performs no ADD. MODE/AMT/TIME reuse global IDs 13/15/21; older projects
have zero in those retired fields, so set AMT and TIME before their first overdub.

Two 32,768-sample mono capture banks (0.743 s each at 44.1 kHz) share the existing
128-KiB allocation. A background bank collects recent live REV input while HOLD
plays. It is frozen before mixing; neither audible source is rewritten during the
fade. An incremental commit uses an equivalent per-address read rule, preserving
the mixture exactly while freeing the background bank for the next operation.
The extra management state is 60 bytes. A second HALL network is not allocated.

`tests/stasis_layer_test.c` runs the real DSP and panel in hardware/browser storage
profiles, checks exact mixture/commit identity, silence and cancellation, repeated
hot-input ADD, timed BLEND, transport release and project-setting round trips.

## Loading projects made with omitted features

The reduced target build (`FELUCCA_SLICE=0`, `CARAVEL_REVERB_MINIMAL=1`)
warns before loading a project that needs SLICE, ROOM or SPRING. The browser
build retains SLICE and only warns for its omitted reverbs. The existing panel
dialog shows the actual substitutions: SLICE > ANALOG, ROOM/SPRING > HALL.
OCT- **CANCEL** leaves the working project untouched; OCT+ **TEST** loads an
explicitly converted working copy. The original stored parameters are retained.

The source slot is protected against project saves, erase and USB replacement
for the rest of this session. Save the audition to another slot (an occupied
destination still has the normal overwrite confirmation). Autosave is held from
the warning until an explicit copy is saved, a compatible project is loaded, or
the working project is initialized. Cancelling a slot load restores the previous
session's autosave policy. Cancelling RESTORE LAST keeps autosave held, so the
incompatible last session is not silently replaced. Explicitly queued slot saves
can still finish while autosave is held.

RESTORE LAST asks the same question. Because an autosave has no reliable source
slot identity, its test load protects all occupied slots: save to an empty slot.
If none is free, export a backup and arrange a free slot before auditioning.
Full project launches during playback require stopping first for the dialog.
Direct USB/current-session restores of incompatible material are refused; import
the saved project into a slot and use its panel LOAD for the confirmation.
No new audio or decoded-project buffers are allocated; the confirmation re-reads
its source and permission expires after that attempt.

`tests/project_compat_test.c` exercises the real panel buttons, both project
storage backends, original bytes, copy saves, autosave holds, RESTORE LAST,
unconfirmed direct restore refusal, and exact reverb/engine warning conditions.

## Verification

`tests/stasis_test.c` exercises capture/hold, fixed-pitch tap boundaries on MOTN knob changes, hall tail, STR dynamics, bounded sample discontinuity, the one-page layout and HALL fallback for old ROOM/SPRING projects, and STOP→release→fade→PLAY. The independent browser CI builds a WebAssembly preview only after the host tests succeed.

**Not yet validated for release:** subjective sound quality, actual FM-1 worst-case CPU/stability, dense four-track audio glitches, legacy GM drum-global migration and complete web editor regression. Target compilation, static Flash/RAM/POOL limits and D3 host checks passed for the preceding test build; each new firmware build repeats them. Do not flash emulator assets.

