/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SLICES (EDIT family, a SLICE track only; after EDIT 2): the slices of the selected part's sample, set by hand.
 * Based on hugelton/Felucca#27 by andreahaku (SLICE EDIT), on the current UI: a page of the EDIT family with the
 * action-page OCT+ / OCT- of SAVE > TOOLS.
 * The markers: the slice starts the track plays (its DIV: 4..32 equal, AUTO, MAN) and the END of the last slice.
 *   KNOB 1  SLICE  the marker (1..n, then END); a key pressed picks the slice it plays (its keys are lit)
 *   KNOB 2  POS    moves it: one column of the view per detent; the view spans its neighbours, so a marker between
 *                  close ones moves in fine steps. Slice 1's start trims the head, END the tail
 *   KNOB 3  SPLIT  picked (right): OCT+ splits the slice at its middle (up to 32 slices)
 *   KNOB 4  JOIN   picked (right): OCT+ joins the slice to the one before (its start goes)
 *   OCT-    drops the pick, or (none) goes HOME
 * Edits are for a user slot (USR1..3: SRC); BREAK and PIANO show their slices only, a source with no material (it
 * plays a sine) says SAMPLE NOT FOUND (the no-file icon). The first edit takes
 * the slices shown into the slot's MAN table (eng_slice.c) and sets DIV to MAN; later edits change that table (the
 * audio side switches tables between notes). Leaving the page saves them with the sample (slice_store.c, once
 * stopped). Included by ui.c (before the action pages); the cards: ui_draw.c, the waveform: ui_graph.c. */
static int slice_page_ok(void) { return 0; }
static int slice_page_on(void) { return 0; }
