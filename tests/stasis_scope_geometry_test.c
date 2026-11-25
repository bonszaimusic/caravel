/* SPDX-License-Identifier: GPL-3.0-only
 * Standalone geometric verification of the Scope V6/V7 LCD strips (stasis_scope_strips.h):
 * every pixel of the disc r<=48 is covered exactly once, no strip reaches beyond r52 and none
 * reaches the MIDI band (y186..196, below the lower knob arcs at y<=182 and above the legend
 * text at y201).
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
    assert(116+66*0.9925<=186);               /* lower knob arcs (r58..66, ends at 173/187 deg) stay above the band */
    assert(186+11<=201-3);                    /* band y186..196 keeps >=3 rows to the legend text from y201 */
    assert(201+10<=216);                      /* the legend cannot touch the track status row */
    assert(pixels<=8000u && points==7213u);
    printf("STASIS V6 strips: %u strips, %u pixels, %u inside r48 all covered, max r2 %d (<=52^2), MIDI band y186..196, legend y201 PASS\n",
           (unsigned)STVIZ_STRIPS,pixels,points,(int)maxr2);
    return 0;
}
