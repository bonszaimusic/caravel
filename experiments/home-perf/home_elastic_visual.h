/* SPDX-License-Identifier: GPL-3.0-only */
/* ELASTIC P1.2: tiny, deterministic HOME cell motion indicator.
 * Geometry only; no font, framebuffer, heap, DSP or persistent state. */
#ifndef CARAVEL_HOME_ELASTIC_VISUAL_H
#define CARAVEL_HOME_ELASTIC_VISUAL_H
#include <stdint.h>

/* Signed 3px travel from the actual Q8 spring displacement, not the rounded
 * parameter value. Small slow PULLs therefore become visible before the
 * numerical parameter changes. This is display feedback, never DSP control. */
static inline int32_t hp_evis_shift(int32_t displacement_q8,int32_t travel_q8)
{
    if(!displacement_q8 || travel_q8<=0)return 0;
    int32_t sign=displacement_q8<0?-1:1;
    /* Unsigned subtraction handles INT32_MIN exactly, without 64-bit negation. */
    uint32_t magnitude=displacement_q8<0?
                       0u-(uint32_t)displacement_q8:(uint32_t)displacement_q8;
    uint32_t limit=(uint32_t)travel_q8;
    if(magnitude>limit)magnitude=limit;
    uint32_t amount=(magnitude*3u+limit/2u)/limit;
    if(amount==0u)amount=1u;
    if(amount>3u)amount=3u;
    return sign*(int32_t)amount;
}
/* Six zigzag links, in a 24x9 px area to the left of the units.
 * 14px at rest, up to 20px when stretched/compressed. */
static inline int32_t hp_evis_span(int32_t shift)
{
    if(shift<0)shift=-shift;
    if(shift>3)shift=3;
    return 14+2*shift;
}
static inline int32_t hp_evis_wave_y(int32_t center_y,int32_t shift,
                                    uint32_t segment)
{
    static const int8_t coil[7]={0,-1,1,-1,1,-1,0};
    if(segment>6u)segment=6u;
    if(shift<-3)shift=-3;
    if(shift>3)shift=3;
    return center_y+shift+coil[segment];
}
#endif
