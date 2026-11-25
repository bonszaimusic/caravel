/* SPDX-License-Identifier: GPL-3.0-only
 * Host-only geometry guard for STASIS2 FREEZE -> LIVE resumption.
 * This proves source-address bounds under a moving writer. It does not
 * render PCM or establish click-free audio transitions.
 */
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include "../firmware/src/stasis_region.h"

int main(void)
{
    const uint32_t capacity=65536u, guard=256u;
    uint32_t cases=0u, checked=0u;
    for(uint32_t q=0u;q<3u;q++)
    for(uint32_t g=2048u;g<=16384u;g<<=1u)
    for(uint32_t valid=32768u;valid<=capacity;valid+=32768u)
    for(uint32_t pos=0u;pos<=127u;pos+=127u)
    for(uint32_t len=0u;len<=127u;len+=127u) {
        uint32_t factor=1u<<q;
        stasis_region_t frozen=stasis_region_plan(valid,capacity,g,factor,pos,len,1u);
        stasis_region_t live=stasis_region_plan(valid,capacity,g,factor,pos,len,0u);
        cases++;
        if(frozen.status<STREG_FIT || live.status<STREG_FIT)continue;
        assert(live.young>=guard && live.old<=valid-guard);
        assert(live.anchor_min>=live.young+live.span);
        assert(live.anchor_max<=capacity-live.span-guard);
        /* Model the reader age as the writer moves forward: reverse playback
         * traverses the source twice as fast in age coordinates. Both live
         * stereo taps clamp to this identical safe anchor interval. */
        const uint32_t preferred[]={0u,capacity/2u,capacity*2u};
        for(uint32_t h=0;h<3u;h++) {
            uint32_t age=stasis_region_anchor(live,preferred[h]);
            assert(age>=live.anchor_min && age<=live.anchor_max);
            for(uint32_t t=0;t<live.span;t++) {
                uint32_t reverse_age=age-live.span+1u+2u*t;
                assert(reverse_age>=guard && reverse_age<capacity-guard);
            }
        }
        checked++;
    }
    assert(checked>0u);
    printf("STASIS2 release writer/reader geometry: %u/%u ready configurations PASS (not PCM)\n",checked,cases);
    return 0;
}
