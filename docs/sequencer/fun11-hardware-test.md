# FUN11 / Harmony / Link hardware smoke test

Status: prepared for the FM-1; not yet run on hardware.

## Build under test

- `build/caravel-0.11.0.fwsc` (FUN11 build, identity `FM-1_999`)
- Keep a **FUN10** project (any 0.11.0 slot) and a backup of the current flash before flashing.

## 1. Project format

1. Flash the build, power on, open `SAVE > PROJECT`.
2. Load an existing **FUN10** slot. It must load, and its 0.11 HARMONY roots/on/off (if any) must appear on `SCL > HARMONY`.
3. Change: four HARMONY roots, four durations, bus on/off, per-track LINK source/retrigger.
4. Save to another slot; power-cycle; load that slot.
5. Check every value after the reload. The slot must be `FUN11`; older firmware will show it `EMPTY` (expected).
6. Save a new project in a slot that held FUN10 and confirm the name, sound and song chain survive.

## 2. SCL two-group pages

1. `SCL` shows two rows of four cells: `SCL` above, `CHORD` below. The SCL button and **SELECT** both step the pages SCL -> CHORD -> LINK -> HARMONY, as ENV/ENV DEST.
2. On `CHORD`, its knobs edit `CHRD / VOIC / LATCH`; on `SCL`, `ROOT / SCL / QNT / TRN`.
3. Next opens **LINK / HARMONY SETUP**: upper row `K1 SOURCE`, `K2 RETRIG`, `K3 KEY`, `K4 HARMONY`; on this page the first **SCL** press switches to the lower HARMONY SETUP row, whose `K1` is the bus ON/OFF; the next SCL press steps on. No linked source may be offered.
4. With HARMONY ON, `SELECT` opens the HARMONY page: top row chord names, bottom row durations; the first **SCL** press switches roots to durations, the next steps on. The bus switch is on LINK / HARMONY SETUP, not this page.
5. `OCT-/OCT+` are ordinary octave keys everywhere; they do not switch rows or the bus. Turn a knob after every page/row change; the value and the card must change. Repeat the SELECT page turns and SCL row taps 20 times and from HOME.

## 3. Clock

1. Internal clock, `PLAY`: the HARMONY bar advances with the sequencer; `STOP` holds it; `PLAY` restarts at BAR 1.
2. `GLO + OCT+`: BAR RESET returns HARMONY to BAR 1 at the next bar.
3. External MIDI clock: send Start; HARMONY must start at BAR 1 and follow 24 PPQN bars. Stop must hold; Continue must resume.
4. Journey `CONT` / `START`: HARMONY uses the same bars as the journey. Journey `FREE`: HARMONY holds (no musical bars).
5. Change tempo or external clock rate while running: no jump in the chord, just the new bar length.

## 4. Harmony / Link sound

1. `CHRD BUS` + `HARMONY ON`: a held key plays the current bus chord; on a bar change with `RETRIG CHORD` it retriggers.
2. Play a chord with `LATCH` on and `RETRIG CHORD`: a harmony chord change retriggers the latched notes too.
3. T2 `SOURCE T1`, `RETRIG LINK`: change T1's ROOT/SCL; T2's held chord retriggers.
3. Sequencer and ARP strong steps take chord tones; weak steps stay scale notes.
4. `SEQ TOOLS > G5` GENERATE: strong steps take chord tones while HARMONY is on.
5. `SCL + OCT+` on SCL/CHORD: one-shot copy still works. On visible HARMONY it does nothing; on HOME it still copies the SCL of the selected track even if HARMONY was the hidden last page. `GLO + G4/G5` must do **nothing** (removed setup shortcuts).

## 5. Regressions

- `SAVE` held undo; project load while playing; song rows; user presets; FM6 patches.
- `PLAY` / `STOP` / `REC`; SCL layer keys and LEDs; HOME action banks (RANDOM/JOURNEY/STASIS/PAIR/LIVE).
- RAM guard and audio: no dropouts with four tracks and the heaviest song.

## Known limits

- FUN11 projects are not readable by 0.11.0; migrate on the new firmware.
- HARMONY is a four-chord loop, not a per-section progression.
- `RETRIG` currently retriggers a held `CHRD BUS` chord; sequencer/ARP steps retrigger naturally.
