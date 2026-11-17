/* SPDX-License-Identifier: GPL-3.0-only
 * HARM-first, conditional KEY, independent HARMONY key and legacy bus policy. */
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define UI_TEST_NO_MAIN 1
#include "ui_harness.h"

static uint32_t find_page(uint32_t graph) {
    for (uint32_t i = 0; i < NPAGES; i++)
        if (PAGES[i].graph == graph && PAGES[i].fam == FAM_SCL) return i;
    assert(0); return 0;
}
int main(void) {
    uint8_t notes[CHORD_MAX];
    int32_t root;
    uint16_t mask;
    ui_power_on();
    harmony_key_mode=1;harmony_key_root=0;harmony_key_scale=1; /* fresh 1.0 */
    harm_choose_role(TSEL,HROLE_NOTES);
    assert(harm_role(TSEL)==HROLE_NOTES);
    open_family(FAM_SCL);assert(cur_page()->graph==GR_LINK);
    assert(page_visible(find_page(GR_SCALE)));
    scl_page_turn(1);assert(cur_page()->graph==GR_SCALE);
    scl_page_turn(1);assert(cur_page()->graph==GR_HARMONY&&harmony_edit_dur==0);
    edit_param(1,5);assert(harmony_key_mode==1&&harmony_key_root==5);
    edit_param(2,1);assert(harmony_key_scale==2);
    uint32_t bus_mask=harmony_scale_abs();
    trk[0].p[P_ROOT]=2;trk[0].p[P_SCALE]=3;
    assert(harmony_scale_abs()==bus_mask); /* no T1 dependence */
    scl_page_turn(1);assert(cur_page()->graph==GR_HARMONY&&harmony_edit_dur==1);
    scl_page_turn(1);assert(cur_page()->graph==GR_HARMONY&&harmony_edit_dur==2);
    scl_page_turn(-1);assert(harmony_edit_dur==1);
    scl_page_turn(-1);assert(harmony_edit_dur==0);
    scl_page_turn(-1);assert(cur_page()->graph==GR_SCALE);
    scl_page_turn(-1);assert(cur_page()->graph==GR_LINK);

    edit_param(0,2);assert(harm_role(TSEL)==HROLE_CHORD);
    assert(TSEL->p[P_CHRD]==CH_DIA3&&page_visible(find_page(GR_SCALE)));
    edit_param(2,3); /* DIA3 -> MIN: remember local chord even when inactive */
    assert(TSEL->p[P_CHRD]==CH_MIN && harmony_local_shape[0]==CH_MIN);
    edit_param(1,1); /* K2 FROM HARMONY */
    assert(TSEL->p[P_CHRD]==CH_BUS && harmony_follow[0]);
    assert(!page_visible(find_page(GR_SCALE)));
    harmony_on=0;
    assert(chord_make(TSEL,60,notes,&root,&mask)==0); /* WAIT, no DIA3 */
    harmony_key_mode=0; /* legacy: historical local DIA3 fallback remains */
    assert(chord_make(TSEL,60,notes,&root,&mask)>0);
    harmony_key_mode=1;harmony_on=1;
    edit_param(1,-1);assert(TSEL->p[P_CHRD]==CH_MIN && harmony_local_shape[0]==CH_MIN);
    assert(page_visible(find_page(GR_SCALE)));

    edit_param(0,1);assert(harm_role(TSEL)==HROLE_EXT);
    assert(TSEL->p[P_CSRC]==1 && !page_visible(find_page(GR_SCALE)));
    scl_page_turn(1);assert(cur_page()->graph==GR_CHORD&&chord_ext_row);
    scl_page_turn(-1);assert(cur_page()->graph==GR_LINK);
    harm_choose_role(TSEL,HROLE_FOLLOW);
    assert(harm_role(TSEL)==HROLE_FOLLOW);
    assert(!page_visible(find_page(GR_SCALE)));

    TSEL->p[P_CHRD]=CH_DIA3;harmony_follow[0]=1;
    assert(harm_role(TSEL)==HROLE_CUSTOM); /* old mixed settings untouched */
    assert(page_visible(find_page(GR_SCALE)));
    open_family(FAM_SCL);
    assert(cur_page()->graph==GR_SCALE); /* already in family: next visible */
    /* KEY FROM is only effective for local-key roles, not FOLLOW or EXT. */
    link_src[1]=1;
    trk[0].p[P_ROOT]=0;trk[0].p[P_SCALE]=1;trk[0].p[P_QUANT]=1;
    trk[1].p[P_ROOT]=5;trk[1].p[P_SCALE]=2;trk[1].p[P_QUANT]=2;
    harm_choose_role(&trk[1],HROLE_NOTES);
    key_link_sync();
    assert(trk[1].p[P_ROOT]==0 && trk[1].p[P_SCALE]==1 && trk[1].p[P_QUANT]==1);
    harm_choose_role(&trk[1],HROLE_EXT);
    trk[1].p[P_ROOT]=5;trk[1].p[P_SCALE]=2;trk[1].p[P_QUANT]=2;
    key_link_sync();
    assert(trk[1].p[P_ROOT]==5 && trk[1].p[P_SCALE]==2 && trk[1].p[P_QUANT]==2);
    harm_choose_role(&trk[1],HROLE_FOLLOW);
    key_link_sync();
    assert(trk[1].p[P_ROOT]==5 && trk[1].p[P_SCALE]==2);
    trk[1].p[P_CHRD]=CH_DIA3;harmony_follow[1]=1; /* legacy mixed */
    key_link_sync();
    assert(trk[1].p[P_ROOT]==0 && trk[1].p[P_SCALE]==1);
    puts("HARM-first routing, CHORD K2, global key, legacy CUSTOM and BUS-off policy: PASS");
    return 0;
}
