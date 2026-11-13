/* SPDX-License-Identifier: GPL-3.0-only */
/* Host regression for ELASTIC P0. No emulated audio or physical encoder. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../experiments/home-perf/home_elastic_core.h"

static void settle(hp_elastic_t *e, hp_elastic_cfg_t cfg, uint32_t at)
{
    for (uint32_t i = 1; i <= 6000u && e->active; ++i)
        hp_elastic_poll(e, at + 16u * i, cfg);
    assert(!e->active);
    assert(hp_elastic_value(e) == e->anchor);
}
static void test_no_touch_is_exact(void)
{
    hp_elastic_t e = {0}; hp_elastic_cfg_t cfg = HP_ELASTIC_DEFAULT;
    hp_elastic_reset(&e, 55, 0, 127, 0);
    for (uint32_t i = 1; i < 1000u; i++) hp_elastic_poll(&e, 16u*i, cfg);
    assert(e.anchor == 55 && hp_elastic_value(&e) == 55 && !e.active);
}
static void test_speed_and_return(void)
{
    hp_elastic_cfg_t cfg = HP_ELASTIC_DEFAULT;
    hp_elastic_t slow = {0}, fast = {0};
    hp_elastic_reset(&slow, 64, 0, 127, 0); hp_elastic_reset(&fast, 64, 0, 127, 0);
    hp_elastic_touch(&slow, 1, 1000u, cfg);
    hp_elastic_touch(&slow, 1, 1300u, cfg);
    hp_elastic_touch(&fast, 1, 1000u, cfg);
    hp_elastic_touch(&fast, 1, 1010u, cfg);
    assert(fast.v > slow.v);
    assert(hp_elastic_value(&fast) >= hp_elastic_value(&slow));
    settle(&slow,cfg,1300u); settle(&fast,cfg,1010u);
}
static void test_bounce_and_reexcite(void)
{
    hp_elastic_cfg_t cfg = HP_ELASTIC_DEFAULT;
    cfg.bounce=100; cfg.decay=85; cfg.range=40;
    hp_elastic_t e = {0}; hp_elastic_reset(&e, 64, 0, 127, 0);
    hp_elastic_touch(&e, 3, 1000u, cfg);
    hp_elastic_touch(&e, 3, 1012u, cfg);
    int cross = 0;
    for (uint32_t t = 1028u; t < 5028u; t += 16u) {
        hp_elastic_poll(&e,t,cfg);
        if (e.x < -128) cross=1;
    }
    assert(cross);       /* overshoot crosses the anchor */
    hp_elastic_touch(&e, -2, 5030u, cfg);  /* live gesture while settling */
    settle(&e,cfg,5030u);
}
static void test_bounds_and_cancel(void)
{
    hp_elastic_cfg_t cfg = HP_ELASTIC_DEFAULT;cfg.range=100;
    hp_elastic_t e={0};hp_elastic_reset(&e,126,0,127,0);
    for (uint32_t t=10u;t<=2000u;t+=10u) {
        hp_elastic_touch(&e,8,t,cfg);hp_elastic_poll(&e,t+16u,cfg);
        assert(hp_elastic_value(&e)>=0&&hp_elastic_value(&e)<=127);
    }
    hp_elastic_cancel(&e);
    assert(!e.active && hp_elastic_value(&e)==126);
    hp_elastic_reset(&e,10,-64,63,0);
    hp_elastic_touch(&e,-8,3,cfg);
    for(uint32_t t=16;t<15000;t+=16){hp_elastic_poll(&e,t,cfg);assert(hp_elastic_value(&e)>=-64 && hp_elastic_value(&e)<=63);}
    hp_elastic_cancel(&e);
}
static void test_hold_commit(void)
{
    hp_elastic_t e={0};hp_elastic_cfg_t cfg=HP_ELASTIC_DEFAULT;
    hp_elastic_reset(&e,50,0,127,0);
    hp_elastic_touch(&e,4,16,cfg);
    hp_elastic_hold(&e,1,16);
    int16_t v=hp_elastic_value(&e);
    hp_elastic_poll(&e,2000,cfg);
    assert(hp_elastic_value(&e)==v);
    hp_elastic_hold(&e,0,2000);
    hp_elastic_poll(&e,2016,cfg);
    hp_elastic_commit(&e);
    assert(hp_elastic_value(&e)==e.anchor && !e.active);
    assert(e.anchor!=50);
}
static void test_slow_both_directions(void)
{
    hp_elastic_cfg_t cfg = HP_ELASTIC_DEFAULT;
    hp_elastic_t e = {0};
    uint32_t settle_ms = 0;
    int below = 0, above = 0;
    hp_elastic_reset(&e, 64, 0, 127, 0);
    hp_elastic_touch(&e, 4, 16, cfg);
    for (uint32_t t = 32; t < 16000; t += 16) {
        hp_elastic_poll(&e, t, cfg);
        int16_t v = hp_elastic_value(&e);
        if (v < 64) below = 1;
        if (v > 64) above = 1;
        if (!e.active) { settle_ms = t; break; }
    }
    assert(below && above);                 /* it swings through the anchor both ways */
    assert(settle_ms >= 1500 && settle_ms <= 8000);
    assert(hp_elastic_value(&e) == e.anchor);
    hp_elastic_reset(&e, 64, 0, 127, 0);
    hp_elastic_touch(&e, -4, 16, cfg);
    below = above = 0;
    for (uint32_t t = 32; t < 16000; t += 16) {
        hp_elastic_poll(&e, t, cfg);
        int16_t v = hp_elastic_value(&e);
        if (v < 64) below = 1;
        if (v > 64) above = 1;
    }
    assert(below && above);                 /* negative gesture overshoots upward too */
    hp_elastic_cancel(&e);
}
static void test_stress_and_clock_wrap(void)
{
    hp_elastic_t e={0}; hp_elastic_cfg_t cfg=HP_ELASTIC_DEFAULT;
    uint32_t seed=0x752AB903u, now=UINT32_MAX-1000u;
    hp_elastic_reset(&e,0,-64,63,now);
    for (uint32_t i=0; i<300000u; ++i) {
        seed=seed*1664525u+1013904223u;
        now+=(seed>>24)%30u+1u; /* repeatedly crosses 32-bit wrap */
        if ((seed&63u)==0) {
            hp_elastic_cancel(&e);
            int16_t base=(int16_t)((seed>>8)%128u-64);
            hp_elastic_reset(&e,base,-64,63,now);
        }
        if ((seed&3u)==0) {
            cfg.response=(uint8_t)(seed%101u);
            cfg.decay=(uint8_t)((seed>>9)%101u);
            cfg.bounce=(uint8_t)((seed>>16)%101u);
            cfg.range=(uint8_t)((seed>>23)%101u);
            hp_elastic_touch(&e,(int32_t)((seed>>12)%17u)-8,now,cfg);
        }
        hp_elastic_poll(&e,now,cfg);
        int16_t v=hp_elastic_value(&e);
        assert(v>=e.low && v<=e.high);
        if (!e.active) assert(v==e.anchor);
    }
    hp_elastic_cancel(&e);
    assert(hp_elastic_value(&e)==e.anchor);
    assert(sizeof(hp_elastic_t)<=32u);
}
int main(void)
{
    test_no_touch_is_exact();
    test_speed_and_return();
    test_bounce_and_reexcite();
    test_bounds_and_cancel();
    test_hold_commit();
    test_slow_both_directions();
    test_stress_and_clock_wrap();
    puts("home_elastic_core_test: PASS (7 groups)");
}
