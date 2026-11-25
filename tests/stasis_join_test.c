/* SPDX-License-Identifier: GPL-3.0-only
 * Isolated HQ source-ring join test, without generated firmware tables.
 */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "stasis_join.h"

int main(void)
{
    int32_t prev = 10000, slope = 0, worst = 0, old = -1;
    uint32_t remaining;
    for (remaining = STASIS_SOURCE_JOIN_FRAMES; remaining; remaining--) {
        uint32_t t = STASIS_SOURCE_JOIN_FRAMES - remaining;
        int32_t live = (int32_t)(10000.0 *
            sin((double)(t + 10000u) * 6.283185307179586 * 110.0 / 44100.0));
        int32_t v = stasis_source_join(10000, live, remaining);
        int32_t monotonic = stasis_source_join(0, 32767, remaining);
        int32_t delta = v - prev, curvature = delta - slope;
        if (curvature < 0) curvature = -curvature;
        if (curvature > worst) worst = curvature;
        assert(v >= -10000 && v <= 10000);
        assert(monotonic >= old && monotonic <= 32767);
        old = monotonic;
        prev = v;
        slope = delta;
    }
    {
        const uint32_t t = STASIS_SOURCE_JOIN_FRAMES;
        const int32_t live = (int32_t)(10000.0 *
            sin((double)(t + 10000u) * 6.283185307179586 * 110.0 / 44100.0));
        int32_t curvature = (live - prev) - slope;
        if (curvature < 0) curvature = -curvature;
        if (curvature > worst) worst = curvature;
        assert(stasis_source_join(10000, live, 0u) == live);
        assert(old <= 32767 && stasis_source_join(0, 32767, 0u) == 32767);
    }
    assert(worst < 1000); /* isolated splice stays far below A4's 4000 */
    for (remaining = 0u; remaining <= STASIS_SOURCE_JOIN_FRAMES; remaining++) {
        int32_t up = stasis_source_join(-32768, 32767, remaining);
        int32_t down = stasis_source_join(32767, -32768, remaining);
        assert(up >= -32768 && up <= 32767);
        assert(down >= -32768 && down <= 32767);
    }
    assert(stasis_source_join(-32768, 32767, 512u) == -32768);
    assert(stasis_source_join(-32768, 32767, 0u) == 32767);
    puts("HQ source join: Q15 progression, saturation and 110 Hz seam PASS");
    return 0;
}
