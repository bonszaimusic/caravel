/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN random_return_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void random_press(void){key(1u<<(ACT_KEY_FIRST+1u));key(0);}
static void return_press(void){key(1u<<(ACT_KEY_FIRST+3u));key(0);}
static void finish(void){for(uint32_t i=0;i<8;i++)hp_rm_poll(hp_rm.stamp+1000u);CHECK(hp_rm.state==HP_RM_IDLE || hp_rm.state==HP_RM_DONE);}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
 for(uint32_t t=0;t<NTRK;t++){
  hp_apply(t,0);CHECK(web_home_assign(t,0,0,HP_TRACK,t,P_FILT));
  trk[t].p[P_FILT]=(int16_t)(10+10*t);
 }
 song.sel=0;go_home();action_enabled=1;action_bank=0;action_refresh();
 hp_rm_reset();hp_rm.scope=HP_RM_TRACK;hp_rm.bars=1;hp_rm.depth=100;
 random_press();finish();trk[0].p[P_FILT]=44;
 track_select(1);random_press();finish();trk[1].p[P_FILT]=-33;
 track_select(0);return_press();CHECK(hp_rm.state==HP_RM_RETURN);
 CHECK(hp_rm.cell[0].ref.track==0u);finish();
 CHECK(trk[0].p[P_FILT]==10);CHECK(trk[1].p[P_FILT]==-33);
 track_select(1);return_press();CHECK(hp_rm.state==HP_RM_RETURN);finish();
 CHECK(trk[1].p[P_FILT]==20);CHECK(trk[0].p[P_FILT]==10);
 /* No origin here must not redirect or stop another track's morph. */
 track_select(0);random_press();uint32_t phase=hp_rm.phase;
 track_select(3);return_press();CHECK(hp_rm.origin_track==0u);
 CHECK(hp_rm.state==HP_RM_RANDOM && hp_rm.phase>=phase);
 track_select(0);hp_rm_freeze();hp_ref_t manual=hp_rm.cell[0].ref;
 track_select(1);random_press();finish();
 hp_rm_disengage(manual);trk[0].p[P_FILT]=55;
 track_select(0);return_press();finish();CHECK(trk[0].p[P_FILT]==55);
 /* ROW origins survive switches within a track too. */
 hp_rm_reset();hp_rm.scope=HP_RM_ROW;
 CHECK(web_home_assign(0,1,0,HP_TRACK,0,P_REV));
 hp.track[0].active=0;trk[0].p[P_FILT]=12;random_press();finish();
 trk[0].p[P_FILT]=31;hp.track[0].active=1;trk[0].p[P_REV]=42;
 random_press();finish();trk[0].p[P_REV]=79;
 hp.track[0].active=0;return_press();finish();
 CHECK(trk[0].p[P_FILT]==12 && trk[0].p[P_REV]==79);
 hp.track[0].active=1;return_press();finish();CHECK(trk[0].p[P_REV]==42);
 /* Capacity exhaustion never evicts a different track's origin. */
 hp_rm_reset();hp_rm.scope=HP_RM_TRACK;
 uint32_t counts[NTRK]={0};
 for(uint32_t t=0;t<NTRK;t++){
  hp_apply(t,0);host_preset(&trk[t],ENGI_SEM,0);
  for(uint32_t id=0;id<P_COUNT;id++){
   hp_ref_t r=hp_track_ref(t,id,0xffffu);const param_desc_t*d=hp_desc(r);
   if(!hp_rm_safe(r,d)||!hp_rm_random_allowed(r,d))continue;
   uint32_t n=counts[t]++;CHECK(n<HP_ROWS*4u);
   CHECK(web_home_assign(t,n/4u,n%4u,HP_TRACK,t,id));
  }
 }
 CHECK(counts[0]*NTRK>HP_RM_MAX);
 uint32_t captured=0;
 for(uint32_t t=0;t<NTRK;t++){
  track_select(t);uint16_t used=hp_rm.used;
  int16_t before[P_COUNT];memcpy(before,trk[t].p,sizeof before);
  action_random(0);
  if(!hp_rm.count){
   CHECK(hp_rm.used==used);CHECK(!memcmp(before,trk[t].p,sizeof before));
   CHECK(strstr(ui.msg,"MEMORY FULL"));break;
  }
  captured++;finish();
 }
 CHECK(captured>0 && captured<NTRK);
 track_select(0);return_press();CHECK(hp_rm.state==HP_RM_RETURN);finish();
 CHECK(hp_rm.used<HP_RM_MAX);
 printf("RETURN TRACK PASS: %u checks; independent origins across track changes and physical A3. cell=%u state=%u bytes.\n",checks,(unsigned)sizeof(hp_rm_cell_t),(unsigned)sizeof hp_rm);
 return 0;
}
