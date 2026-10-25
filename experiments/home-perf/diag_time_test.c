/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FELUCCA_CPU2 0
#define NTRK 4
#define HALF_FRAMES 128
#define ENGI_DRONE 16
#define ENGI_HADES 15
static uint32_t synthetic_clock;
#define PD_CLOCK() synthetic_clock
#include "perf_diag.h"
#define C(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 synthetic_clock=0xfffffc00u;t5_nested_ticks=400;pd_stamp_t a=pd_stamp();
 synthetic_clock=600;t5_nested_ticks=700;C(pd_elapsed(a)==1324);
 pd_engine_done(0,ENGI_DRONE,a);C(pd.engine[0].sum==1324&&pd.engine[0].calls==1&&pd.engine[0].max==1324);
 pd_track_done(2,a);C(pd.track[2].sum==1324&&pd.track[2].max==1324);
 a=pd_stamp();synthetic_clock=1000;pd_engine_done(0,ENGI_HADES,a);C(pd.engine[1].sum==400);
 a=pd_stamp();synthetic_clock=1100;pd_engine_done(0,14,a);C(pd.engine[2].sum==100);
 pd_request=1;pd_seconds=60;pd_begin_half(0xfffffc00u,8);C(pd_active&&pd.state==1&&pd.seconds==60&&!pd.total);
 synthetic_clock+=PD_PERIOD*2;pd_begin_half(100,8);C(pd.gap_long==1&&pd.gap_max==PD_PERIOD*2);
 pd_finish_half(200,70000,400,1,10);C(pd.maximum==70000&&pd.deadline==1&&pd.shed==2&&pd.late==1&&pd.over85==1);
 pd_begin_half((uint32_t)(0xfffffc00u+60000u),10);C(!pd_active&&pd.state==3);
 unsigned halves=pd.halves;pd_finish_half(500,100,0,0,10);C(pd.halves==halves);
 pd_request=1;pd_seconds=10;pd_begin_half(100,42);C(pd_active&&pd.session==2&&!pd.total);
 pd_request=2;pd_begin_half(103,42);C(!pd_active&&pd.state==2);
 printf("DIAG CLOCK: wrap, TIMER5 exclusion, engine buckets, track time, timeout, deadline/late/shed and frozen stop passed (synthetic timer; no hardware cycle claim).\n");return 0;
}
