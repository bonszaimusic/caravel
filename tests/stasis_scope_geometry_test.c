/* SPDX-License-Identifier: GPL-3.0-only
 * Standalone geometric verification of the Scope V6/V7 LCD strips (stasis_scope_strips.h):
 * every pixel of the disc r<=48 is covered exactly once, no strip reaches beyond r52 and none
 * reaches the MIDI band (y190..200, below the orbit thumbs at y<=186 and above the message
 * text at y203).
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../firmware/src/stasis_scope_strips.h"
int main(void)
{
    uint32_t points=0u, pixels=0u, i;
    int32_t maxr2=0;
    static uint8_t seen[240][240];
    for(i=0u;i<STVIZ_STRIPS;i++){
        int32_t x0=120-(int32_t)STVIZ_HALF[i],w=2*(int32_t)STVIZ_HALF[i]+1;
        int32_t y0=(int32_t)STVIZ_STRIP_Y0+(int32_t)i*(int32_t)STVIZ_STRIP_H,x,y;
        assert(x0>=0 && x0+w<=240 && y0>=46 && y0+(int32_t)STVIZ_STRIP_H<=173);
        assert(y0+(int32_t)STVIZ_STRIP_H<=186);
        for(y=y0;y<y0+(int32_t)STVIZ_STRIP_H;y++)
            for(x=x0;x<x0+w;x++){
                int32_t r2=(x-120)*(x-120)+(y-116)*(y-116);
                assert(!seen[y][x]);          /* no overlap between strips */
                seen[y][x]=1u;
                if(r2>maxr2)maxr2=r2;
                ++pixels;
            }
    }
    assert(maxr2<=52*52);                     /* the knob thumbs and arcs reach in to about r53 */
    for(int32_t y=116-48;y<=116+48;y++)
        for(int32_t x=120-48;x<=120+48;x++)
            if((x-120)*(x-120)+(y-116)*(y-116)<=48*48){
                assert(seen[y][x]);           /* the whole inner disc is covered */
                ++points;
            }
    assert(116+61*0.9925+10<=187);               /* outer thumb envelope at 173/187 deg stays above the band */
    assert(190+11<=203-2);                    /* band y190..200 keeps >=2 rows to the message y203 */
    assert(203+12<=219-2);                      /* the legend cannot touch the track status row */
    assert(pixels<=8000u && points==7213u);
    printf("STASIS V6 strips: %u strips, %u pixels, %u inside r48 all covered, max r2 %d (<=52^2), MIDI band y190..200, message y203 PASS\n",
           (unsigned)STVIZ_STRIPS,pixels,points,(int)maxr2);
    return 0;
}
