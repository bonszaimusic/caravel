/* SPDX-License-Identifier: GPL-3.0-only */
/* ELASTIC P1.3: safe, pure control-rate boundary reflection and ROW coupling.
 * No audio-ISR code, hidden matrix, heap, trigonometric LUT or RNG state.
 * A seed selects a symmetric signed 4x4 pair map; COUPLE=0 is bit-identical
 * to independent P1.2 springs. The mapping is stable across reloads. */
#ifndef CARAVEL_HOME_ELASTIC_P13_H
#define CARAVEL_HOME_ELASTIC_P13_H
#include <stdint.h>

#define HP_EC_MAX 4u
#define HP_EC_BOUNCE_MAX 1000u
#define HP_EC_MIN_CYCLE_MS 128u /* at least 8 nominal 16ms control ticks */
#define HP_EC_REFLECT_DEFAULT 70u
#define HP_EC_BOUNCE_DEFAULT 2u

static inline uint32_t hp_ec_cycles(uint32_t requested,uint32_t return_ms)
{
    if(return_ms<=160u)return 0u; /* sub-160ms return is a single glide */
    if(requested<1u)requested=1u;
    if(requested>HP_EC_BOUNCE_MAX)requested=HP_EC_BOUNCE_MAX;
    uint32_t cap=return_ms/HP_EC_MIN_CYCLE_MS;
    return requested<cap?requested:cap;
}
/* Short detents resolve individual bounces. Longer values accelerate so
 * 1..1000 remains reachable with a few dozen turns. The displayed value is
 * exact (not a percentage); endpoints 1 and 1000 are selectable. */
static inline uint16_t hp_ec_bounce_step(uint16_t old,int32_t detents)
{
    int32_t v=old<1?1:old>1000?1000:old;
    if(detents>10000)detents=10000;
    if(detents< -10000)detents=-10000;
    while(detents){
        int32_t d=detents>0?1:-1;
        int32_t quantum=v<16?1:v<64?4:v<256?16:v<512?32:64;
        if(d<0 && v==16)quantum=1;
        else if(d<0 && v==64)quantum=4;
        else if(d<0 && v==256)quantum=16;
        else if(d<0 && v==512)quantum=32;
        v+=d*quantum;
        if(v<1)v=1;
        if(v>1000)v=1000;
        detents-=d;
    }
    return (uint16_t)v;
}
/* Damped geometrical reflection at the permitted parameter boundary, in Q8.
 * A projected overshoot is mirrored and multiplied by restitution 0..100.
 * This changes sign of the slope on impact. With restitution 0 it is the
 * previous clamped behavior; even pathological multi-wall overshoots run
 * at most 4 iterations, then clamp safely. */
static inline int32_t hp_ec_reflect_x(int32_t v,int32_t lo,int32_t hi,uint32_t restitution)
{
    if(hi<=lo)return lo;
    if(restitution>100u)restitution=100u;
    if(!restitution)return v<lo?lo:v>hi?hi:v;
    int32_t x=v;
    for(uint32_t n=0;n<4u && (x<lo||x>hi);n++){
        if(x>hi){
            int32_t over=x-hi;
            if(over>21474836){x=lo;break;} /* product stays inside int32 */
            x=hi-(over*(int32_t)restitution)/100;
        }else{
            int32_t over=lo-x;
            if(over>21474836){x=hi;break;}
            x=lo+(over*(int32_t)restitution)/100;
        }
    }
    if(x<lo)x=lo;
    if(x>hi)x=hi;
    return (int32_t)x;
}
/* Exact per-track pair weights. Index order is irrelevant: influence
 * between K1->K2 and K2->K1 shares the same deterministic signed link. */
static inline int32_t hp_ec_weight(uint32_t seed,uint32_t a,uint32_t b)
{
    if(a==b||a>=HP_EC_MAX||b>=HP_EC_MAX)return 0;
    if(a>b){uint32_t t=a;a=b;b=t;}
    uint32_t x=0x9E3779B9u+(seed&255u)*0x45d9f3bu+
                 (a+1u)*0x85EBCA6Bu+(b+3u)*0xC2B2AE35u;
    x^=x>>16;x*=0x7FEB352Du;x^=x>>15;x*=0x846CA68Bu;x^=x>>16;
    int32_t w=(int32_t)(x%201u)-100;
    return w?w:1;
}
/* Row-only bounded mix, called in UI control/publisher, not audio DSP.
 * Coupled offset is at most 35% of destination travel even if all three
 * source springs are maximally extended. A stationary destination can
 * respond to another moving spring, but never exceed its legal limits.
 * Returning 0 with strength=0 guarantees P1.2 default behavior. */
static inline int32_t hp_ec_coupling_delta(const int32_t x[HP_EC_MAX],
    const int32_t travel[HP_EC_MAX],uint32_t target,uint32_t seed,uint32_t strength)
{
    if(!strength||target>=HP_EC_MAX||travel[target]<=0)return 0;
    if(strength>100u)strength=100u;
    int32_t signed_sum=0;
    for(uint32_t j=0;j<HP_EC_MAX;j++){
        if(j==target||travel[j]<=0)continue;
        int32_t src=x[j];
        if(src>travel[j])src=travel[j];
        if(src<-travel[j])src=-travel[j];
        signed_sum+=(src*32768/travel[j])*hp_ec_weight(seed,target,j);
    }
    /* Three sources, 100-unit weights, 35% max travel. Keep all arithmetic
     * 32-bit: the pi32v2 target has no 64-bit divide helper. */
    int32_t avg=signed_sum/(3*32768);
    int32_t pct=(avg*(int32_t)strength*35)/10000;
    int32_t out=(pct*travel[target])/100;
    if(out>travel[target])out=travel[target];
    if(out<-travel[target])out=-travel[target];
    return out;
}
#endif
