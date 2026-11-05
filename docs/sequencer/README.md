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

- **SCL > LINK** — per track. `K1 SOURCE` = `OFF` / `T1`..`T4`; the follower copies the source's `ROOT`, `SCL` and
  `QNT`. `TRN`, `CHRD` and `VOIC` stay its own. A linked track is not offered as a source (no chains). `K2 RETRIG`:
  `OFF`, `CHORD` (a held `CHRD BUS` chord retriggers when the harmony chord changes), `LINK` (when the source's key
  changes) or `BOTH`. `K4 HARM` is this track's **HARMONY FOLLOW**: `OFF` means the track ignores the bus (its
  sequencer/ARP strong steps stay scale notes, `CHRD BUS` falls back to `DIA3`, and GENERATE adds no chord tones);
  the other tracks keep following. Saved with the project (FUN11).
- **SCL / CHORD** are one two-group page, like ENV / ENV DEST: **two rows of four cells** on screen. The upper row is
  the SCL page (ROOT SCL QNT TRN), the lower one CHORD (CHRD VOIC LATCH); the page you tapped is the row K1..K4 edit,
  the other is a view. **HARMONY** is the same two-row layout: the upper row is `BAR 1..4` (the chord names), the
  lower row the four durations (`1/2/4/8` bars); `OCT-` switches K1..K4 between the roots and the durations, `OCT+`
  turns the bus on/off, and the header shows `ON`/`OFF`. The roots, durations and on/off are saved with the project
  (FUN11).
- **Clock:** the bus counts the transport's musical bars, not an audio beat: the internal BPM clock or the external
  MIDI clock, whichever the sequencer uses. `PLAY` starts at BAR 1; `STOP` holds the chord; `CONT`/external Continue
  goes on; `GLO + OCT+` (BAR RESET) returns to BAR 1. Journey `CONT`/`START` shares the same bars; Journey `FREE` has
  no musical bars, so the bus holds. A song start is BAR 1; sections continue the progression.
- **Where it acts:** strong steps (every fourth step) of the sequencer, the ARP, and `SEQ TOOLS > G5` GENERATE take a
  chord tone. Live keys and the DRUM lanes stay the player's / the kit's — unless you set `SCL > CHORD > K1 CHRD` to
  **BUS**: then a key plays the current bus chord, at the octave nearest the key (`CHRD BUS` falls back to `DIA3`
  while the bus is off). A held bus chord follows `RETRIG` above; otherwise it ends with the key.
- **No hidden setup keys:** the old `GLO + G4` / `GLO + G5` shortcuts are gone. Harmony and Link live on their SCL
  pages; the GLO map stays performance only.
- **SEQ TOOLS: G5** — **GENERATE**: a scale-aware phrase from a short motif, rests between the strong steps, and chord
  tones on the strong steps while the HARMONIC CLOCK is on. One undo; `SAVE` held puts the pattern back.
- **ARP MODE TURING** — a 16-bit feedback register walks the held-note list, with an occasional random bit flip, so
  the line moves without locking onto one note. It uses the existing ARP controls; no new parameter.
