# Live parts and step recording

This guide describes live A–D snapshots, STEP/LIVE recording and queued project
launches.

## Create four parts without stopping

While playing, open SAVE > PROJECT. Select A-D with KNOB 1, pick SAVE with
KNOB 4 and press OCT+. An occupied slot asks before overwriting. Name the part
or keep its name, then press OCT+ again. Playback and recording keep running.
The display briefly says RAM ONLY, followed by STOP TO SAVE. The saved slot
shows RAM: before its name until it is committed to flash.

Each slot holds its own snapshot, including steps, timing, sounds, automation
and parameter locks. A-D are immediately available to the live LOAD below;
live LOAD retains the current sounds in PATTERN mode; FULL restores them. Saving B, C or D does not
replace A. Saving a slot again replaces only that slot's snapshot.

STOP automatically commits the pending snapshots, one per main-loop pass;
PARTS SAVED means all are durable. Restarting playback pauses any remaining
commits. A flash error keeps the RAM snapshot and previous durable copy, shows
RAM: SAVE ERROR and retries after one second while stopped. Uncommitted parts
are lost on power-off or reboot. User presets and renames still require STOP;
active SONG playback/armed SONG must be stopped before saving project snapshots.


## Step recording while playback continues

On a melodic track's SEQ STEP page, hold SEQ and tap REC to select and arm
STEP REC. The footer shows STEP REC. Playback continues independently of the
input cursor. KNOB 1 or PRESETS selects a step; the panel keys write there.
In the default HOLD chord-entry mode, releasing every key advances one step,
wrapping inside the pattern length. With MENU > CONTROL > CHORD ENTRY ADD,
one-finger chords accumulate at the same step until you move the cursor.

Hold SEQ and tap REC again to switch to LIVE REC at the moving playhead.
Release all keys before switching modes. A normal REC tap disarms recording.
STEP REC remains selected across STOP/PLAY; it is transient per track, not a
saved project setting. A queued project launch disarms both recording paths.
The normal REC gesture retains its existing stopped-step/live-play behavior.
Active SONG and the drum grid cannot select this mode; open a melodic STEP
page first. STEP REC accepts the panel keys, not external MIDI step entry;
external MIDI can still sound, and other armed tracks can still record live.
Knob moves do not record playhead automation on a STEP REC track.

Save the edited parts separately to A-D while playing as above. They remain
RAM ONLY until STOP commits them automatically.

## Switch parts live

While playing, open SAVE > PROJECT. Select A-D with KNOB 1, select LOAD
with KNOB 3, then press OCT+. QUEUED A-D replaces any earlier valid request.
At track 1's loop boundary all four editable tracks restart together,
including saved step data, timing, automation and parameter locks.
With MENU > AUDIO > LIVE LOAD PATTERN (the default), current sounds, global tempo, effects and clock source stay in place;
automation still changes the parameters it controls. An active recording ends
at launch. Stop cancels a pending launch. Stopped LOAD still loads the full project.
In PATTERN mode all four destination engines must match the current engines. SONG cannot run
alongside this launcher because both reuse the existing pattern cache.

### FULL live loading

Set **MENU > AUDIO > LIVE LOAD > FULL**, then use the same live LOAD gesture.
The mode is a device preference, preserved across power cycles and exposed as menu id 33
in the matching web editor. It is captured when queuing a request: changing the preference
does not change a request already queued. PATTERN remains the default for older settings.

FULL launches the queued snapshot's track parameters and engines, FM6 patches, DRUM-X
A/B kit data, FX sends, inserts, slicer, global delay/chorus/reverb and tuning, together
with its patterns, timing, automation and FX-track ownership. A RAM-only save has the same
contents as a durable slot. Saving over that slot after queuing does not change the launch.
The existing clock source, tempo, global swing, MIDI routing and channel mapping remain.
Recording ends at launch; STOP cancels pending loads. SONG's behavior is unchanged.

Active voices fade through the existing 128-sample engine-switch path (about
2.9 ms); new notes wait for that fade. FX tails follow their existing model/type
changes, so an old reverb tail is not guaranteed to survive a model change.
Hardware listening and deadline checks remain necessary. Live FX latches and
FILTER/CRUSH/THROW/DEPTH macros remain live and are neither captured nor recalled
by FULL. This does not extend the saved project format.

The web editor's Settings > LIVE LOAD changes the same preference; its existing
project-load command remains a complete stopped project load. Test the live launcher
with the device panel or the matching browser emulator's panel.

## Validation

Live-launch and step-recording checks, the UI and persistence suites (including four separate
snapshots, retry, failed-write rollback and interrupted commits), the USB/TRS
MIDI integration suite and the WebAssembly emulator suite pass locally.
The emulator tests STEP/LIVE REC mode switching, cursor chords with independent
playback, SAVE and LOAD during playback, automatic flash commits
after STOP and all four slots in a fresh instance. The target application
passes the RAM/image checks and unchanged audio-ISR cost budget; the separate
boundary-launch helper also has an explicit budget. Hardware testing remains
outstanding.


## Related guide

See [FX-special automation](FX_SPECIAL_AUTOMATION.md) for recording, editing and
clearing the FX layer's special-effect events.
