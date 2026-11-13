/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../experiments/home-perf/home_elastic_trajectory.h"
static hp_elastic_cfg_t c(void){return HP_ELASTIC_DEFAULT;}
/* Preserve exact Q16 easing output, including round-up at the upper limit. */
static void test_exact_ease_rounding(void)
{
 for(uint32_t x=0;x<=65536u;x++){
  uint32_t ref;
  if(x==65536u)ref=65536u;
  else{
   uint32_t xx=(uint32_t)(((uint64_t)x*x+32768u)>>16);
   ref=(uint32_t)(((uint64_t)xx*(196608u-2u*x)+32768u)>>16);
  }
  assert(hp_et_ease(x)==ref);
 }
}
/* Q16 radix-256 version must match the old 64-bit division exactly.
 * Includes maximum 300s return and clamped elapsed-time edge cases. */
static void test_exact_fraction_q16(void)
{
 uint32_t seed=0x12986A13u;
 for(uint32_t dur=16u;dur<=300000u;dur+=223u){
  assert(hp_et_fraction_q16(0u,dur)==0u);
  assert(hp_et_fraction_q16(dur,dur)==65536u);
  assert(hp_et_fraction_q16(dur+1u,dur)==65536u);
  for(uint32_t j=0u;j<25u;j++){
   seed=seed*1664525u+1013904223u;
   uint32_t elapsed=seed%(dur+1u);
   uint32_t old=(uint32_t)(((uint64_t)elapsed*65536u)/dur);
   assert(hp_et_fraction_q16(elapsed,dur)==old);
  }
 }
 assert(hp_et_fraction_q16(299999u,300000u)==
        (uint32_t)(((uint64_t)299999u*65536u)/300000u));
}
static void test_time_map(void)
{
 assert(hp_et_ms(0)==16 && hp_et_ms(20)==80 && hp_et_ms(40)==400);
 assert(hp_et_ms(60)==2000 && hp_et_ms(80)==10000 && hp_et_ms(100)==45000);
 uint32_t last=0;for(uint32_t i=0;i<=100;i++){uint32_t m=hp_et_ms(i);assert(m>=last && m<=45000);last=m;}
 assert(hp_et_return_ms(0)==16 && hp_et_return_ms(20)==80);
 assert(hp_et_return_ms(40)==400 && hp_et_return_ms(60)==2000);
 assert(hp_et_return_ms(80)==10000 && hp_et_return_ms(90)==45000);
 assert(hp_et_return_ms(91)==50000 && hp_et_return_ms(94)==90000);
 assert(hp_et_return_ms(95)==120000 && hp_et_return_ms(100)==300000);
 assert(hp_et_return_ms(255)==300000); /* defensive clamp */
 last=0;for(uint32_t i=0;i<=100;i++){
  uint32_t m=hp_et_return_ms(i);assert(m>=last && m<=300000);last=m;
 }
}
static void test_no_instant_then_both_phases(void)
{
 hp_elastic_t e={0};hp_et_t t={0};hp_et_cfg_t time=HP_ET_DEFAULT;time.bounce=2u;time.pull=30;time.back=70;
 hp_elastic_reset(&e,60,0,127,0);hp_et_touch(&e,&t,7,100,time,c());
 assert(e.x==0 && hp_elastic_value(&e)==60 && t.phase==1);
 uint32_t rise=hp_et_ms(time.pull),fall=hp_et_return_ms(time.back);
 hp_et_poll(&e,&t,100+rise/2,time,c());assert(e.x>0 && e.x<t.goal_q8);
 hp_et_poll(&e,&t,100+rise,time,c());assert(t.phase==2 && e.x==t.goal_q8);
 int32_t peak=e.x;hp_et_poll(&e,&t,100+rise+fall/3,time,c());assert(e.x<0);
 assert(peak>0);hp_et_poll(&e,&t,100+rise+fall,time,c());
 assert(!e.active && t.phase==0 && hp_elastic_value(&e)==60);
}
static void test_extreme_times_and_hold(void)
{
 hp_elastic_t e={0};hp_et_t t={0};hp_et_cfg_t time=HP_ET_DEFAULT;time.bounce=2u;time.pull=100;time.back=100;
 hp_elastic_reset(&e,64,0,127,0);hp_et_touch(&e,&t,-8,16,time,c());
 assert(e.x==0);hp_et_poll(&e,&t,1000,time,c());assert(e.x<0 && t.phase==1);
 int32_t before=e.x;hp_et_hold(&e,&t,1,1000);hp_et_poll(&e,&t,100000,time,c());assert(e.x==before);
 hp_et_hold(&e,&t,0,100000);hp_et_poll(&e,&t,144016,time,c());assert(t.phase==2);
 assert(t.return_duration_ms==300000);
 hp_et_poll(&e,&t,294016,time,c());assert(e.active && t.phase==2);
 hp_et_poll(&e,&t,444016,time,c());assert(!e.active && !t.phase);
 time.pull=0;time.back=0;
 hp_et_touch(&e,&t,8,445000,time,c());assert(e.x==0);
 hp_et_poll(&e,&t,445016,time,c());assert(t.phase==2 && e.x>0);
 hp_et_poll(&e,&t,445032,time,c());assert(!e.active && e.x==0);
}
static void test_300s_wrap_and_live_edit(void)
{
 hp_elastic_t e={0};hp_et_t t={0};
 hp_et_cfg_t times=HP_ET_DEFAULT;times.bounce=2u;times.pull=0;times.back=100;
 uint32_t start=UINT32_MAX-256u;
 hp_elastic_reset(&e,64,0,127,start);
 hp_et_touch(&e,&t,6,start,times,c());
 assert(t.return_duration_ms==300000u);
 hp_et_poll(&e,&t,start+16u,times,c());
 assert(t.phase==2u && e.x>0);
 int32_t initial=e.x;
 /* Encoder settings only alter the NEXT pluck. No 5min envelope jump. */
 times.back=0u;
 hp_et_poll(&e,&t,start+60016u,times,c());
 assert(t.phase==2u && t.return_duration_ms==300000u);
 assert(e.x!=initial);
 hp_et_hold(&e,&t,1u,start+60016u);
 int32_t held=e.x;
 hp_et_poll(&e,&t,start+160016u,times,c());
 assert(e.x==held && t.phase==2u);
 hp_et_hold(&e,&t,0u,start+160016u);
 hp_et_poll(&e,&t,start+350016u,times,c());
 assert(t.phase==2u && e.active);
 hp_et_poll(&e,&t,start+400016u,times,c());
 assert(!t.phase && !e.active && e.x==0);
 /* New touch uses the edited 16ms RETURN, not the old 300s. */
 hp_et_touch(&e,&t,-6,start+400016u,times,c());
 assert(t.return_duration_ms==16u);
 hp_et_poll(&e,&t,start+400048u,times,c());
 assert(!t.phase && !e.active && e.x==0);
}
static void test_repluck_reverse_wrap(void)
{
 hp_elastic_t e={0};hp_et_t t={0};hp_et_cfg_t time=HP_ET_DEFAULT;time.bounce=2u;time.pull=20;time.back=80;
 uint32_t now=UINT32_MAX-70u;
 hp_elastic_reset(&e,70,0,127,now);
 hp_et_touch(&e,&t,7,now,time,c());hp_et_poll(&e,&t,now+64u,time,c());
 int32_t before=e.x;assert(before>0);
 hp_et_touch(&e,&t,-8,now+70u,time,c());assert(e.x==before);
 hp_et_poll(&e,&t,now+150u,time,c());assert(e.x<before);
 hp_et_poll(&e,&t,now+50000u,time,c());assert(!e.active && e.x==0);
}
static void test_bounds_stress(void)
{
 hp_elastic_t e={0};hp_et_t t={0};hp_et_cfg_t tm=HP_ET_DEFAULT;tm.bounce=2u;tm.pull=0;tm.back=0;hp_elastic_cfg_t cfg=c();cfg.range=100;
 uint32_t seed=0x819af0u,now=UINT32_MAX-800u;
 hp_elastic_reset(&e,126,0,127,now);
 for(uint32_t i=0;i<100000;i++){
  seed=seed*1664525u+1013904223u;now+=(seed>>26)+1u;
  tm.pull=seed%101u;tm.back=(seed>>9)%101u;
  if((seed&7u)==0u)hp_et_touch(&e,&t,(int32_t)((seed>>16)%17u)-8,now,tm,cfg);
  if((seed&127u)==0u)hp_et_hold(&e,&t,!e.paused,now);
  hp_et_poll(&e,&t,now,tm,cfg);
  assert(hp_elastic_value(&e)>=0 && hp_elastic_value(&e)<=127);
  if(!t.phase)assert(hp_elastic_value(&e)==e.anchor);
 }
 hp_et_cancel(&e,&t);assert(e.x==0 && !e.active);
}
static void test_time_edit_does_not_jump(void)
{
 hp_elastic_t e={0};hp_et_t t={0};
 hp_et_cfg_t times=HP_ET_DEFAULT;times.bounce=2u;times.pull=20;times.back=60;
 hp_elastic_reset(&e,60,0,127,0);
 hp_et_touch(&e,&t,5,10,times,c());
 hp_et_poll(&e,&t,50,times,c());
 int32_t v=e.x;assert(v>0 && t.phase==1);
 times.pull=100;times.back=100; /* live SETUP edits affect next touch only */
 hp_et_poll(&e,&t,90,times,c());
 assert(t.phase==2 && e.x>=v);
 assert(t.pull_duration_ms==80 && t.return_duration_ms==2000);
 hp_et_poll(&e,&t,2090,times,c());assert(!e.active && e.x==0);
}

