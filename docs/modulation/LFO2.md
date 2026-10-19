# Independent second LFO

Every track now has two independent control-rate LFOs. Both are shared by the
voices of their track. This adds modulation to every engine, including SEM and
HADES; no new synthesis engine or voices are needed.

**Menu access:** press the **LFO** button repeatedly to reach **LFO2**.

## Shared LFO controls

Press LFO to cycle through LFO, LFO SYNC, LFO DEST, LFO2, LFO2 SYNC and MOD.
The first LFO's settings and direct destinations stay together, followed by
LFO2's settings and the shared MOD routing page. LFO2 has no separate DEST
page: its destinations and amounts are configured in MOD.
The former page called **LFO 2** is now **LFO SYNC**: it still configures the
first LFO, and its stored parameter IDs have not changed.

LFO2 provides RATE, WAVE (SIN/TRI/SAW/SQR/S&H), PHS and FADE. LFO2 SYNC provides
SYNC, TRIG (NOTE/FREE) and POL (BI/UNI). SYNC overrides RATE. NOTE restarts the
phase at the start of a fresh phrase, matching the first LFO. FREE+SYNC aligns
to transport start. FADE restarts on a fresh phrase in either trigger mode.

Route the second LFO through the existing four MOD slots:

| Source | Meaning |
| --- | --- |
| LFO2 | The second oscillator, with its own fade and polarity |
| S&H2 | Random value held for a cycle of LFO2 |
| SLEW2 | Smooth transition between LFO2's previous and current random values |

MOD selection lists are grouped by function, independently of their stored IDs:

- Sources: OFF; LFO, S&H, SLEW; LFO2, S&H2, SLEW2; ENV, VEL, KEY, RAND; MODW, AT, EXPR.
- Destinations: OFF; PITCH, CUT, SHP, AMP, PAN, MORPH, PHS FB; RATE, VIB, DEPTH;
  E1–E8 (shown with engine-specific labels); VEL, TIME; DIST, CHO, DLY, REV.

The device knobs, browser emulator and web editor use the same selection order.
Saved routes, automation and MIDI values keep their original numeric meanings.

The amounts on MOD control the depth. The existing RATE, VIB and DEPTH matrix
destinations continue to control **LFO1**. The matrix still has four slots.
LFO2 uses its own RNG for each track and never consumes an engine's random
sequence. Its default rate is the slowest available rate (RATE 0); all routes
are off in existing presets and projects.

For a slowly moving pad, use LFO1 on CUT at a low depth, then a MOD slot
LFO2 -> SHP (or an engine-specific destination) with a different slow rate and
small amount. A second slot SLEW2 -> PAN adds gradual movement. Availability
and audible effect of SHP depend on the selected synthesis engine.

## Persistence and compatibility

- Common parameters remain at 113, and every engine, MIDI Learn, lock and
  performance-FX ID retains its meaning. Sources 11/12/13 append LFO2/S&H2/SLEW2.
- Seven additional learnable controls use virtual IDs 400..406. These control
  settings are not sequencer automation or lock targets, as with DRUM-X's
  virtual controls. Existing matrix routing/amount controls retain their normal
  automation behavior.
- Project snapshots, autosave, stopped project loads, FULL live loads, user
  presets, factory reset and sound-load undo include LFO2. PATTERN live loads
  and SONG keep the current sound and its LFO2 settings.
- Projects with LFO2 settings or routing use FUNC. Its length remains 3840
  bytes and the full 128-event motion capacity is retained. Seven bytes per
  track are packed into the previously unused high bits of its 128-byte
  7-bit sound payload. The original FM6 or DRUM-X payload is unchanged in
  the low seven bits. Unused extension bits and out-of-range settings are
  rejected. Older FUN1..FUNB files import with LFO2 defaults. Projects without
  LFO2 still save as FUNA/FUNB. Firmware without FUNC support refuses FUNC.
- Presets use spare packed bytes 128..134 and versions 8/9 (sound/grid) or
  10/11 (DRUM-X sound/grid) when the extension is required. Each record remains
  192 bytes; older firmware refuses these new versions. Existing records load
  with default LFO2 settings.
- The matching editor advertises capability 59 01 07 and uses SysEx commands
  88..90. Capture, audition, bank transfer and library JSON retain `lfo2`.
  UP_PUT appends tag 32 and seven raw 7-bit settings after kind/high/category.
  Unsupported firmware is rejected before sending a sound that requires LFO2.

## Validation

Tests cover independent rates, sync/trigger/polarity/fade, matrix sources,
Learn and settings persistence, all four project tracks, FM6 and DRUM-X payload
identity, user presets, undo, autosave signatures, legacy defaults, invalid
transfers, FULL launch boundaries and one/two-core parity with active LFO2.
The UI renderer sweeps every new control/value with normal and LARGE cards.
All 117 pre-existing golden sound renders remain unchanged.

Compared with a build without LFO2 on the same pi32v2 toolchain: 3756 additional
app bytes, 112 additional data/BSS bytes and 64 additional pool bytes (176
bytes total RAM). Target loop budgets are retained; new control-rate helper
footprints have separate measured baselines. These static measurements are
not a hardware CPU-percentage measurement. FM-1 listening, CPU/deadline and
USB/TRS controller hardware acceptance remain pending.
