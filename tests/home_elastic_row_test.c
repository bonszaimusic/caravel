/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../experiments/home-perf/home_elastic_row.h"
static hp_elastic_cfg_t cfg(void){return HP_ELASTIC_DEFAULT;}
static void init(hp_er_t*s){hp_er_reset(s,0,1,16);}
static void test_bind_dedup(void)
{
    hp_er_t s={0};init(&s);
    assert(hp_er_bind(&s,0,70,50,0,127,100));
    assert(!hp_er_bind(&s,1,70,70,0,127,100));
    assert(hp_er_bind(&s,1,72,70,0,127,100));
    assert(hp_er_bind(&s,2,73,40,0,127,100));
    assert(hp_er_count(&s)==3);
    assert(!hp_er_bind(&s,4,74,20,0,127,100));
    assert(!hp_er_bind(&s,3,75,20,127,127,100));
    hp_er_reset(&s,2,3,16);assert(!s.valid && s.last==255u);
}
static void test_independent_excitation(void)
{
    hp_er_t s={0};init(&s);
    assert(hp_er_bind(&s,0,70,50,0,127,0));
    assert(hp_er_bind(&s,1,71,60,0,127,0));
    assert(hp_er_touch(&s,0,4,100, cfg()));
    assert(hp_er_value(&s,0)!=50 && hp_er_value(&s,1)==60);
    assert(hp_er_touch(&s,1,-4,120,cfg()));
    assert(hp_er_value(&s,1)<60);
    assert(hp_er_value(&s,0)>50);
    int16_t a=hp_er_value(&s,0),b=hp_er_value(&s,1);
    hp_er_poll(&s,136,cfg());
    assert(hp_er_value(&s,0)!=50 || hp_er_value(&s,1)!=60);
    assert(a!=50 && b!=60);
    hp_er_return(&s);
    assert(hp_er_value(&s,0)==50 && hp_er_value(&s,1)==60);
}
static void test_pulse_last_only(void)
{
    hp_er_t s={0};init(&s);
    assert(hp_er_bind(&s,0,70,50,0,127,0));
    assert(hp_er_bind(&s,2,72,70,0,127,0));
    assert(!hp_er_pulse(&s,16,cfg()));
    hp_er_touch(&s,2,2,16,cfg());
    int32_t a=s.spring[2].v,unused=s.spring[0].v;
    assert(hp_er_pulse(&s,32,cfg()));
    assert(s.spring[2].v>a && s.spring[0].v==unused);
}
static void test_hold_and_cancel_one(void)
{
    hp_er_t s={0};init(&s);
    hp_er_bind(&s,0,70,50,0,127,0);
    hp_er_bind(&s,1,71,60,0,127,0);
    hp_er_touch(&s,0,4,100,cfg());
    hp_er_touch(&s,1,-4,100,cfg());
    hp_er_hold(&s,1,100);
    int16_t a=hp_er_value(&s,0),b=hp_er_value(&s,1);
    hp_er_poll(&s,2000,cfg());
    assert(hp_er_value(&s,0)==a && hp_er_value(&s,1)==b);
    hp_er_unbind(&s,0);
    assert(!(s.valid&1u) && (s.valid&2u));
    assert(hp_er_value(&s,1)==b);
    hp_er_hold(&s,0,2000);
    for(uint32_t t=2016;t<150000;t+=16)hp_er_poll(&s,t,cfg());
    assert(!s.spring[1].active && hp_er_value(&s,1)==60);
}
static void test_range_and_wrap(void)
{
    hp_er_t s={0};init(&s);
    hp_er_bind(&s,3,70,126,0,127,UINT32_MAX-50u);
    hp_elastic_cfg_t c=cfg();c.range=100;
    for(uint32_t i=0;i<20000;i++) {
        uint32_t now=(UINT32_MAX-50u)+i*16u;
        hp_er_touch(&s,3,(i%2u)?-8:8,now,c);
        hp_er_poll(&s,now,c);
        assert(hp_er_value(&s,3)>=0 && hp_er_value(&s,3)<=127);
    }
    hp_er_return(&s);assert(hp_er_value(&s,3)==126);
}
int main(void)
{
    test_bind_dedup();
    test_independent_excitation();
    test_pulse_last_only();
    test_hold_and_cancel_one();
    test_range_and_wrap();
    puts("home_elastic_row_test: PASS (5 groups)");
}
