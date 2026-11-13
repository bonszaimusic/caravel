/* SPDX-License-Identifier: GPL-3.0-only */
/* Pure render-bracket regression, no FM-1 hardware or full DSP emulation. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../experiments/home-perf/home_elastic_overlay.h"
static void test_off_and_empty(void)
{int16_t p=64;hp_elo_frame_t f={0};hp_elo_begin(&p,0,0,127,&f);assert(p==64 && !f.applied);hp_elo_end(&p,&f);assert(p==64);hp_elo_begin(0,50,0,127,&f);assert(!f.applied);}
static void test_audio_restore(void)
{int16_t p=71;hp_elo_frame_t f={0};hp_elo_begin(&p,23,0,127,&f);assert(p==94 && f.previous==71);hp_elo_end(&p,&f);assert(p==71 && !f.applied);}
static void test_macro_mod_order_and_early_return(void)
{int16_t p=60;hp_elo_frame_t f={0};int16_t macro_backup=p;p=70;hp_elo_begin(&p,25,0,127,&f);assert(p==95);int16_t mod_backup=p;p=107; /* modulation overrides after ELASTIC */
p=mod_backup;hp_elo_end(&p,&f);assert(p==70);p=macro_backup;assert(p==60);
/* silent path also unrolls the bracket */
p=70;hp_elo_begin(&p,-18,0,127,&f);assert(p==52);hp_elo_end(&p,&f);assert(p==70);p=60;}
static void test_clamping_and_sign(void)
{int16_t p=125;hp_elo_frame_t f={0};hp_elo_begin(&p,1000,0,127,&f);assert(p==127);hp_elo_end(&p,&f);assert(p==125);
p=2;hp_elo_begin(&p,-1000,0,127,&f);assert(p==0);hp_elo_end(&p,&f);assert(p==2);
p=-60;hp_elo_begin(&p,-50,-64,63,&f);assert(p==-64);hp_elo_end(&p,&f);assert(p==-60);}
static void test_independent_track_scratch(void)
{int16_t p[4]={20,40,60,80};hp_elo_frame_t f[4]={{0}};hp_elo_begin(p+0,5,0,127,f+0);hp_elo_begin(p+3,-23,0,127,f+3);assert(p[0]==25&&p[1]==40&&p[2]==60&&p[3]==57);hp_elo_end(p+0,f+0);hp_elo_end(p+3,f+3);assert(p[0]==20&&p[3]==80);}
int main(void){test_off_and_empty();test_audio_restore();test_macro_mod_order_and_early_return();test_clamping_and_sign();test_independent_track_scratch();puts("home_elastic_overlay_test: PASS (5 groups)");}