static void test_tempo_sync_and_endless(void)
{
 hp_elastic_t e={0};hp_et_t t={0};
 hp_et_cfg_t opt=HP_ET_DEFAULT;
 opt.pull=0u;opt.back=HP_ES_RETURN_INF;
 opt.bounce=HP_ES_SYNC_FLAG|3u; /* quarter note: 500 ms at 120 BPM */
 hp_elastic_reset(&e,64,0,127,0);
 hp_et_touch(&e,&t,6,100u,opt,c());
 assert(t.return_infinite && hp_es_is_sync(t.bounce_snapshot));
 hp_et_poll_bpm(&e,&t,116u,opt,c(),120u);
 assert(t.phase==2u && e.active && e.x>0);
 int32_t initial=e.x;
 hp_et_poll_bpm(&e,&t,241u,opt,c(),120u);
 assert(t.phase==2u && e.active && e.x!=initial);
 uint32_t phase=t.sync_phase.phase_q12;
 int32_t held=e.x;
 hp_et_hold(&e,&t,1u,241u);
 hp_et_poll_bpm(&e,&t,1241u,opt,c(),120u);
 assert(e.x==held && t.sync_phase.phase_q12==phase);
 hp_et_hold(&e,&t,0u,1241u);
 hp_et_poll_bpm(&e,&t,1366u,opt,c(),180u);
 assert(t.phase==2u && e.active && t.sync_phase.phase_q12!=phase);
 int32_t same_phase=e.x;
 /* Four-quarter-note cycle at 180 BPM: six cycles in 2 seconds.
  * Endlessly running spring must not lose amplitude with elapsed time. */
 hp_et_poll_bpm(&e,&t,3366u,opt,c(),180u);
 assert(e.x==same_phase && t.return_infinite);
 for(uint32_t i=0;i<6000;i++)
    hp_et_poll_bpm(&e,&t,3382u+i*16u,opt,c(),120u);
 assert(e.active && t.phase==2u && t.return_infinite);
 hp_et_cancel(&e,&t);
 assert(!e.active && t.phase==0u && e.x==0);
}

int main(void)
{
 test_exact_ease_rounding();test_exact_fraction_q16();test_time_map();test_no_instant_then_both_phases();test_extreme_times_and_hold();
 test_300s_wrap_and_live_edit();test_repluck_reverse_wrap();
 test_bounds_stress();test_time_edit_does_not_jump();test_tempo_sync_and_endless();
 puts("home_elastic_trajectory_test: PASS (8 groups incl. tempo/infinite)");
}
