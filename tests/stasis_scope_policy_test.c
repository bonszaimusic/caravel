/* SPDX-License-Identifier: GPL-3.0-only
 * Verify SENDS/FILTER inclusion, HALL exclusion and Tone quantization.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../firmware/src/stasis_scope_policy.h"

int main(void)
{
    static const uint32_t breaks[] = {0u,25u,26u,51u,52u,
                                       76u,77u,102u,103u,127u};
    static const uint8_t levels[] = {0u,0u,1u,1u,2u,
                                      2u,3u,3u,4u,4u};
    uint32_t g, t;
    for (g = 0u; g < STASIS_SCOPE_SPACE_GROUPS; ++g) {
        assert(stasis_scope_show(1u,1u,g,3u));
        assert(!stasis_scope_show(1u,1u,g,2u)); /* HALL */
        assert(!stasis_scope_show(0u,1u,g,3u)); /* BUILD */
        assert(!stasis_scope_show(1u,0u,g,3u)); /* other area */
    }
    assert(!stasis_scope_show(1u,1u,5u,3u));
    for (t = 0u; t < sizeof breaks / sizeof breaks[0]; ++t)
        assert(stasis_scope_tone_tier(breaks[t]) == levels[t]);
    for (t = 1u; t <= 127u; ++t)
        assert(stasis_scope_tone_tier(t) >= stasis_scope_tone_tier(t-1u));
    assert(stasis_scope_tone_tier(1024u) == 4u);
    puts("STASIS Scope V6 policy: all 5 SPACE groups + 5 TONE tiers PASS");
    return 0;
}
