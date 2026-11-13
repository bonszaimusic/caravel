/* SPDX-License-Identifier: GPL-3.0-only */
/* ELASTIC tempo-sync: pure fixed-point full-cycle phase, no heap or DSP work.
 * Quarter-note phase uses Q12; periods are powers of two and are exactly
 * representable as 1/16 note .. 64 four-quarter bars.
 * The historical prototype index 0 (1/32) is reserved/invalid. */
#ifndef CARAVEL_HOME_ELASTIC_SYNC_H
#define CARAVEL_HOME_ELASTIC_SYNC_H
#include <stdint.h>
#define HP_ES_SYNC_FLAG 0x8000u
#define HP_ES_COUNT 12u /* index 0 reserved; playable 1..11 */
#define HP_ES_MIN_INDEX 1u
#define HP_ES_DEFAULT (HP_ES_SYNC_FLAG | 3u) /* quarter note */
#define HP_ES_RETURN_INF 101u

typedef struct { uint32_t phase_q12; uint32_t remainder; } hp_es_phase_t;
static inline int hp_es_is_sync(uint16_t b)
{ return (b & 0xFFF0u) == HP_ES_SYNC_FLAG && (b & 15u)>=HP_ES_MIN_INDEX && (b & 15u)<HP_ES_COUNT; }
static inline uint32_t hp_es_index(uint16_t b)
{ return hp_es_is_sync(b)?(b & 15u):3u; }
static inline uint16_t hp_es_step(uint16_t b,int32_t detents)
{
    int32_t i=(int32_t)hp_es_index(b)+detents;
    if(i<(int32_t)HP_ES_MIN_INDEX)i=(int32_t)HP_ES_MIN_INDEX;
    if(i>=(int32_t)HP_ES_COUNT)i=HP_ES_COUNT-1;
    return (uint16_t)(HP_ES_SYNC_FLAG|(uint16_t)i);
}
static inline const char *hp_es_label(uint16_t b)
{
    /* Index zero is invalid, so omit its never-displayed string/slot. */
    static const char *const labels[HP_ES_COUNT-HP_ES_MIN_INDEX]={
        "1/16","1/8","1/4","1/2","1/1",
        "2 BAR","4 BAR","8 BAR","16 BAR","32 BAR","64 BAR"
    };
    return labels[hp_es_index(b)-HP_ES_MIN_INDEX];
}
/* Minimum playable 1/16 note is one quarter of a beat (Q12=1024).
 * The index-0 value is reserved and must not be offered by the UI.
 * These values assume a four-quarter bar, the FM-1 transport grid. */
static inline uint32_t hp_es_period_q12(uint16_t b)
{ return 512u<<hp_es_index(b); }
static inline uint32_t hp_es_phase_q16(const hp_es_phase_t *p,uint16_t b)
{
    uint32_t i=hp_es_index(b);
    return i<=7u ? p->phase_q12<<(7u-i) : p->phase_q12>>(i-7u);
}
static inline void hp_es_advance(hp_es_phase_t *p,uint32_t ms,uint32_t bpm,uint16_t b)
{
    if(bpm<20u)bpm=20u;
    if(bpm>300u)bpm=300u;
    /* A stalled UI must not create unbounded catch-up work. */
    if(ms>2000u)ms=2000u;
    if(!ms)return;
    /* Max 2000*300*4096+59999 = 2,457,659,999: fits uint32.
     * The old two-chunk carry is arithmetically identical to one step. */
    const uint32_t numerator=ms*bpm*4096u+p->remainder;
    const uint32_t whole=numerator/60000u;
    p->remainder=numerator%60000u;
    p->phase_q12=(p->phase_q12+whole)&(hp_es_period_q12(b)-1u);
}
#endif
