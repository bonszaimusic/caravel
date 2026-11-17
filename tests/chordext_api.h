/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* CHORD SOURCE / EXT / OCT test adapter (tests/chordext_test.c, later the fuzzer): the three settings by NUMBER (ids 76, 77, 79
 * of the track's p[], retired DIGITAL operator ids), so that the same test source compiles on the commit before the engine
 * (slice 1a stores them and nothing reads them: every test then fails by behaviour, not by compile error).
 *   ce_set(track, CE_SRC | CE_EXT | CE_OCT, v)   SRC 0 OFF, 1 HARM, 2..5 T1..T4;
 *                                                EXT as the UI SHOWS it: 0 = 7, 1 = 7+9, 2 = 7+9+13 (stored 1, 0, 2: written
 *                                                here independently of chordext_stored);
 *                                                OCT -2..2
 *   ce_get(track, which)                         the same way back (EXT shown)
 *   ce_info(track, &o)                           ext_info (the UI's status)
 *   ce_pool(rel, root, kind, level, scale, pc)   the pure resolver (ext_pool): -1 when the firmware has none (base)
 * Included after tests/ui_test.c. */
#ifndef CHORDEXT_API_H
#define CHORDEXT_API_H

enum { CE_SRC, CE_EXT, CE_OCT };
/* ids by number: 76 SOURCE, 77 EXT, 79 OCT */
static void ce_set(uint32_t t, int which, int v)
{
    static const int8_t EXT_STORED[3] = {1, 0, 2};      /* shown 7, 7+9, 7+9+13 -> stored */
    if (which == CE_SRC) trk[t].p[76] = (int16_t)v;
    else if (which == CE_EXT) trk[t].p[77] = EXT_STORED[v < 0 ? 0 : v > 2 ? 2 : v];
    else trk[t].p[79] = (int16_t)v;
}
static int ce_get(uint32_t t, int which)
{
    static const int8_t EXT_SHOWN[3] = {1, 0, 2};       /* stored 0, 1, 2 -> shown (the swap of 0 and 1) */
    if (which == CE_SRC) return trk[t].p[76];
    if (which == CE_EXT) return EXT_SHOWN[(uint32_t)trk[t].p[77] > 2u ? 0 : trk[t].p[77]];
    return trk[t].p[79];
}
static void ce_info(uint32_t t, ext_info_t *o) { ext_info(t, o); }

/* kinds of ext_pool: the firmware's XK_* order (MAJ MIN DIM AUG SUS2 SUS4 POW) */
enum { CE_MAJ, CE_MIN, CE_DIM, CE_AUG, CE_SUS2, CE_SUS4, CE_POW };
#ifdef EXT_POOL_API
static int ce_pool(uint32_t rel, uint32_t root, uint32_t kind, uint32_t level, uint32_t scale, uint8_t *pc)
{
    static const uint8_t K[7] = {XK_MAJ, XK_MIN, XK_DIM, XK_AUG, XK_SUS2, XK_SUS4, XK_POW};
    return (int)ext_pool(rel, root, K[kind], level, scale, pc);
}
#else
static int ce_pool(uint32_t rel, uint32_t root, uint32_t kind, uint32_t level, uint32_t scale, uint8_t *pc)
{
    (void)rel; (void)root; (void)kind; (void)level; (void)scale; (void)pc;
    return -1;
}
#endif
#endif
