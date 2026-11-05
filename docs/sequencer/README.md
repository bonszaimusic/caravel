# Sequencer and recording guides

User-facing workflows, controls, compatibility and limits:

- [Live parts and step recording](LIVE_PATTERNS.md): A–D snapshots, STEP/LIVE
  recording and queued PATTERN/FULL loads.
- [FX-special automation](FX_SPECIAL_AUTOMATION.md): record, edit and delete
  press/release events on the designated FX track.

- [Project INIT and the global FX track](project-init.md).
- [DOUBLE](double.md): duplicate a track sequence with locks, automation and FX edges.
- [External MIDI clock](external-clock.md): resync, quantized start and EXT START with USB / TRS clock.

Host and emulator tests do not establish physical FM-1 acceptance.

## Coupling the four tracks (development build)

Two performance shortcuts sit in the existing SET layers; neither changes the project format.

- **SCL + OCT+ (let go)** copies the selected track's `ROOT`, `SCL` and `QNT` to all four tracks. `TRN`, `CHRD` and
  `VOIC` stay each track's own, so the tracks can still be transposed and voiced apart. The top bar says `SCALE TO ALL`.
- **GLO + OCT+ (let go)** arms **BAR RESET**. Playing, every track goes to its step 0 at the next bar line; the
  transport keeps running (no STOP/PLAY flam). Stopped, it takes the beat display back at once; the next PLAY starts at
  the top anyway. The message is `RESET NEXT BAR` or `BAR RESET`. In a SONG it says `STOP SONG FIRST`.

Both work on the internal clock and on an external MIDI clock; the bar reset uses the same downbeat path as the
external-clock resync (`src/seq.c`, `seq_resync`). Host-tested in `tests/ui_test.c` (`SCL + OCT+`, `GLO + OCT+`), not
yet tried on an FM-1.

### Generative layer (development build)

- **GLO + G4** — **HARMONIC CLOCK** on/off. A four-chord loop, one chord per bar, from the selected track's
  `ROOT`/`SCL` (major: I–V–vi–IV; minor: i–VI–III–VII). It is volatile (not saved) and feeds the new generated
  patterns, not the older ARP/sequencer sources.
- **GLO + G5** — **KEY LINK** on/off. All four tracks follow the source track's `ROOT`, `SCL` and `QNT`; the source is
  the selected track when the link goes on. `TRN`, `CHRD` and `VOIC` stay each track's own. Selecting another track does
  not move the source; turn the link off and on again to make the new track the master. Saved with the settings.
- **SEQ TOOLS: G5** — **GENERATE**: a scale-aware phrase from a short motif, rests between the strong steps, and chord
  tones on the strong steps while the HARMONIC CLOCK is on. One undo; `SAVE` held puts the pattern back.
- **ARP MODE TURING** — a 16-bit feedback register walks the held-note list, with an occasional random bit flip, so
  the line moves without locking onto one note. It uses the existing ARP controls; no new parameter.
