/* SPDX-License-Identifier: GPL-3.0-only
 * Standalone regression for the actual STASIS2 REGION coordinate helper.
 * No firmware-generated tables or FM-1 required: cc -std=c11 -Wall -Wextra
 * -Werror tests/stasis_region_math_test.c -o /tmp/streg && /tmp/streg
 */
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "../firmware/src/stasis_region.h"

static void check(uint32_t quality, uint32_t grain, uint32_t live)
{
    const uint32_t capacity=65536u, factor=1u<<quality;
    uint32_t wait=0u, ready=0u, limits=0u;
    for(uint32_t valid=0u;valid<=capacity;valid+=256u)
    /* Include 127 explicitly: the ordinary 7/13 strides stop at 126/117,
     * but HQ HOLD->LIVE failures exercise POS=127 and LENGTH=127. */
    for(uint32_t li=0u;li<=19u;li++)
    for(uint32_t pi=0u;pi<=10u;pi++) {
        uint32_t length=li==19u?127u:li*7u;
        uint32_t pos=pi==10u?127u:pi*13u;
        stasis_region_t r=stasis_region_plan(valid,capacity,grain,factor,pos,length,!live);
        if(r.status<STREG_FIT) {wait++;continue;}
        ready++;
        limits+=r.status==STREG_LIMIT;
        assert(r.effective_length>=r.span+256u);
        assert(r.anchor_min<=r.anchor_max);
        assert(r.young>=256u && r.old<=valid-256u);
        if(live) assert(r.old<=capacity-r.span-256u);
        for(uint32_t t=0;t<4u;t++){
            uint32_t age=stasis_region_anchor(r,r.young+t*22001u);
            assert(age>=r.anchor_min && age<=r.anchor_max);
            assert(age-r.span>=r.young);
            assert(age<=r.old);
        }
        {   /* MOTN scatter: inside the window, 0 at MOTN 0, growing with MOTN, the whole window at 127 */
            uint32_t width=r.anchor_max-r.anchor_min, prev=0u;
            static const uint32_t rs[3]={0u,32768u,65535u};
            for(uint32_t m=0u;m<=127u;m+=1u) {
                uint32_t w=stasis_region_wander(r,m,65535u);
                assert(w<=width && w>=prev);
                prev=w;
                if(m==0u) assert(w==0u);
                for(uint32_t k=0u;k<3u;k++) assert(stasis_region_wander(r,m,rs[k])<=width);
                assert(stasis_region_wander(r,m,0u)==0u);
            }
            assert(prev+1u>=width && prev<=width);   /* 127 with the largest random reaches the far edge (within a frame) */
            assert(stasis_region_wander(r,45u,65535u)*100u<=width*13u);   /* default MOTN: ~12 % of the window */
        }
    }
    assert(wait>0u && ready>0u && limits>0u && limits<ready);
    printf("REGION quality=%u grain=%u %s: ready=%u wait=%u limit=%u PASS\n",
        quality,grain,live?"LIVE":"FREEZE",ready,wait,limits);
}
int main(void)
{
    for(uint32_t q=0;q<3u;q++)
        for(uint32_t g=2048u;g<=16384u;g<<=1u)
            for(uint32_t live=0;live<=1;live++)check(q,g,live);
    stasis_region_t empty=stasis_region_plan(0,65536,16384,1,127,127,0);
    assert(empty.status==STREG_FILL);
    stasis_region_t blocked=stasis_region_plan(1024,1024,768,1,64,127,0);
    assert(blocked.status==STREG_WAIT); /* even a grain+guard cannot fit */
    puts("STASIS2 REGION geometry: 24 quality/grain/LIVE-FREEZE cases PASS (incl. MOTN scatter bounds)");
    return 0;
}
