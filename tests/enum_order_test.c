/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 bonszaimusic */
/* The shown-order tables of the enum knobs (params.c enum_order: MSRC_ORDER, MDST_ORDER, DIV_ORDER, SLDIV_ORDER, DTIME_ORDER) against the
 * descriptors that use them: a table is a permutation of 0..max-min, so a knob turned to either end never reads past it and every value is
 * reachable. Found by a fuzz run (an ASan build with CARAVEL_STASIS_EXPAND=1, which is the firmware's default: felucca.c:57-58): MOD DST has 30 names there
 * (SCAN appended) but MDST_ORDER had 29 entries: param_turn read one byte past the table (ASan: global-buffer-overflow) and SCAN could
 * not be reached on the knob. Pre-existing (6b493ed, in 89bf631 = 0.12.0 and in the 0.13.0 line alike).
 * Build under ASan in the configurations: default, -DCARAVEL_REVERB_MINIMAL=1, + -DCARAVEL_STASIS_EXPAND=1, + -DFELUCCA_PAIR_MORPH=1.
 * Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"

int main(void)
{
    uint32_t e, id, nd = 0, bad = 0;
    const param_desc_t *seen[64];
    for (e = 0; e < 1u; e++)                             /* (the track-wide table TP: MOD SRC / DST, the note divisions) */
        for (id = 0; id < P_E0; id++) {
            const param_desc_t *d = param_desc_of(e, id);
            uint32_t k, dup = 0;
            int32_t v, n, visited[64] = {0}, cnt = 0, top;
            if (!d || d->fmt != F_ENUM || !enum_order(d))
                continue;
            for (k = 0; k < nd; k++)
                dup |= seen[k] == d || (seen[k]->names == d->names && seen[k]->min == d->min && seen[k]->max == d->max);
            if (dup || nd >= 64u)
                continue;
            seen[nd++] = d;
            n = d->max - d->min + 1;
            top = param_turn(d, enum_order(d)[0] + d->min, 1000);
            if (top < d->min || top > d->max) { printf("enum_order: id %u (e %u): +1000 from the first value gives %d outside %d..%d\n", id, e, (int)top, d->min, d->max); bad++; }
            v = enum_order(d)[0] + d->min;                 /* the first value of the shown order */
            for (k = 0; k < (uint32_t)n; k++) {          /* walk the shown order, one step at a time (the table is read at every rank) */
                if (v < d->min || v > d->max || visited[v - d->min]) { bad++; printf("enum_order: id %u (e %u): value %d reached twice or outside the range\n", id, e, (int)v); break; }
                visited[v - d->min] = 1; cnt++;
                v = param_turn(d, v, 1);
            }
            if (cnt != n) { printf("enum_order: id %u (e %u): %d of %d values reachable\n", id, e, (int)cnt, (int)n); bad++; }
            if (param_turn(d, top, 1000) != top || param_turn(d, top, -1000) != enum_order(d)[0] + d->min) { printf("enum_order: id %u (e %u): the ends are not stable\n", id, e); bad++; }
            printf("enum_order: id %u (e %u) %d values %s\n", id, e, (int)n, cnt == n ? "ok" : "FAIL");
        }
    if (nd < 4u) { printf("enum_order: only %u ordered descriptors found (want DIV, SLDIV, MSRC, MDST)\n", nd); bad++; }
    printf(bad ? "enum_order test FAILED\n" : "enum_order test passed\n");
    return bad != 0;
}
