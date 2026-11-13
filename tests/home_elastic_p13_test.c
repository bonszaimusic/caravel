/* SPDX-License-Identifier: GPL-3.0-only */
/* Exact pure ELASTIC P1.3 math: wall restitution, sampled oscillations and
 * symmetric deterministic ROW coupling. No fake hardware or audio claims. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../experiments/home-perf/home_elastic_trajectory.h"

static void bounce_and_alias(void)
{
    assert(hp_ec_cycles(1u,300000u)==1u);
    assert(hp_ec_cycles(1000u,300000u)==1000u);
    assert(hp_ec_cycles(1000u,1000u)==7u);
    assert(hp_ec_cycles(1000u,160u)==0u);
    assert(hp_ec_bounce_step(1u,-10)==1u);
    assert(hp_ec_bounce_step(1u,15)==16u);
    assert(hp_ec_bounce_step(1000u,200)==1000u);
    uint16_t v=1u;
    for(uint32_t i=0;i<1000u;i++)v=hp_ec_bounce_step(v,1);
    assert(v==1000u);
    for(uint32_t i=0;i<1000u;i++)v=hp_ec_bounce_step(v,-1);
    assert(v==1u);
    hp_et_cfg_t cfg=HP_ET_DEFAULT;
    cfg.pull=0u;cfg.back=100u;cfg.bounce=1000u;
    hp_elastic_t e={0};hp_et_t t={0};hp_elastic_cfg_t c=HP_ELASTIC_DEFAULT;
    hp_elastic_reset(&e,64,0,127,0u);
    hp_et_touch(&e,&t,8,100u,cfg,c);
    assert(t.bounce_snapshot==1000u && t.reflect_snapshot==HP_EC_REFLECT_DEFAULT);
    cfg.bounce=1u;cfg.reflect=0u; /* new SETUP changes never retime active gesture */
    hp_et_poll(&e,&t,116u,cfg,c);
    assert(t.phase==2u && t.bounce_snapshot==1000u);
    int sign_changes=0,last_sign=0;
    for(uint32_t ms=132u;ms<300116u;ms+=16u){
        hp_et_poll(&e,&t,ms,cfg,c);
        int v=e.x>0?1:e.x<0?-1:0;
        if(v && last_sign && v!=last_sign)sign_changes++;
        if(v)last_sign=v;
        assert(hp_elastic_value(&e)>=0&&hp_elastic_value(&e)<=127);
    }
    assert(sign_changes>500); /* 1000 cycles produce many visible crossings */
    hp_et_poll(&e,&t,300116u,cfg,c);
    assert(!e.active && !t.phase && e.x==0);
}
static void walls(void)
{
    assert(hp_ec_reflect_x(130*256,0,100*256,0u)==100*256);
    assert(hp_ec_reflect_x(130*256,0,100*256,50u)==85*256);
    assert(hp_ec_reflect_x(130*256,0,100*256,100u)==70*256);
    assert(hp_ec_reflect_x(-10*256,0,100*256,50u)==5*256);
    assert(hp_ec_reflect_x(50*256,0,100*256,70u)==50*256);
    uint32_t seed=0x10d2026u;
    for(uint32_t i=0;i<100000u;i++){
        seed=seed*1664525u+1013904223u;
        int32_t x=(int32_t)(seed%400001u)-200000;
        for(uint32_t r=0;r<=100u;r+=25u){
            int32_t reflected=hp_ec_reflect_x(x,-40*256,60*256,r);
            assert(reflected>=-40*256&&reflected<=60*256);
        }
    }
}
static void coupling(void)
{
    int32_t x[4]={14000,-12000,9000,7000},travel[4]={20000,20000,20000,20000};
    for(uint32_t s=0;s<256u;s++){
        for(uint32_t i=0;i<4u;i++)for(uint32_t j=0;j<4u;j++){
            assert(hp_ec_weight(s,i,j)==hp_ec_weight(s,j,i));
            if(i==j)assert(hp_ec_weight(s,i,j)==0);
        }
        for(uint32_t i=0;i<4u;i++){
            assert(hp_ec_coupling_delta(x,travel,i,s,0u)==0);
            int32_t m=hp_ec_coupling_delta(x,travel,i,s,100u);
            assert(m<=travel[i]*35/100&&m>=-travel[i]*35/100);
        }
    }
    int32_t stationary[4]={0,0,0,0};
    for(uint32_t i=0;i<4u;i++)
        assert(hp_ec_coupling_delta(stationary,travel,i,42u,100u)==0);
    uint32_t rnd=0x12345678u;
    for(uint32_t i=0;i<100000u;i++){
        rnd=rnd*1664525u+1013904223u;
        x[(rnd>>20)&3u]=(int32_t)(rnd%40001u)-20000;
        uint32_t target=(rnd>>15)&3u;
        int32_t delta=hp_ec_coupling_delta(x,travel,target,rnd&255u,(rnd>>8)%101u);
        assert(delta<=travel[target]*35/100&&delta>=-travel[target]*35/100);
    }
}
int main(void)
{
    bounce_and_alias();walls();coupling();
    puts("home_elastic_p13_test: PASS (bounce 1..1000, bounded reflection, 4x4 seeded coupling)");
}
