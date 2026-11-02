/* SPDX-License-Identifier: GPL-3.0-only */
/* RANDOM MORPH and MORPH JOURNEY tutorial screens from the actual firmware renderer, driven by the same panel keys a player uses.
 * Build like journey_panel_test.c (see docs/tutorials/random-morph-journey.md); frames land in build/release-prep/*.ppm. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN morph_tutorial_baseline_main
#include "../experiments/engine-pair-morph/pair_test.c"
static void shot(const char*name){char path[256];snprintf(path,sizeof path,"build/release-prep/%s.ppm",name);FILE*f=fopen(path,"wb");CHECK(f);fprintf(f,"P6\n240 240\n255\n");for(unsigned i=0;i<240*240;i++){uint16_t c=(web_fb[i]>>8)|(web_fb[i]<<8);unsigned char p[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(p,1,3,f);}fclose(f);}
static void ready(const char*n){ui.msg_t=ui.hot_t=0;ui.force=1;run_ms(60);shot(n);}      /* the screen at rest */
static void with_msg(const char*n){ui.force=1;run_ms(60);shot(n);}                         /* the screen with its message line */
static void tap(uint32_t k){key(1u<<(ACT_KEY_FIRST+k));key(0);}                           /* 0 = BANK, 1..4 = A1..A4 */
static void hold(uint32_t k,uint32_t ms){web_keys(1u<<(ACT_KEY_FIRST+k));run_ms(ms);web_keys(0);run_ms(60);}
static void store(uint32_t slot,int filt,int rev,int pan,int rate){
 trk[0].p[P_FILT]=(int16_t)filt;trk[0].p[P_REV]=(int16_t)rev;trk[0].p[P_PAN]=(int16_t)pan;trk[0].p[P_LRATE]=(int16_t)rate;
 action_snap_slot=slot;hold(4,700);
}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
 for(uint32_t t=0;t<NTRK;t++)hp_apply(t,0);
 host_preset(&trk[0],ENGI_SEM,3);
 CHECK(web_home_assign(0,0,0,HP_TRACK,0,P_FILT));
 CHECK(web_home_assign(0,0,1,HP_TRACK,0,P_REV));
 CHECK(web_home_assign(0,0,2,HP_TRACK,0,P_PAN));
 CHECK(web_home_assign(0,0,3,HP_TRACK,0,P_LRATE));
 CHECK(web_home_assign(1,0,0,HP_TRACK,1,P_FILT));CHECK(web_home_assign(1,0,1,HP_TRACK,1,P_REV));   /* track 2 has controls of its own */
 song.sel=0;go_home();action_enabled=1;action_bank=0;action_refresh();
 hp_rm_reset();hp_rm.scope=HP_RM_TRACK;hp_rm.bars=1;hp_rm.depth=100;hp_rm.shape=HP_RM_LINEAR;
 /* 1 RANDOM bank before anything is saved, then three snapshots saved with A4 hold */
 trk[0].p[P_FILT]=-30;trk[0].p[P_REV]=20;trk[0].p[P_PAN]=-30;trk[0].p[P_LRATE]=10;
 ready("random-empty");
 store(0,-30,20,-30,10);with_msg("random-saved");
 tap(4);store(1,40,90,30,70);tap(4);store(2,-10,55,0,35);
 tap(4);store(3,25,35,10,20);                                                  /* a fourth slot, deleted again below */
 action_snap_slot=0;ready("random-slots");
 /* 2 RANDOM SETUP: BARS, DEPTH, SCOPE, SHAPE */
 hold(0,700);CHECK(action_setup==1u);ready("random-setup");
 action_setup_turn(0,2);action_setup_turn(3,1);ready("random-setup-set");
 action_snap_slot=3;hold(4,700);CHECK(hp_rm_snapshot_count(0,3)==0u);with_msg("random-delete");      /* SETUP: A4 hold deletes the selected slot */
 action_snap_slot=0;
 hold(0,700);CHECK(action_setup==0u);
 hp_rm.bars=1;hp_rm.shape=HP_RM_LINEAR;
 /* 3 A3 morphs to the selected slot */
 action_snap_slot=1;tap(3);run_ms(450);ready("random-morph");
 run_ms(3000);
 /* 4 the JOURNEY bank: fifth bank after LIVE */
 tap(0);tap(0);tap(0);tap(0);CHECK(action_bank==ACT_BANK_JOURNEY);
 action_snap_slot=0;ready("journey-off");   /* the live sound is S2 (the last morph); PLAY heads for the selected S1 */
 tap(1);run_ms(700);ready("journey-play");
 tap(1);ready("journey-pause");
 tap(1);run_ms(60);
 tap(2);with_msg("journey-reverse");
 tap(4);tap(4);with_msg("journey-wait-set");
 /* wait for an arrival to see the resting state */
 {uint32_t guard=0;while(hp_j.phase==0&&guard++<4000u)run_ms(10);}
 ready("journey-resting");
 tap(3);run_ms(300);ready("journey-next");
 /* a RANDOM morph on track 2 while the journey of track 1 runs */
 song.sel=1;go_home();action_refresh();tap(0);CHECK(action_bank==0u);tap(1);run_ms(400);CHECK(hp_j.held==1u);ready("journey-held");
 printf("MORPH TUTORIAL FRAMES: %u checks.\n",checks);
 return 0;
}
