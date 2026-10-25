/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define FELUCCA_CPU2 1
#define NTRK 4
#define HALF_FRAMES 128
#define ENGI_DRONE 16
#define ENGI_HADES 15
static uint32_t clock_ticks;
#define PD_CLOCK() clock_ticks
#include "../home-perf/perf_diag.h"
int main(void){
 pd_worker_mask=2;clock_ticks=100;t5_nested_ticks=10;pd_stamp_t s=pd_stamp();
 clock_ticks=300;t5_nested_ticks=90;
 pd_engine_done(0,ENGI_DRONE,s);pd_engine_done(1,ENGI_DRONE,s);
 assert(pd.engine[0].sum==120&&pd.worker_engine[0].sum==200);
 assert(pd.engine[0].calls==1&&pd.worker_engine[0].calls==1);
 pd_track_done(0,s);pd_track_done(1,s);
 assert(pd.track[0].sum==120&&pd.track[1].sum==200);
 clock_ticks=0xfffffff0;t5_nested_ticks=90;s=pd_stamp();clock_ticks=32;t5_nested_ticks=100;
 assert(pd_part_elapsed(0,s)==38&&pd_part_elapsed(1,s)==48);
 pd_worker_mask=0;assert(pd_part_elapsed(1,s)==38);
 puts("D1 timing: worker clock, core-0 interrupt exclusion, disjoint writers and wraparound OK");
}
