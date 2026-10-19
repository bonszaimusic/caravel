<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Copyright (C) 2026 bonszaimusic -->
# LOFI BYTE and FLOAT

Two LOFI waves adapted from **JIANT** by juanjiant-bit (GPL-3.0-only). The formulas acknowledge the bytebeat and floatbeat community, among them
Tejeez, xpansive and viznut. Hardware acceptance on a real FM-1 is pending.

WAVE appends **BYTE** and **FLOAT** (values 5 and 6) with 32 formulas each. BYTE plays an 8-bit formula of the time t as the wave; FLOAT is
its sibling in floating point. Existing waves and preset numbers keep their meaning.

| Control | Effect |
| --- | --- |
| ALGO | selects the formula |
| VAR | mutates time |
| MOTION | controls the slow time |
| GRIT | holds BYTE samples |
| FEEDBK | feeds FLOAT's output into its phase |
| BEND | four levels |
| LOOP | limits the time window |

**BYTEBEAT** and **FLOAT FM** are appended factory presets. The controls use LOFI's existing eight engine parameters, so no common parameter or storage
format is added. CHIP STEP keeps its stepped envelope. MORPH on LOFI blends the five legacy waves, not the formulas (see
[MORPH and PHASE feedback](morph-and-feedback.md)).
