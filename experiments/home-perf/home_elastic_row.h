/* SPDX-License-Identifier: GPL-3.0-only */
/* ELASTIC P1.2: at most four independent springs on the visible HOME row.
 * This header is a pure fixed-size control model, not the audio publisher.
 * No heap, no project storage and no hidden HOME-wide target graph. */
#ifndef CARAVEL_HOME_ELASTIC_ROW_H
#define CARAVEL_HOME_ELASTIC_ROW_H
#include <stdint.h>
#include "home_elastic_core.h"
#define HP_ER_MAX 4u

typedef struct {
    hp_elastic_t spring[HP_ER_MAX];
    uint16_t id[HP_ER_MAX];
    uint8_t valid, last, track, row, engine;
} hp_er_t;

static inline void hp_er_reset(hp_er_t *s,uint8_t track,uint8_t row,uint8_t engine)
{
    s->valid=0u;s->last=255u;s->track=track;s->row=row;s->engine=engine;
    for(uint32_t k=0;k<HP_ER_MAX;k++) {
        s->id[k]=0u;
        hp_elastic_cancel(&s->spring[k]);
    }
}
static inline uint32_t hp_er_count(const hp_er_t *s)
{
    uint32_t n=0u;
    for(uint32_t k=0;k<HP_ER_MAX;k++)if(s->valid&(1u<<k))n++;
    return n;
}
/* Duplicate track parameter IDs are deliberately rejected: one audio restore
 * bracket must never own two different spring states for the same p[id]. */
static inline int hp_er_bind(hp_er_t*s,uint32_t slot,uint16_t id,
                             int16_t base,int16_t lo,int16_t hi,uint32_t ms)
{
    if(slot>=HP_ER_MAX || lo>=hi || (s->valid&(1u<<slot)))return 0;
    for(uint32_t i=0;i<HP_ER_MAX;i++)
        if((s->valid&(1u<<i))&&s->id[i]==id)return 0;
    hp_elastic_reset(&s->spring[slot],base,lo,hi,ms);
    s->id[slot]=id;s->valid|=(uint8_t)(1u<<slot);
    return 1;
}
static inline int hp_er_touch(hp_er_t*s,uint32_t slot,int32_t raw,
                              uint32_t ms,hp_elastic_cfg_t cfg)
{
    if(slot>=HP_ER_MAX || !(s->valid&(1u<<slot)))return 0;
    if(s->spring[slot].paused)return 1;
    hp_elastic_touch(&s->spring[slot],raw,ms,cfg);
    s->last=(uint8_t)slot;
    return 1;
}
static inline int hp_er_pulse(hp_er_t*s,uint32_t ms,hp_elastic_cfg_t cfg)
{
    if(s->last>=HP_ER_MAX || !(s->valid&(1u<<s->last)))return 0;
    return hp_er_touch(s,s->last,1,ms,cfg);
}
static inline void hp_er_poll(hp_er_t*s,uint32_t ms,hp_elastic_cfg_t cfg)
{
    for(uint32_t k=0;k<HP_ER_MAX;k++)
        if(s->valid&(1u<<k))hp_elastic_poll(&s->spring[k],ms,cfg);
}
static inline void hp_er_hold(hp_er_t*s,uint8_t held,uint32_t ms)
{
    for(uint32_t k=0;k<HP_ER_MAX;k++)
        if((s->valid&(1u<<k))&&s->spring[k].active)
            hp_elastic_hold(&s->spring[k],held,ms);
}
static inline void hp_er_return(hp_er_t*s)
{
    for(uint32_t k=0;k<HP_ER_MAX;k++)
        if(s->valid&(1u<<k))hp_elastic_cancel(&s->spring[k]);
}
static inline void hp_er_unbind(hp_er_t*s,uint32_t slot)
{
    if(slot>=HP_ER_MAX)return;
    s->valid&=(uint8_t)~(1u<<slot);
    hp_elastic_cancel(&s->spring[slot]);
    if(s->last==slot)s->last=255u;
}
static inline int16_t hp_er_value(const hp_er_t*s,uint32_t slot)
{
    return slot<HP_ER_MAX && (s->valid&(1u<<slot))?
        hp_elastic_value(&s->spring[slot]):0;
}
#endif
