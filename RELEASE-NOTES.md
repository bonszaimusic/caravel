# Caravel 0.7.0

## Audio and performance

Hot DRONE, SEM, HADES, STASIS and HALL DSP now runs from checked executable RAM. STASIS joins/capture and reverse LIVE handling are smoother; redundant DRONE work and sample-loop calls are reduced without changing DSP reference output. Newly started voices are included before dual-core work is split. The owner reports no audible crackle and indicated maximum load around 70% with the rc6 setup. This is a listening/display report, not a controlled benchmark or a guarantee for every patch.

## Controls and automation

CHORUS precedes REVERB; STASIS GRAIN and LAYER share one view. Random Morph RETURN keeps independent origins per track/HOME row. HOME macro, PAIR MORPH and PAIR MOTION recordings are now visible under the owning track's SEQ > AUTOMATION. PLAY OFF pauses them and restores their baseline. Pair recordings belong to member A: A:T3 B:T1 means look on T3. Events can be edited, changed between AUTO/LOCK, deleted and undone without evicting other tracks. Existing stored PLAY OFF now pauses previously hidden performance events too. The new automation UI still needs confirmation on the physical device.

## Snapshots

S1–S4 Random Morph snapshots are still volatile RAM. Saving a project retains its current sound, not those four slots or captured STASIS audio. Permanent snapshot storage is explicitly deferred to a later update.

## Install and verify

Back up the FM-1 first. Download `Caravel-0.7.0-FM1-Testpaket.zip` for the local installer and firmware; the matching offline emulator is `Caravel-0.7.0-Emulator.html`. The owner's existing Android installer is updated separately. The standalone firmware is `caravel-0.7.0.fwsc`; after reboot INFO should show `v0.7.0`. [Detailed German instructions](https://github.com/bonszaimusic/caravel/blob/v0.7.0/docs/prerelease-070.md). Older releases remain available as rollbacks.

The hardware profile still contains HALL/STASIS and omits ROOM, SPRING and the SLICE engine. The SLICER effect remains. Old projects keep compatibility prompts and source-slot protection. Ordinary tempo DELAY is suspended while STASIS owns the shared buffer.

The package includes verified source, build record, notices/licences and SHA256SUMS. Publication requires exact target, ABI, DSP/state/continuity, ARP, MIDI/UI/storage, HOME/Pair, native/WASM and complete validation gates. Hardware worst-case load and long-term stability remain practical device checks.
