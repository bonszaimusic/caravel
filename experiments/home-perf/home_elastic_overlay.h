/* SPDX-License-Identifier: GPL-3.0-only */
/* ELASTIC P1 proof: reversible, bounded per-block parameter overlay.
 * The caller owns the audio render bracket and must call end on every exit.
 * Pure helpers; no IO, persistent state, allocation or flash writes. */
#ifndef CARAVEL_HOME_ELASTIC_OVERLAY_H
#define CARAVEL_HOME_ELASTIC_OVERLAY_H
#include <stdint.h>
typedef struct {int16_t previous; uint8_t applied; uint16_t id;} hp_elo_frame_t;
static inline int16_t hp_elo_fit(int32_t v,int32_t lo,int32_t hi)
{ return (int16_t)(v<lo?lo:v>hi?hi:v); }
static inline void hp_elo_begin(int16_t *p,int32_t delta,int32_t lo,int32_t hi,hp_elo_frame_t *f)
{
    f->applied=0;
    if (!p || !delta) return;
    f->previous=*p;
    *p=hp_elo_fit((int32_t)f->previous+delta,lo,hi);
    f->applied=1;
}
static inline void hp_elo_end(int16_t *p,hp_elo_frame_t *f)
{
    if(f->applied){*p=f->previous;f->applied=0;}
}
#endif
