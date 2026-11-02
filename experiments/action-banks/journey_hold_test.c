/* SPDX-License-Identifier: GPL-3.0-only */
/* MORPH JOURNEY next to RANDOM on other tracks: a RANDOM morph holds the journey and it goes on by itself, per-track RANDOM stays independent,
 * tracks can be left out of a journey, a snapshot slot can be deleted. The firmware is booted in the host harness. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN journey_hold_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void tap(uint32_t k){key(1u<<(ACT_KEY_FIRST+k));key(0);}   /* 0 = BANK, 1..4 = A1..A4 */
static void hold(uint32_t k,uint32_t ms){web_keys(1u<<(ACT_KEY_FIRST+k));run_ms(ms);web_keys(0);run_ms(60);}
static int32_t absd(int32_t a){return a<0?-a:a;}
static void sel(uint32_t t){song.sel=t;go_home();action_refresh();run_ms(60);}
static void set_all(int r1,int p1,int r2,int p2){trk[0].p[P_REV]=(int16_t)r1;trk[0].p[P_PAN]=(int16_t)p1;trk[1].p[P_REV]=(int16_t)r2;trk[1].p[P_PAN]=(int16_t)p2;}
static void bank(uint32_t b){uint32_t g=0;while(action_bank!=b&&g++<8u)tap(0);CHECK(action_bank==b);}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
 for(uint32_t t=0;t<NTRK;t++)hp_apply(t,0);
 /* HOME of track 1 pins T1's REV and PAN, HOME of track 2 pins T2's REV and PAN */
 for(uint32_t t=0;t<2;t++){CHECK(web_home_assign(t,0,0,HP_TRACK,t,P_REV));CHECK(web_home_assign(t,0,1,HP_TRACK,t,P_PAN));}
 song.sel=0;go_home();action_enabled=1;action_bank=0;action_refresh();
 hp_rm_reset();hp_rm.scope=HP_RM_TRACK;hp_rm.bars=1;hp_rm.depth=100;hp_rm.shape=HP_RM_LINEAR;

 /* 1. without a journey RANDOM stays per track: T2's morph moves T2 only, T1 stays where it was */
 set_all(10,-40,20,-20);
 tap(1);run_ms(600);
 CHECK(trk[0].p[P_REV]!=10&&hp_rm.state==HP_RM_RANDOM&&hp_rm.origin_track==0u);
 sel(1);tap(1);run_ms(100);                                                  /* T1's morph runs until T2's starts */
 {int32_t r1=trk[0].p[P_REV];run_ms(500);
  CHECK(hp_rm.state==HP_RM_RANDOM&&hp_rm.origin_track==1u&&trk[1].p[P_REV]!=20&&trk[0].p[P_REV]==r1);
  run_ms(2500);CHECK(trk[0].p[P_REV]==r1);}
 hp_rm_reset();sel(0);

 /* 2. a journey on T1; RANDOM on T2 holds it, T1 stands still, and it goes on by itself afterwards */
 set_all(10,-40,20,-20);action_snap_slot=0;hp_rm.bars=2;
 CHECK(hp_rm_snapshot_store(0));set_all(90,40,20,-20);action_snap_slot=1;CHECK(hp_rm_snapshot_store(1));
 set_all(50,0,20,-20);hp_j.wait=1;                                             /* WAIT 1 bar, so an arrival shows as a rest */
 bank(ACT_BANK_JOURNEY);action_snap_slot=0;tap(1);run_ms(1000);
 CHECK(hp_j.on&&hp_j.phase==0u&&hp_j.held==0u&&hp_rm.state==HP_RM_RECALL);
 sel(1);bank(0u);tap(1);run_ms(300);                                          /* T2 RANDOM */
 CHECK(hp_j.on&&hp_j.held==1u&&hp_rm.state==HP_RM_RANDOM&&hp_rm.origin_track==1u);
 {int32_t r=trk[0].p[P_REV],p=trk[0].p[P_PAN];run_ms(600);CHECK(trk[0].p[P_REV]==r&&trk[0].p[P_PAN]==p);}   /* T1 stands still while held */
 {uint32_t g=0;while(hp_j.held&&g++<3000u)run_ms(10);}
 run_ms(100);CHECK(hp_j.on&&hp_j.held==0u&&hp_rm.state==HP_RM_RECALL&&hp_j.tgt==0u);              /* it went on by itself, same target */
 {uint32_t g=0;while(hp_j.phase==0u&&g++<6000u)run_ms(10);}
 CHECK(hp_j.phase==1u&&hp_j.cur==0u&&absd(trk[0].p[P_REV]-10)<=1);                                 /* it arrived at S1 */
 hp_j.wait=0;

 /* 3. A1 while held switches the automatic go-on off; A1 again starts the leg from the live values */
 hp_j.on=hp_j.paused=hp_j.held=0;hp_rm_reset();set_all(50,0,20,-20);
 bank(ACT_BANK_JOURNEY);action_snap_slot=0;tap(1);run_ms(300);CHECK(hp_j.on);
 sel(1);bank(0u);tap(1);run_ms(200);CHECK(hp_j.held==1u);
 bank(ACT_BANK_JOURNEY);tap(1);run_ms(50);CHECK(hp_j.paused&&hp_j.held==2u);
 {uint32_t g=0;while(hp_rm.state==HP_RM_RANDOM&&g++<3000u)run_ms(10);}
 run_ms(300);CHECK(hp_j.on&&hp_j.paused&&hp_j.held==2u&&hp_rm.state!=HP_RM_RECALL);               /* the T2 morph is done, the journey stays paused */
 tap(1);run_ms(100);CHECK(!hp_j.paused&&hp_j.held==0u&&hp_rm.state==HP_RM_RECALL);
 hp_j.on=hp_j.paused=hp_j.held=0;hp_rm_reset();

 /* 4. held while resting: the rest goes on afterwards and the journey moves on */
 set_all(50,0,20,-20);hp_j.wait=2;hp_j.rev=0;bank(ACT_BANK_JOURNEY);action_snap_slot=0;tap(1);
 {uint32_t g=0;while(hp_j.phase==0u&&g++<6000u)run_ms(10);}CHECK(hp_j.phase==1u);
 {uint32_t left=hp_j.left;sel(1);bank(0u);tap(1);run_ms(200);CHECK(hp_j.held==1u&&hp_j.phase==1u&&hp_j.left<=left);}
 {uint32_t g=0;while(hp_j.held&&g++<3000u)run_ms(10);}
 run_ms(50);CHECK(hp_j.on&&hp_j.held==0u&&hp_j.phase==1u);
 {uint32_t g=0;while(hp_j.phase==1u&&g++<6000u)run_ms(10);}CHECK(hp_j.phase==0u&&hp_j.tgt==1u&&hp_rm.state==HP_RM_RECALL);
 hp_j.on=hp_j.paused=hp_j.held=0;hp_j.wait=0;hp_rm_reset();sel(0);

 /* 5. tracks left out of a journey: a snapshot of T1's HOME that pins T1 and T2 controls */
 CHECK(web_home_assign(0,0,2,HP_TRACK,1,P_REV));CHECK(web_home_assign(0,0,3,HP_TRACK,1,P_PAN));
 hp_rm_snapshot_clear();hp_rm_reset();hp_rm.bars=1;
 set_all(10,-40,15,-35);action_snap_slot=0;CHECK(hp_rm_snapshot_store(0));
 set_all(90,40,85,35);action_snap_slot=1;CHECK(hp_rm_snapshot_store(1));
 hp_j.wait=1;
 set_all(50,0,50,0);hp_j.skip=2u;                                    /* leave track 2 alone */
 bank(ACT_BANK_JOURNEY);action_snap_slot=0;tap(1);
 {uint32_t g=0;while(hp_j.phase==0u&&g++<6000u)run_ms(10);}
 CHECK(hp_j.on&&hp_j.phase==1u&&absd(trk[0].p[P_REV]-10)<=1&&trk[1].p[P_REV]==50&&trk[1].p[P_PAN]==0);          /* T1 arrived, T2 untouched */
 hp_j.on=hp_j.paused=hp_j.held=0;hp_rm_reset();
 hp_j.skip=0u;set_all(50,0,50,0);action_snap_slot=0;tap(1);
 {uint32_t g=0;while(hp_j.phase==0u&&g++<6000u)run_ms(10);}
 CHECK(hp_j.phase==1u&&absd(trk[0].p[P_REV]-10)<=1&&absd(trk[1].p[P_REV]-15)<=1);                                   /* nothing skipped: both arrive */
 hp_j.on=hp_j.paused=hp_j.held=0;hp_rm_reset();hp_j.wait=0;
 hp_j.skip=3u;set_all(50,0,50,0);action_snap_slot=0;tap(1);run_ms(300);                              /* every pinned track left out */
 CHECK(!hp_j.on&&strstr(ui.msg,"ALL TRACKS SKIPPED")!=NULL&&trk[0].p[P_REV]==50&&trk[1].p[P_REV]==50);
 hp_j.skip=0u;

 /* 6. delete a slot: RANDOM SETUP, A4 hold; outside SETUP the same hold still saves */
 bank(0u);CHECK(action_setup==0u);
 hp_rm_reset();set_all(33,0,50,0);action_snap_slot=2;hold(4,700);CHECK(hp_rm_snapshot_count(2)>0u&&strstr(ui.msg,"SAVED")!=NULL);
 hold(0,700);CHECK(action_setup==1u);
 hold(4,700);CHECK(hp_rm_snapshot_count(2)==0u&&strstr(ui.msg,"S3 DELETED")!=NULL);
 hold(4,700);CHECK(strstr(ui.msg,"S3 EMPTY")!=NULL);
 CHECK(hp_rm_snapshot_count(0)>0u&&hp_rm_snapshot_count(1)>0u);
 hold(0,700);CHECK(action_setup==0u);
 /* deleting a slot while a journey runs ends it (its set of slots changed) */
 hp_j.skip=0u;bank(ACT_BANK_JOURNEY);set_all(50,0,50,0);action_snap_slot=0;tap(1);run_ms(300);CHECK(hp_j.on);
 bank(0u);hold(0,700);action_snap_slot=1;hold(4,700);CHECK(!hp_j.on&&hp_rm_snapshot_count(1)==0u);
 hold(0,700);CHECK(action_setup==0u);
 /* 7. every slot is a scene: slots of different tracks (and an ALL slot) are one journey; a leg moves only what its target slot stores */
 hp_rm_snapshot_clear();hp_rm_reset();hp_rm.bars=1;hp_j.skip=0;hp_j.wait=1;hp_rm.scope=HP_RM_TRACK;
 hp_j.on=hp_j.paused=hp_j.held=0;
 set_all(10,-40,15,-35);sel(0);action_snap_slot=0;CHECK(hp_rm_snapshot_store(0));                   /* S1 = T1 (its HOME pins T1 and T2 controls) */
 sel(1);                                                                                           /* T2's own HOME pins T2 controls only */
 set_all(10,-40,70,30);action_snap_slot=1;CHECK(hp_rm_snapshot_store(1));                           /* S2 = T2 */
 {char o[4];hp_rm_snapshot_owner(0,o);CHECK(!strcmp(o,"T1"));hp_rm_snapshot_owner(1,o);CHECK(!strcmp(o,"T2"));hp_rm_snapshot_owner(2,o);CHECK(!strcmp(o,"-"));}
 set_all(50,0,50,0);bank(ACT_BANK_JOURNEY);action_snap_slot=0;tap(1);run_ms(100);CHECK(hp_j.on&&hp_rm.state==HP_RM_RECALL);      /* mixed owners start */
 {uint32_t g=0;while(hp_j.phase==0u&&g++<6000u)run_ms(10);}
 CHECK(hp_j.phase==1u&&absd(trk[0].p[P_REV]-10)<=1&&absd(trk[1].p[P_REV]-15)<=1);                   /* S1 moved T1 and the T2 controls T1's HOME pins */
 {int32_t t1=trk[0].p[P_REV],t1p=trk[0].p[P_PAN];
  {uint32_t g=0;while(hp_j.phase==1u&&g++<6000u)run_ms(10);}
  {uint32_t g=0;while(hp_j.phase==0u&&g++<6000u)run_ms(10);}
  CHECK(hp_j.phase==1u&&hp_j.cur==1u&&absd(trk[1].p[P_REV]-70)<=1&&absd(trk[1].p[P_PAN]-30)<=1);    /* S2 moved T2 */
  CHECK(trk[0].p[P_REV]==t1&&trk[0].p[P_PAN]==t1p);}                                                /* ... and left T1 where it stood */
 hp_j.on=hp_j.paused=hp_j.held=0;hp_rm_reset();hp_j.wait=0;
 /* an ALL slot: one snapshot of every track's HOME controls */
 hp_rm.scope=HP_RM_ALL;set_all(90,40,85,35);action_snap_slot=2;sel(0);CHECK(hp_rm_snapshot_store(2));
 {char o[4];hp_rm_snapshot_owner(2,o);CHECK(!strcmp(o,"ALL"));}
 hp_rm.scope=HP_RM_TRACK;
 /* saving over a slot that belongs to another track says so */
 sel(1);set_all(10,-40,20,-20);action_snap_slot=0;hold(4,700);CHECK(strstr(ui.msg,"WAS T1")!=NULL);
 /* issue #35: FM6's patch selector (P_E7) is never captured, whatever the HOME pins */
 {hp_ref_t r={0};r.kind=HP_TRACK;r.aux=1;r.engine=ENGI_FM6;r.id=7;r.track=0;r.page=0xFFFFu;
  CHECK(hp_rm_fm6_patch(r));r.id=6;CHECK(!hp_rm_fm6_patch(r));r.id=7;r.engine=ENGI_SAMPLE;CHECK(!hp_rm_fm6_patch(r));
  r.engine=ENGI_FM6;r.id=7;
  hp_rm_reset();hp.track[0].count=1;hp.track[0].cell[0][0]=r;hp.track[0].cell[0][1]=hp.track[0].cell[0][2]=hp.track[0].cell[0][3]=(hp_ref_t){0};
  hp_rm.scope=HP_RM_TRACK;song.sel=0;trk[0].engine=trk[0].eng_req=ENGI_FM6;
  CHECK(!hp_rm_capture(0));}
 printf("JOURNEY HOLD PASS: %u checks.\n",checks);
 return 0;
}
