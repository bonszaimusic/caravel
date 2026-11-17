/* SPDX-License-Identifier: GPL-3.0-only
 * Regressions: ELASTIC rings must follow their caption values. */
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void action_refresh(void);
static void action_dispatch(uint32_t);
#define FELUCCA_CPU2 0
#define UI_TEST_NO_MAIN 1
#include "ui_harness.h"
#include "../experiments/action-banks/action_control.c"
int main(void)
{
    char name[16], build[24], play[24];
    ui_power_on(); hp_init(); song.sel=0;
    hp_elastic_p1_init(); memset(&uxl,0,sizeof uxl); hp_el_setup_page=0;
    hp_el_times_track[0].pull=0; hp_el_times_track[0].back=0;
    hp_el_cfg_track[0].range=0;
    hp_el_times_track[0].bounce=HP_ES_SYNC_FLAG|HP_ES_MIN_INDEX;
    for(uint32_t k=0;k<4;k++)assert(uxl_el_arc_frac(k,0,1)==0);
    hp_el_times_track[0].pull=100; hp_el_times_track[0].back=HP_ES_RETURN_INF;
    hp_el_cfg_track[0].range=100;
    hp_el_times_track[0].bounce=HP_ES_SYNC_FLAG|(HP_ES_COUNT-1u);
    for(uint32_t k=0;k<4;k++)assert(uxl_el_arc_frac(k,0,1)==76);
    assert(uxl_ev_frac(ACT_BANK_ELASTIC,0)==76);
    assert(uxl_ev_frac(ACT_BANK_ELASTIC,3)==76);
    hp_el_times_track[0].bounce=1000u;
    assert(uxl_el_arc_frac(3,0,1)==76);
    hp_el_times_track[0].bounce=500u;
    assert(uxl_el_arc_frac(3,0,1)==38);
    /* BUILD shows and edits a track template; PLAY shows the last spring. */
    hp_el_times_track[0].pull=80u; hp_el1.on=1;
    hp_el_free_track[0]=1u; hp_er.valid=1u; hp_er.last=0u;
    hp_el_stimes[0][0]=hp_el_times_track[0];
    hp_el_stimes[0][0].pull=20u; hp_el_scfg[0][0]=hp_el_cfg_track[0];
    assert(uxl_el_arc_frac(0,0,1)==60);
    assert(uxl_el_arc_frac(0,0,0)==15);
    uxl_el_cap_ex(0,name,build,0,1);
    assert(!strcmp(name,"PULL"));
    uxl_el_cap_ex(0,name,play,0,0);
    assert(strcmp(build,play));
    uxl_el_setup_turn_ex(0,10,0,1);
    assert(uxl_el_arc_frac(0,0,1)==68);
    assert(uxl_el_arc_frac(0,0,0)==15);
    hp_el_times_track[0].couple=100u; hp_el_times_track[0].reflect=50u;
    hp_el_times_track[0].seed=255u;
    assert(uxl_el_arc_frac(0,1,1)==76);
    assert(uxl_el_arc_frac(1,1,1)==38);
    assert(uxl_el_arc_frac(2,1,1)==76);
    assert(uxl_el_arc_frac(3,1,1)==-1);
    hp_el_setup_page=1;
    assert(uxl_ev_frac(ACT_BANK_ELASTIC,0)==76);
    assert(uxl_ev_frac(ACT_BANK_ELASTIC,1)==38);
    assert(uxl_ev_frac(ACT_BANK_ELASTIC,3)==-1);
    puts("ux_elastic_arc_test: ELASTIC values, arcs, BUILD template, PLAY spring and pages PASS");
    return 0;
}
