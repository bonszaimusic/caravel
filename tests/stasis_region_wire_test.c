/* SPDX-License-Identifier: GPL-3.0-only
 * Standalone exhaustive native bit packing test. Actual project FORMAT-2
 * integration is additionally exercised by format2_project_test.c.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../firmware/src/stasis_region_wire.h"
_Static_assert(sizeof(stasis_region_saved_t)==5u,"five byte runtime struct");
static void check(stasis_region_saved_t s)
{
    uint8_t hb[24]={0},original[24];
    hb[0]=3u;hb[1]=5u;hb[2]=7u;hb[3]=11u;
    hb[4]=0xF1u;hb[5]=0xE2u;hb[6]=0xD3u;hb[7]=0x1Cu;
    memcpy(original,hb,sizeof hb);
    streg_wire_pack(hb,s);
    assert(hb[0]%16u==3u && hb[1]%16u==5u);
    assert(hb[2]%16u==7u && hb[3]%16u==11u);
    assert((hb[7]&31u)==0x1cu);
    stasis_region_saved_t got=streg_wire_unpack(hb);
    assert(streg_wire_bits(got)==streg_wire_bits(s));
    assert(!memcmp(hb,original,sizeof hb));
}
int main(void)
{
    stasis_region_saved_t d=streg_wire_default();
    assert(streg_wire_valid(d) && !streg_wire_nondefault(d));
    unsigned long count=0;
    for(uint32_t q=0;q<3u;q++)for(uint32_t g=0;g<4u;g++)
    for(uint32_t rev=0;rev<2u;rev++)
    for(uint32_t pos=0;pos<128u;pos++)for(uint32_t len=0;len<128u;len++) {
        stasis_region_saved_t s={(uint8_t)pos,(uint8_t)len,
                                 (uint8_t)g,(uint8_t)q,(uint8_t)rev};
        assert(streg_wire_valid(s));check(s);count++;
    }
    stasis_region_saved_t invalid={128u,64u,1u,0u,0u};
    assert(!streg_wire_valid(invalid));
    invalid=d;invalid.grain=4u;assert(!streg_wire_valid(invalid));
    invalid=d;invalid.quality=3u;assert(!streg_wire_valid(invalid));
    invalid=d;invalid.reverse=2u;assert(!streg_wire_valid(invalid));
    printf("STASIS2 FORMAT2 REGION packed bits %lu exhaustive cases PASS\n",count);
    return 0;
}
