/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../experiments/home-perf/home_elastic_sync.h"
int main(void)
{
    static const char *const expected[]={"1/32 (RESERVED)","1/16","1/8","1/4","1/2","1/1", "2 BAR","4 BAR","8 BAR","16 BAR","32 BAR","64 BAR"};
    for(uint32_t i=HP_ES_MIN_INDEX;i<HP_ES_COUNT;i++){
        uint16_t v=(uint16_t)(HP_ES_SYNC_FLAG|i);
        assert(hp_es_is_sync(v));
        assert(hp_es_period_q12(v)==(512u<<i));
        assert(strcmp(hp_es_label(v),expected[i])==0);
        assert(hp_es_step(v,1)==(uint16_t)(HP_ES_SYNC_FLAG|(i+1u<HP_ES_COUNT?i+1u:i)));
    }
    assert(!hp_es_is_sync(2));
    assert(!hp_es_is_sync(0x800Fu));
    assert(!hp_es_is_sync(0x8000u)); /* old 1/32 selection rejected */
    assert(hp_es_step(0x8000u,-100)==0x8001u);
    assert(hp_es_step(0x8001u,-100)==0x8001u);
    assert(hp_es_step(0x800Bu,100)==0x800Bu);
    hp_es_phase_t s={0};
    for(uint32_t ms=0;ms<500;ms++)hp_es_advance(&s,1,120,0x8001u);
    assert(s.phase_q12==0);
    assert(s.remainder==0);
    s=(hp_es_phase_t){0};
    hp_es_advance(&s,500,120,HP_ES_SYNC_FLAG|7u);
    assert(hp_es_phase_q16(&s,HP_ES_SYNC_FLAG|7u)==4096u);
    s=(hp_es_phase_t){0};
    for(uint32_t i=0;i<128;i++)hp_es_advance(&s,1000,120,HP_ES_SYNC_FLAG|11u);
    assert(s.phase_q12==0);
    s=(hp_es_phase_t){0};hp_es_advance(&s,100,120,HP_ES_SYNC_FLAG|3u);
    uint32_t before=s.phase_q12;
    hp_es_advance(&s,100,180,HP_ES_SYNC_FLAG|3u);
    assert(s.phase_q12==((before+1229u)&4095u));
    /* One 2000ms advance must equal the old two 1000ms chunks,
     * even at the high BPM limit and maximum fractional carry. */
    for(uint32_t i=HP_ES_MIN_INDEX;i<HP_ES_COUNT;i++){
        hp_es_phase_t bulk={123u,59999u},halves=bulk;
        uint16_t v=(uint16_t)(HP_ES_SYNC_FLAG|i);
        hp_es_advance(&bulk,2000u,300u,v);
        hp_es_advance(&halves,1000u,300u,v);
        hp_es_advance(&halves,1000u,300u,v);
        assert(bulk.phase_q12==halves.phase_q12);
        assert(bulk.remainder==halves.remainder);
    }
    puts("ELASTIC sync phase tests PASS");return 0;
}
