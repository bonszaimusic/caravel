/* SPDX-License-Identifier: GPL-3.0-only
 * Reproduce a moving-writer source splice after a long STASIS hold.
 * Independent of HALL and grain heads: measures only the captured PCM seam.
 */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "stasis_join.h"

enum { TEST_FS = 44100u, TEST_RING = 65536u, TEST_LAG = 10349u };
static int16_t ring[TEST_RING];
static int32_t wave(uint32_t t)
{
    return (int32_t)(10000.0 *
        sin((double)t * 6.283185307179586 * 110.0 / TEST_FS));
}
static uint32_t scenario(uint32_t join)
{
    const uint32_t filled = 2u * TEST_FS;
    const uint32_t pause = 11u * TEST_FS + 16971u;
    const uint32_t render = TEST_LAG + 600u;
    int32_t anchor, prev = 0, slope = 0;
    uint32_t i, peak = 0u;
    for (i = 0u; i < filled; i++)
        ring[i & (TEST_RING - 1u)] = (int16_t)wave(i);
    anchor = ring[(filled - 1u) & (TEST_RING - 1u)];
    for (i = 0u; i < render; i++) {
        uint32_t w = filled + i;
        int32_t next = wave(filled + pause + i);
        if (join && i < STASIS_SOURCE_JOIN_FRAMES)
            next = stasis_source_join(anchor,next,
                    STASIS_SOURCE_JOIN_FRAMES - i);
        ring[w & (TEST_RING - 1u)] = (int16_t)next;
        int32_t out = ring[(w + 1u - TEST_LAG) & (TEST_RING - 1u)];
        int32_t diff = out - prev, curvature = diff - slope;
        if (curvature < 0) curvature = -curvature;
        if (i >= 2u && (uint32_t)curvature > peak)
            peak = (uint32_t)curvature;
        prev = out;
        slope = diff;
    }
    return peak;
}
int main(void)
{
    uint32_t raw = scenario(0u), joined = scenario(1u);
    printf("STASIS source seam: raw curvature=%u, joined=%u\n",raw,joined);
    assert(raw > 4000u); /* a held-to-live seam causes an actual PCM impulse */
    assert(joined < 1000u); /* 512-step join bounds the isolated input seam */
    puts("STASIS isolated source ring splice PASS (not full HALL/FX PCM)");
    return 0;
}
