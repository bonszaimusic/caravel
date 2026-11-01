/* SPDX-License-Identifier: GPL-3.0-only */
/* MORPH JOURNEY pilot: S1..S4 recalls chained by a UI scheduler, controlled by learned MIDI CCs.
 * The firmware is booted in the host harness; time is simulated render time (1 ms per run_ms step). */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN journey_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void random_press(void){key(1u<<(ACT_KEY_FIRST+1u));key(0);}
static int32_t rev(void){return trk[0].p[P_REV];}
static int32_t absd(int32_t a,int32_t b){return a>b?a-b:b-a;}
/* a button CC: 0 then 127 = one rising edge */
static void press(uint32_t cc){midi(0xB0,cc,0);midi(0xB0,cc,127);}
static void learn(uint32_t j,uint32_t cc){
 ui.ml.on=1;ml_knob(&trk[0],HP_VFIRST+8u+j);CHECK(ui.ml.pick==1u&&ui.ml.id==HP_VFIRST+8u+j);
 midi(0xB0,cc,100);run_ms(60);CHECK(ui.ml.pick==0u);ui.ml.on=0;
}
static void store(uint32_t slot,int32_t r,int32_t p){
 trk[0].p[P_REV]=(int16_t)r;trk[0].p[P_PAN]=(int16_t)p;
 hp_rm_reset();hp_rm.scope=HP_RM_TRACK;CHECK(hp_rm_snapshot_store(slot));
}
enum{C_PLAY=20,C_DIR=21,C_NEXT=22,C_WAIT=23,C_SLOT=24,C_BARS=25};
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
 for(uint32_t t=0;t<NTRK;t++)hp_apply(t,0);
 CHECK(web_home_assign(0,0,0,HP_TRACK,0,P_REV));
 CHECK(web_home_assign(0,0,1,HP_TRACK,0,P_PAN));
 song.sel=0;go_home();action_enabled=1;action_bank=0;action_refresh();
 hp_rm_reset();hp_rm.scope=HP_RM_TRACK;hp_rm.bars=1;hp_rm.depth=100;hp_rm.shape=HP_RM_LINEAR;
 /* the targets are learnable and the learned CCs survive as ordinary virtual entries */
 learn(HJ_PLAY,C_PLAY);learn(HJ_DIR,C_DIR);learn(HJ_NEXT,C_NEXT);learn(HJ_WAIT,C_WAIT);
 learn(HJ_SLOT,C_SLOT);learn(HJ_BARS,C_BARS);
 /* fewer than two snapshots: nothing starts, nothing moves */
 store(0,10,-40);action_snap_slot=0;
 trk[0].p[P_REV]=77;
 press(C_PLAY);run_ms(100);CHECK(!hp_j.on);CHECK(rev()==77);
 store(1,90,40);store(2,50,0);
 /* snapshots of another owner/ref set are refused before anything moves */
 {hp_rm_snap_t keep=hp_rm_snap[2];hp_rm_snap[2].refs[0][0]^=1u;
  trk[0].p[P_REV]=77;press(C_PLAY);run_ms(100);CHECK(!hp_j.on);CHECK(rev()==77);hp_rm_snap[2]=keep;}
 /* the full loop with a one-bar rest: every arrival really reaches its snapshot's values (S1 S2 S3 S1) */
 midi(0xB0,C_WAIT,25);run_ms(30);CHECK(hp_j.wait==1u);
 trk[0].p[P_REV]=77;trk[0].p[P_PAN]=5;action_snap_slot=0;
 press(C_PLAY);run_ms(100);CHECK(hp_j.on);
 {static const int32_t R[3]={10,90,50},P[3]={-40,40,0};static const uint8_t order[4]={0,1,2,0};
  for(uint32_t k=0;k<4u;k++){
   uint32_t guard=0;while(hp_j.phase!=1u&&guard++<12000u)run_ms(10);
   CHECK(hp_j.phase==1u&&hp_j.cur==order[k]);
   CHECK(absd(rev(),R[order[k]])<=1&&absd(trk[0].p[P_PAN],P[order[k]])<=1);
   guard=0;while(hp_j.phase==1u&&guard++<12000u)run_ms(10);
  }}
 random_press();run_ms(100);CHECK(!hp_j.on);
 midi(0xB0,C_WAIT,0);run_ms(30);CHECK(hp_j.wait==0u);
 /* PLAY: first leg goes to the selected slot (S1), then the scheduler chains S2, S3, S1 ... */
 trk[0].p[P_REV]=77;action_snap_slot=0;
 press(C_PLAY);run_ms(100);CHECK(hp_j.on&&!hp_j.paused&&hp_j.tgt==0u);
 run_ms(2200);CHECK(hp_j.tgt==1u&&hp_j.cur==0u);            /* arrived at S1 (about 2 s), now heading to S2 */
 /* a repeated high value or the release does not trigger again; a new rising edge pauses */
 midi(0xB0,C_PLAY,127);midi(0xB0,C_PLAY,100);midi(0xB0,C_PLAY,0);CHECK(hp_j.on&&!hp_j.paused);
 midi(0xB0,C_PLAY,127);CHECK(hp_j.paused);CHECK(hp_rm.state==HP_RM_FROZEN);
 {int32_t at=rev();run_ms(600);CHECK(rev()==at);}
 press(C_PLAY);run_ms(100);CHECK(!hp_j.paused&&hp_rm.state==HP_RM_RECALL);
 /* arrive at S2, continue to S3 */
 run_ms(2400);CHECK(hp_j.tgt==2u);
 /* the shared slot follows the journey: RANDOM's selected snapshot is the one being approached */
 CHECK(action_snap_slot==hp_j.tgt);
 /* DIR (variant B) only sets the direction: the running leg finishes, the next one goes back */
 press(C_DIR);run_ms(100);CHECK(hp_j.rev==1u&&hp_j.tgt==2u&&hp_j.phase==0u);
 CHECK(strstr(ui.msg,"REVERSE AFTER S3")!=NULL);
 /* NEXT skips the rest of the leg at once: in reverse, the slot before S3 is S2, then S1 */
 press(C_NEXT);run_ms(100);CHECK(hp_j.tgt==1u&&action_snap_slot==1u);
 press(C_NEXT);run_ms(100);CHECK(hp_j.tgt==0u&&action_snap_slot==0u);
 /* a CC owns its parameter for the rest of the leg (the MIDI priority rule), the next leg moves it again */
 midi(0xB0,91,0);CHECK(rev()==0);
 run_ms(800);CHECK(rev()==0);
 run_ms(1500);CHECK(hp_j.tgt!=0u||absd(rev(),10)<=2);        /* the leg ended and the journey went on */
 run_ms(900);CHECK(rev()!=0);
 /* WAIT: rest 2 bars (band 2) after each arrival */
 midi(0xB0,C_WAIT,43);run_ms(30);CHECK(hp_j.wait==2u);
 {uint8_t t0=hp_j.tgt;uint32_t guard=0;while(hp_j.phase==0u&&guard++<8000u)run_ms(10);
  CHECK(hp_j.phase==1u);int32_t at=rev();run_ms(1500);CHECK(rev()==at&&hp_j.phase==1u&&hp_j.cur==t0);
  run_ms(2800);CHECK(hp_j.phase==0u);}
 midi(0xB0,C_WAIT,0);run_ms(30);CHECK(hp_j.wait==0u);
 /* SLOT is absolute and never saves: band 3 is the empty S4, band 1 steers the running journey to S2 */
 {uint32_t n3=hp_rm_snapshot_count(3);midi(0xB0,C_SLOT,100);run_ms(60);
  CHECK(action_snap_slot==3u&&hp_rm_snapshot_count(3)==n3&&n3==0u);
  midi(0xB0,C_SLOT,40);run_ms(60);CHECK(action_snap_slot==1u&&hp_j.tgt==1u&&hp_j.on);}
 /* BARS from a CC is latched while a leg moves and takes effect at the next leg */
 midi(0xB0,C_BARS,0);run_ms(30);CHECK(hp_rm.bars==1u);
 midi(0xB0,C_BARS,127);run_ms(30);CHECK(hp_rm.bars==1u&&hp_j.set[0]==7u);
 {uint32_t guard=0;while(hp_rm.bars==1u&&guard++<8000u)run_ms(10);CHECK(hp_rm.bars==64u);}
 /* starting a RANDOM morph by hand ends the journey */
 random_press();run_ms(100);CHECK(!hp_j.on);
 /* MIDI LEARN entry points on the ACTION controls */
 ui.ml.on=1;ui.ml.pick=0;
 key(1u<<(ACT_KEY_FIRST+1u));CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_PLAY);ui.ml.pick=0;
 key(1u<<(ACT_KEY_FIRST+2u));CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_DIR);ui.ml.pick=0;
 key(1u<<(ACT_KEY_FIRST+3u));CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_NEXT);ui.ml.pick=0;
 web_keys(1u<<(ACT_KEY_FIRST+4u));run_ms(120);web_keys(0);run_ms(60);   /* tap A4 */
 CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_WAIT);ui.ml.pick=0;
 {uint32_t n=hp_rm_snapshot_count(0);web_keys(1u<<(ACT_KEY_FIRST+4u));run_ms(700);web_keys(0);run_ms(60);   /* hold A4 */
  CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_SLOT&&hp_rm_snapshot_count(0)==n);ui.ml.pick=0;}
 action_setup_turn(2,1);CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_BARS+2u);ui.ml.pick=0;
 ui.ml.on=0;
 printf("MORPH JOURNEY PASS: %u checks.\n",checks);
 return 0;
}
