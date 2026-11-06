/* SPDX-License-Identifier: GPL-3.0-only */
/* MORPH JOURNEY TRANSPORT option: FREE keeps time in milliseconds; CONTINUE and START count the transport's own musical time, so a Stop freezes a leg
 * and its WAIT, a tempo change speeds them up, and START begins the journey again with each Start. The firmware is booted in the host harness with
 * its internal clock; the external clock shares the same counter (seq.c) but is not simulated here. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN journey_transport_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void tap(uint32_t k){key(1u<<(k?action_keys[k-1u]:ACT_BANK_NEXT));key(0);}
static int32_t absd(int32_t a){return a<0?-a:a;}
static void set_all(int r,int p){trk[0].p[P_REV]=(int16_t)r;trk[0].p[P_PAN]=(int16_t)p;}
static void leg_state(const char*what){(void)what;}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
 for(uint32_t t=0;t<NTRK;t++)hp_apply(t,0);
 CHECK(web_home_assign(0,0,0,HP_TRACK,0,P_REV));CHECK(web_home_assign(0,0,1,HP_TRACK,0,P_PAN));
 song.sel=0;go_home();action_enabled=1;action_bank=0;action_refresh();
 song.g[G_BPM]=120;
 hp_eng[0].scope=HP_RM_TRACK;hp_eng[0].bars=2;hp_eng[0].shape=HP_RM_LINEAR;hp_jn[0].wait=0;
 hp_rm_snapshot_clear();hp_rm_reset_all();
 set_all(10,-40);action_snap_slot=0;CHECK(hp_rm_snapshot_store(0));
 set_all(90,40);action_snap_slot=1;CHECK(hp_rm_snapshot_store(1));
 while(action_bank!=ACT_BANK_JOURNEY)tap(0);
 CHECK(!song.playing);

 /* FREE: the leg runs with the transport stopped */
 set_all(50,0);hp_j.tp=0;action_snap_slot=0;tap(1);run_ms(1000);
 CHECK(hp_j.on&&hp_rm.state==HP_RM_RECALL&&hp_rm.phase>8000u&&!hp_rm.beat);
 hp_j.on=hp_j.paused=0;hp_rm_reset();

 /* CONTINUE: the leg waits for the transport, counts its bars, freezes on Stop and goes on with the next Start */
 hp_j.tp=1;set_all(50,0);action_snap_slot=0;tap(1);run_ms(1000);
 CHECK(hp_j.on&&strstr(ui.msg,"WAITS FOR PLAY")!=NULL&&hp_rm.beat&&hp_rm.phase==0u&&trk[0].p[P_REV]==50);
 seq_start();run_ms(1000);                                                   /* 120 BPM: 2 s a bar, so 1 s of 2 bars is a quarter */
 CHECK(hp_rm.phase>16384u-2500u&&hp_rm.phase<16384u+2500u);
 uint32_t at_stop;seq_stop();run_ms(200);at_stop=hp_rm.phase;run_ms(1500);CHECK(hp_rm.phase==at_stop&&hp_j.on);         /* Stop freezes it */
 seq_start();run_ms(60);CHECK(hp_j.on&&hp_rm.phase>=at_stop&&hp_rm.phase<at_stop+4000u&&hp_j.tgt==0u);         /* a Start resumes in CONTINUE */
 {uint32_t p0=hp_rm.phase;song.g[G_BPM]=240;run_ms(500);uint32_t d=hp_rm.phase-p0;                                 /* the tempo doubles: 0.5 s is what 1 s was */
  CHECK(d>16384u-3000u&&d<16384u+3000u);song.g[G_BPM]=120;}
 hp_j.on=hp_j.paused=0;hp_rm_reset();seq_stop();run_ms(100);

 /* the WAIT counts bars too */
 hp_j.tp=1;hp_j.wait=1;set_all(50,0);action_snap_slot=0;tap(1);seq_start();
 {uint32_t g=0;while(hp_j.phase==0u&&g++<2000u)run_ms(10);}CHECK(hp_j.phase==1u&&absd(trk[0].p[P_REV]-10)<=1);
 seq_stop();run_ms(2500);CHECK(hp_j.phase==1u&&hp_j.cur==0u);                                                      /* stopped: it rests on */
 seq_start();run_ms(1000);CHECK(hp_j.phase==1u);                                                                  /* 1 bar is 2 s */
 run_ms(1500);CHECK(hp_j.phase==0u&&hp_j.tgt==1u);                                                                /* then the next leg */
 hp_j.on=hp_j.paused=0;hp_rm_reset();seq_stop();run_ms(100);

 /* START: a Start restarts the journey from the first slot */
 hp_j.tp=2;hp_j.wait=0;set_all(50,0);action_snap_slot=1;seq_start();run_ms(100);tap(1);run_ms(2500);
 CHECK(hp_j.on&&hp_j.tgt==1u&&hp_rm.phase>20000u);                                                                 /* more than half way to S2 */
 seq_stop();run_ms(100);seq_start();run_ms(400);
 CHECK(hp_j.on&&hp_j.tgt==0u&&hp_rm.phase<20000u&&strstr(ui.msg,"START")!=NULL);                                   /* it began again at S1 */
 hp_j.tp=1;seq_stop();run_ms(100);seq_start();run_ms(100);CHECK(hp_j.tgt==0u);                                     /* CONTINUE does not restart */
 hp_j.on=hp_j.paused=0;hp_rm_reset();seq_stop();
 printf("JOURNEY TRANSPORT PASS: %u checks.\n",checks);
 return 0;
}
