/* SPDX-License-Identifier: GPL-3.0-only */
/* MORPH JOURNEY from the panel alone (no MIDI): the fifth ACTION bank. BANK taps reach it, A1 PLAY/PAUSE, A2 DIR, A3 NEXT,
 * A4 tap REST / hold save, SETUP K1..K4 LEG REST SLOT SHAPE. The firmware is booted in the host harness. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN journey_panel_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void tap(uint32_t k){key(1u<<(ACT_KEY_FIRST+k));key(0);}   /* 0 = BANK, 1..4 = A1..A4 */
static void hold(uint32_t k,uint32_t ms){web_keys(1u<<(ACT_KEY_FIRST+k));run_ms(ms);web_keys(0);run_ms(60);}
static int32_t rev(void){return trk[0].p[P_REV];}
static void store(uint32_t slot,int32_t r,int32_t p){
 trk[0].p[P_REV]=(int16_t)r;trk[0].p[P_PAN]=(int16_t)p;
 hp_rm_reset();hp_rm.scope=HP_RM_TRACK;CHECK(hp_rm_snapshot_store(slot));
}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
 for(uint32_t t=0;t<NTRK;t++)hp_apply(t,0);
 CHECK(web_home_assign(0,0,0,HP_TRACK,0,P_REV));
 CHECK(web_home_assign(0,0,1,HP_TRACK,0,P_PAN));
 song.sel=0;go_home();action_enabled=1;action_bank=0;action_refresh();
 hp_rm_reset();hp_rm.scope=HP_RM_TRACK;hp_rm.bars=1;hp_rm.depth=100;hp_rm.shape=HP_RM_LINEAR;
 store(0,10,-40);store(1,90,40);store(2,50,0);action_snap_slot=0;
 /* the four existing banks keep their places; the new one follows LIVE and wraps back to RANDOM */
 CHECK(ACT_BANKS==5u&&ACT_BANK_JOURNEY==4u);
 tap(0);CHECK(action_bank==1u);tap(0);CHECK(action_bank==2u);tap(0);CHECK(action_bank==3u);
 tap(0);CHECK(action_bank==ACT_BANK_JOURNEY&&!strcmp(action_bank_names[action_bank],"JOURNEY"));
 /* A1 PLAY: the journey starts from the selected slot, no MIDI involved */
 trk[0].p[P_REV]=77;
 tap(1);run_ms(100);CHECK(hp_j.on&&!hp_j.paused&&hp_j.tgt==0u&&hp_rm.state==HP_RM_RECALL);
 /* A1 again pauses (the leg freezes), once more resumes */
 tap(1);CHECK(hp_j.paused&&hp_rm.state==HP_RM_FROZEN);
 {int32_t at=rev();run_ms(500);CHECK(rev()==at);}
 tap(1);run_ms(60);CHECK(!hp_j.paused&&hp_rm.state==HP_RM_RECALL);
 /* A4 tap steps REST 0 > 1 > 2 > 4 > 8 > 16 > 0 bars; the A4 note neither plays nor starts a RANDOM morph */
 CHECK(hp_j.wait==0u);tap(4);run_ms(60);CHECK(hp_j.wait==1u);tap(4);run_ms(60);CHECK(hp_j.wait==2u);
 tap(4);tap(4);tap(4);run_ms(60);CHECK(hp_j.wait==5u);tap(4);run_ms(60);CHECK(hp_j.wait==0u);
 CHECK(hp_rm.state==HP_RM_RECALL&&hp_j.on);
 /* A2 only sets the direction (the leg finishes); A3 skips to the next slot at once */
 tap(2);run_ms(60);CHECK(hp_j.rev==1u&&hp_j.on);
 {uint8_t t0=hp_j.tgt;tap(3);run_ms(100);CHECK(hp_j.tgt!=t0&&action_snap_slot==hp_j.tgt);}
 tap(2);run_ms(60);CHECK(hp_j.rev==0u);
 /* A4 hold saves the live sound into the selected slot, as in RANDOM; saving ends a running journey */
 tap(1);CHECK(hp_j.paused);
 {uint32_t slot=action_snap_slot;trk[0].p[P_REV]=33;hold(4,700);
  CHECK(!hp_j.on);
  CHECK(hp_rm_snapshot_count(slot)>0u);
  hp_rm_snap_t*s=&hp_rm_snap[slot];int found=0;
  for(uint32_t i=0;i<s->count;i++){hp_ref_t r=hp_rm_snapshot_ref(s,i);if(r.kind==HP_TRACK&&r.id==P_REV&&!r.aux){found=1;CHECK(s->values[i]==33);}}
  CHECK(found);}
 /* RANDOM's A3 "GO Sx" during a journey steers the journey to the selected slot instead of running a stray recall */
 tap(0);CHECK(action_bank==0u);tap(0);tap(0);tap(0);tap(0);CHECK(action_bank==ACT_BANK_JOURNEY);
 hp_j.wait=0;trk[0].p[P_REV]=77;action_snap_slot=0;tap(1);run_ms(100);CHECK(hp_j.on&&hp_j.tgt==0u);
 tap(0);CHECK(action_bank==0u);action_snap_slot=2;tap(3);run_ms(100);
 CHECK(hp_j.on&&hp_j.tgt==2u&&hp_rm.state==HP_RM_RECALL&&action_snap_slot==2u);
 /* a journey also ends when a RANDOM morph is started by hand in bank 0 */
 CHECK(action_bank==0u&&hp_j.on);tap(1);run_ms(100);CHECK(!hp_j.on);
 /* SETUP in the JOURNEY bank: hold BANK; K1 LEG, K2 REST, K3 SLOT, K4 SHAPE (nothing is moving: applied at once) */
 hp_rm_reset();CHECK(hp_rm.state==HP_RM_IDLE);action_snap_slot=0;hp_j.wait=0;hp_j.set[0]=hp_j.set[3]=0;hp_rm.bars=1;hp_rm.shape=HP_RM_LINEAR;
 tap(0);tap(0);tap(0);tap(0);CHECK(action_bank==ACT_BANK_JOURNEY);
 hold(0,700);CHECK(action_setup==1u);
 {char name[24],val[24];
  action_setup_caption(0,name,val);CHECK(!strcmp(name,"LEG")&&!strcmp(val,"1 BAR"));
  action_setup_caption(1,name,val);CHECK(!strcmp(name,"REST")&&!strcmp(val,"NONE"));
  action_setup_caption(2,name,val);CHECK(!strcmp(name,"SLOT"));
  action_setup_caption(3,name,val);CHECK(!strcmp(name,"SHAPE")&&!strcmp(val,"LINEAR"));
  action_setup_turn(0,2);CHECK(hp_rm.bars==4u||hp_j.set[0]==0u);   /* idle: applied at once */
  CHECK(hp_rm.bars==4u);
  action_setup_turn(1,2);CHECK(hp_j.wait==2u);action_setup_caption(1,name,val);CHECK(!strcmp(val,"2 BAR"));
  action_setup_turn(2,1);CHECK(action_snap_slot==1u);
  action_setup_turn(2,-5);CHECK(action_snap_slot==0u);
  action_setup_turn(3,1);CHECK(hp_rm.shape==HP_RM_SMOOTH);
  action_setup_turn(3,9);CHECK(hp_rm.shape==2u);
  action_setup_turn(0,-9);CHECK(hp_rm.bars==1u);}
 /* a running leg keeps its speed: LEG waits for the next leg, the display shows the pending value */
 hold(0,700);CHECK(action_setup==0u);
 hp_j.wait=0;trk[0].p[P_REV]=77;tap(1);run_ms(100);CHECK(hp_j.on&&hp_rm.state==HP_RM_RECALL);
 hold(0,700);CHECK(action_setup==1u);
 {char name[24],val[24];action_setup_turn(0,3);CHECK(hp_rm.bars==1u&&hp_j.set[0]!=0u);
  action_setup_caption(0,name,val);CHECK(strcmp(val,"1 BAR")!=0);}
 {uint32_t guard=0;while(hp_j.set[0]&&guard++<8000u)run_ms(10);CHECK(hp_rm.bars>1u);}
 /* MIDI LEARN in the JOURNEY bank: A1..A3 as before, K1..K4 pick LEG REST SLOT SHAPE */
 hold(0,700);CHECK(action_setup==0u);
 ui.ml.on=1;ui.ml.pick=0;
 tap(1);CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_PLAY);ui.ml.pick=0;
 tap(3);CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_NEXT);ui.ml.pick=0;
 action_setup_turn(0,1);CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_BARS);ui.ml.pick=0;
 action_setup_turn(1,1);CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_WAIT);ui.ml.pick=0;
 action_setup_turn(2,1);CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_SLOT);ui.ml.pick=0;
 action_setup_turn(3,1);CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_SHAPE);ui.ml.pick=0;
 ui.ml.on=0;
 /* BANK from JOURNEY wraps to RANDOM */
 tap(0);CHECK(action_bank==0u);
 printf("JOURNEY PANEL PASS: %u checks.\n",checks);
 return 0;
}
