/* SPDX-License-Identifier: GPL-3.0-only
 * UI-only 6 Hz cap: boundaries, skipped intervals and millis wrap.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../firmware/src/stasis_scope_rate.h"

int main(void)
{
    uint32_t t, last = 0u, sent = 0u, first_window = 0u;
    uint8_t has_last = 0u;
    assert(STASIS_SCOPE_MIN_BLIT_MS == 167u);
    assert(stasis_scope_refresh_due(0u, 0u, 0u));
    assert(!stasis_scope_refresh_due(166u, 0u, 1u));
    assert(stasis_scope_refresh_due(167u, 0u, 1u));
    for (t = 0u; t < 2000u; ++t) {
        if (!stasis_scope_refresh_due(t,last,has_last)) continue;
        if (t < 1000u) ++first_window;
        if (has_last) assert((uint32_t)(t-last) >= 167u);
        last=t;has_last=1u;++sent;
    }
    assert(first_window == 6u);
    assert(sent == 12u); /* ticks 0,167,...,1837 */
    /* No catch-up after a long busy interval. */
    assert(stasis_scope_refresh_due(5000u,last,has_last));
    last=5000u;has_last=1u;
    assert(!stasis_scope_refresh_due(5001u,last,has_last));
    assert(!stasis_scope_refresh_due(5166u,last,has_last));
    assert(stasis_scope_refresh_due(5167u,last,has_last));
    /* Unsigned subtraction supports wraparound of the FM-1 ms clock. */
    last=0xfffffff0u;
    assert(!stasis_scope_refresh_due((uint32_t)(last+166u),last,1u));
    assert(stasis_scope_refresh_due((uint32_t)(last+167u),last,1u));
    puts("STASIS V6 scope 6-Hz refresh cap: boundary / 2-second / wrap PASS");
    return 0;
}
