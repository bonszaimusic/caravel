/* SPDX-License-Identifier: GPL-3.0-only */
/* One engine per track: morphs and journeys of different tracks run at the same time, a control two tracks move belongs to the one that
 * started last, the settings are per track and saved with the project. The firmware is booted in the host harness. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN journey_tracks_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void tap(uint32_t k){key(1u<<(ACT_KEY_FIRST+k));key(0);}   /* 0 = BANK, 1..4 = A1..A4 */
static void sel(uint32_t t){song.sel=t;go_home();action_refresh();run_ms(60);}
static void seal(hp_store_t*p){p->crc=hp_hash((uint8_t*)p+32,p->size-32);p->reserved[0]=hp_hash(p,20);}
static int cell_of(uint32_t u,uint32_t track,uint32_t id)         /* the index of engine u's cell for a track's control, -1 none */
{
 for(uint32_t i=0;i<hp_eng[u].used;i++)if(hp_eng[u].cell[i].ref.kind==HP_TRACK&&hp_eng[u].cell[i].ref.track==track&&hp_tid(hp_eng[u].cell[i].ref)==id)return (int)i;
 return -1;
}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
 for(uint32_t t=0;t<NTRK;t++)hp_apply(t,0);
 /* T1's HOME pins T1's REV and T2's PAN; T2's HOME pins T2's PAN and REV */
 CHECK(web_home_assign(0,0,0,HP_TRACK,0,P_REV));CHECK(web_home_assign(0,0,1,HP_TRACK,1,P_PAN));
 CHECK(web_home_assign(1,0,0,HP_TRACK,1,P_PAN));CHECK(web_home_assign(1,0,1,HP_TRACK,1,P_REV));
 song.sel=0;go_home();action_enabled=1;action_bank=0;action_refresh();
 for(uint32_t t=0;t<NTRK;t++){hp_eng[t].scope=HP_RM_TRACK;hp_eng[t].bars=2;hp_eng[t].depth=100;hp_eng[t].shape=HP_RM_LINEAR;}
 hp_rm_reset_all();
 trk[0].p[P_REV]=10;trk[1].p[P_PAN]=-30;trk[1].p[P_REV]=20;

 /* 1. last started wins: T1 starts, then T2 starts a morph that moves the same control (T2's PAN) */
 tap(1);run_ms(200);
 CHECK(hp_eng[0].state==HP_RM_RANDOM&&cell_of(0,1,P_PAN)>=0&&!hp_eng[0].cell[cell_of(0,1,P_PAN)].released);
 sel(1);tap(1);run_ms(100);
 CHECK(hp_eng[1].state==HP_RM_RANDOM);
 {int a=cell_of(0,1,P_PAN),b=cell_of(1,1,P_PAN),r0=cell_of(0,0,P_REV);
  CHECK(a>=0&&b>=0&&r0>=0);
  CHECK(hp_eng[0].cell[a].released&&!hp_eng[1].cell[b].released&&!hp_eng[0].cell[r0].released);   /* T1 gave the control up, kept its own REV */
  for(uint32_t i=0;i<20u;i++){run_ms(40);CHECK(trk[1].p[P_PAN]==hp_eng[1].cell[b].last);}}          /* only T2's morph writes it now */
 sel(0);tap(1);run_ms(100);                                                                          /* T1 starts again: it takes the control back */
 {int a=cell_of(0,1,P_PAN),b=cell_of(1,1,P_PAN);
  CHECK(!hp_eng[0].cell[a].released&&hp_eng[1].cell[b].released);}
 for(uint32_t t=0;t<NTRK;t++){hp_eng_ovr=(uint8_t)t;hp_rm_reset();}hp_eng_ovr=255u;

 /* 2. the settings are per track and survive a project round trip */
 hp_eng[0].bars=4;hp_eng[0].depth=25;hp_eng[0].shape=HP_RM_DRIFT;hp_jn[0].wait=3;hp_jn[0].rev=1;
 hp_eng[1].bars=16;hp_eng[1].depth=75;hp_eng[1].scope=HP_RM_ALL;hp_eng[1].shape=HP_RM_LINEAR;hp_jn[1].wait=0;hp_jn[1].rev=0;
 hp_eng[2].bars=1;hp_eng[2].depth=10;hp_eng[2].scope=HP_RM_ROW;hp_eng[2].shape=HP_RM_SMOOTH;hp_jn[2].wait=5;hp_jn[2].rev=1;
 hp_eng[3].bars=64;hp_eng[3].depth=100;hp_eng[3].shape=HP_RM_SMOOTH;hp_jn[3].wait=1;hp_jn[3].rev=0;
 {uint8_t bars[NTRK],depth[NTRK],scope[NTRK],shape[NTRK],wait[NTRK],rev[NTRK];
  for(uint32_t t=0;t<NTRK;t++){bars[t]=hp_eng[t].bars;depth[t]=hp_eng[t].depth;scope[t]=hp_eng[t].scope;shape[t]=hp_eng[t].shape;wait[t]=hp_jn[t].wait;rev[t]=hp_jn[t].rev;}
  CHECK(hp_capture(&hp_io));seal(&hp_io);
  for(uint32_t t=0;t<NTRK;t++){hp_eng[t].bars=8;hp_eng[t].depth=35;hp_eng[t].scope=HP_RM_TRACK;hp_eng[t].shape=HP_RM_SMOOTH;hp_jn[t].wait=0;hp_jn[t].rev=0;}
  CHECK(hp_restore(&hp_io));
  for(uint32_t t=0;t<NTRK;t++)CHECK(hp_eng[t].bars==bars[t]&&hp_eng[t].depth==depth[t]&&hp_eng[t].scope==scope[t]&&hp_eng[t].shape==shape[t]&&hp_jn[t].wait==wait[t]&&hp_jn[t].rev==rev[t]);}

 /* 3. SCOPE ALL fans a RANDOM key out to every track, each with its own BARS */
 for(uint32_t t=0;t<NTRK;t++){hp_eng[t].scope=HP_RM_TRACK;hp_eng[t].bars=1;}
 hp_rm_reset_all();
 CHECK(web_home_assign(2,0,0,HP_TRACK,2,P_REV));CHECK(web_home_assign(3,0,0,HP_TRACK,3,P_REV));
 sel(0);hp_rm.scope=HP_RM_ALL;hp_eng[2].bars=4;tap(1);run_ms(100);
 for(uint32_t t=0;t<NTRK;t++)CHECK(hp_eng[t].state==HP_RM_RANDOM&&hp_eng[t].count>=1u&&hp_eng[t].origin_track==t);
 CHECK(strstr(ui.msg,"4 TRACKS")!=NULL);
 run_ms(2500);
 CHECK(hp_eng[0].state==HP_RM_DONE&&hp_eng[2].state==HP_RM_RANDOM);                                 /* T3's longer BARS are its own */
 tap(3);run_ms(100);                                                                                  /* A3 with no slot: RETURN on every track */
 for(uint32_t t=0;t<NTRK;t++)CHECK(hp_eng[t].state==HP_RM_RETURN);
 printf("JOURNEY TRACKS PASS: %u checks.\n",checks);
 return 0;
}
