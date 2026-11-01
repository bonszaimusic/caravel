/* SPDX-License-Identifier: GPL-3.0-only */
/* MORPH JOURNEY from the panel alone (no MIDI): the fifth ACTION bank. BANK taps reach it, A1 PLAY/PAUSE, A2 DIR, A3 NEXT,
 * A4 tap WAIT / hold save. No SETUP of its own: LEG/SHAPE are RANDOM's BARS/SHAPE. The firmware is booted in the host harness. */
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
 /* A4 tap steps WAIT 0 > 1 > 2 > 4 > 8 > 16 > 0 bars; the A4 note neither plays nor starts a RANDOM morph */
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
 CHECK(action_bank==0u&&hp_j.on);tap(1);run_ms(100);CHECK(hp_j.on&&hp_j.held==1u);   /* held, and it goes on after the morph */
 hp_j.on=hp_j.paused=hp_j.held=0;hp_rm_reset();run_ms(30);
 /* the JOURNEY bank has no SETUP of its own: LEG is RANDOM's BARS and SHAPE is RANDOM's SHAPE, WAIT is A4, the slot follows the journey */
 hp_rm_reset();CHECK(hp_rm.state==HP_RM_IDLE);action_snap_slot=0;hp_j.wait=0;hp_j.set[0]=hp_j.set[3]=0;hp_rm.bars=1;hp_rm.shape=HP_RM_LINEAR;
 CHECK(action_bank==0u);hold(0,700);CHECK(action_setup==1u);               /* SETUP on in RANDOM ... */
 tap(0);tap(0);tap(0);tap(0);CHECK(action_bank==ACT_BANK_JOURNEY&&action_setup==0u);   /* ... is left when JOURNEY is entered */
 /* BANK hold in the JOURNEY bank opens its SETUP: K1..K4 choose which tracks the journey takes along (LEG and SHAPE stay in RANDOM's SETUP) */
 hold(0,700);CHECK(action_setup==1u&&strstr(ui.msg,"JOURNEY TRACKS")!=NULL);
 CHECK(action_setup_value(0)==1&&action_setup_value(3)==1&&hp_j.skip==0u);
 action_setup_turn(1,-1);action_setup_turn(3,-4);CHECK(hp_j.skip==(2u|8u)&&action_setup_value(1)==0&&action_setup_value(3)==0&&action_setup_value(0)==1);
 {char n[16],v[24];action_setup_caption(1,n,v);CHECK(!strcmp(n,"TRACK 2")&&!strcmp(v,"SKIP"));action_setup_caption(0,n,v);CHECK(!strcmp(v,"ON"));}
 CHECK(hp_rm.bars==1u);                                                        /* the knobs of this bank edit no RANDOM value */
 action_setup_turn(1,1);action_setup_turn(3,1);CHECK(hp_j.skip==0u);
 hold(0,700);CHECK(action_setup==0u);
 /* in RANDOM's SETUP, K1 BARS and K4 SHAPE are the journey's LEG and SHAPE: at once while nothing runs ... */
 tap(0);CHECK(action_bank==0u);hold(0,700);CHECK(action_setup==1u);
 action_setup_turn(0,2);CHECK(hp_rm.bars==4u&&hp_j.set[0]==0u);
 action_setup_turn(3,1);CHECK(hp_rm.shape==HP_RM_SMOOTH&&hp_j.set[3]==0u);
 action_setup_turn(0,-9);action_setup_turn(3,-9);CHECK(hp_rm.bars==1u&&hp_rm.shape==HP_RM_LINEAR);
 /* ... and held back while a journey runs, so a running leg keeps its speed; the display shows the pending value */
 hold(0,700);CHECK(action_setup==0u);
 tap(0);tap(0);tap(0);tap(0);CHECK(action_bank==ACT_BANK_JOURNEY);
 hp_j.wait=0;trk[0].p[P_REV]=77;tap(1);run_ms(100);CHECK(hp_j.on&&hp_rm.state==HP_RM_RECALL);
 tap(0);CHECK(action_bank==0u);hold(0,700);CHECK(action_setup==1u);
 action_setup_turn(0,3);CHECK(hp_rm.bars==1u&&hp_j.set[0]!=0u&&action_setup_value(0)==(int32_t)hp_rm_times[hp_j.set[0]-1u]);
 action_setup_turn(3,2);CHECK(hp_rm.shape==HP_RM_LINEAR&&hp_j.set[3]!=0u&&action_setup_value(3)==2);
 /* SCOPE resets the morph: in RANDOM's SETUP it waits for the next leg too and does not wipe the running one (review of PR #34) */
 {uint8_t scope=hp_rm.scope;action_setup_turn(2,1);
  CHECK(hp_rm.scope==scope&&hp_j.set[2]==(uint8_t)(scope+2u)&&action_setup_value(2)==(int32_t)scope+1&&hp_j.on&&hp_rm.state==HP_RM_RECALL);}
 {uint32_t guard=0;while((hp_j.set[0]||hp_j.set[2]||hp_j.set[3])&&guard++<8000u)run_ms(10);
  CHECK(hp_rm.bars==8u&&hp_rm.shape==2u&&hp_rm.scope==HP_RM_ALL&&hp_j.on);}
 hp_rm.scope=HP_RM_TRACK;
 hold(0,700);CHECK(action_setup==0u);
 /* MIDI LEARN in the JOURNEY bank: A1..A3 as before, A4 tap WAIT, hold SLOT; BARS and SHAPE are picked in RANDOM's SETUP */
 tap(0);tap(0);tap(0);tap(0);CHECK(action_bank==ACT_BANK_JOURNEY);
 ui.ml.on=1;ui.ml.pick=0;
 tap(1);CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_PLAY);ui.ml.pick=0;
 tap(2);CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_DIR);ui.ml.pick=0;
 tap(3);CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_NEXT);ui.ml.pick=0;
 web_keys(1u<<(ACT_KEY_FIRST+4u));run_ms(120);web_keys(0);run_ms(60);
 CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_WAIT);ui.ml.pick=0;
 {uint32_t n=hp_rm_snapshot_count(0);hold(4,700);CHECK(ui.ml.pick&&ui.ml.id==HP_VFIRST+8u+HJ_SLOT&&hp_rm_snapshot_count(0)==n);ui.ml.pick=0;}
 ui.ml.on=0;
 /* BANK from JOURNEY wraps to RANDOM */
 tap(0);CHECK(action_bank==0u);
 printf("JOURNEY PANEL PASS: %u checks.\n",checks);
 return 0;
}
