/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN random_track_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void random_key(void){key(1u<<(ACT_KEY_FIRST+1u));key(0);}
static void targets(uint32_t track,uint32_t id){
    CHECK(hp_rm.count==1u);CHECK(hp_rm.cell[0].ref.track==track);CHECK(hp_tid(hp_rm.cell[0].ref)==id);
}
int main(void){
    web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
    for(uint32_t t=0;t<NTRK;t++){
        hp_apply(t,0);
        CHECK(web_home_assign(t,0,0,HP_TRACK,t,P_FILT));
        CHECK(web_home_assign(t,1,0,HP_TRACK,t,P_REV));
        trk[t].p[P_FILT]=0;trk[t].p[P_REV]=40;hp.track[t].active=0;
    }
    song.sel=0;go_home();action_enabled=1;action_bank=0;action_refresh();
    for(uint32_t t=0;t<NTRK;t++){hp_eng[t].scope=HP_RM_TRACK;hp_eng[t].bars=1;}   /* every track has its own engine and settings */
    random_key();CHECK(hp_rm.origin_track==0u);CHECK(hp_rm.count==2u);
    int16_t original=hp_rm.cell[0].original;
    random_key();CHECK(hp_rm.cell[0].original==original); /* same context retains RETURN origin */
    track_select(1);run_ms(50);CHECK(hp_eng[0].origin_track==0u&&hp_eng[0].state==HP_RM_RANDOM&&hp_rm.state==HP_RM_IDLE); /* selection alone never redirects or stops a running morph */
    random_key();CHECK(hp_rm.origin_track==1u);
    for(uint32_t i=0;i<hp_rm.count;i++)CHECK(hp_rm.cell[i].ref.track==1u);
    hp_rm_freeze();track_select(2);random_key();CHECK(hp_rm.origin_track==2u);
    hp_rm.state=HP_RM_DONE;track_select(3);random_key();CHECK(hp_rm.origin_track==3u);
    hp_rm_reset();hp_rm.scope=HP_RM_ROW;hp.track[3].active=0;
    random_key();targets(3,P_FILT);
    hp.track[3].active=1;random_key();targets(3,P_REV);CHECK(hp_rm.origin_row==1u);
    hp_rm_freeze();track_select(0);hp_rm_reset();hp_rm.scope=HP_RM_ROW;random_key();targets(0,P_FILT);
    /* SCOPE ALL: every track's engine starts on that track's own HOME controls */
    hp_rm_reset();hp_rm.scope=HP_RM_ALL;random_key();
    for(uint32_t t=0;t<NTRK;t++){CHECK(hp_eng[t].count==2u&&hp_eng[t].state==HP_RM_RANDOM&&hp_eng[t].origin_track==t);for(uint32_t i=0;i<2u;i++)CHECK(hp_eng[t].cell[i].ref.track==t);}
    original=hp_eng[2].cell[0].original;
    track_select(2);hp_rm.scope=HP_RM_TRACK;random_key();CHECK(hp_rm.cell[0].original==original);   /* same context: the RETURN origin is retained */
    printf("RANDOM track/row selection PASS: %u checks; physical ACTION A1, active/frozen/completed contexts, selection pinning, same-context RETURN origin, one engine per track and ALL scope.\n",checks);
    return 0;
}
