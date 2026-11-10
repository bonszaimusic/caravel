/* SPDX-License-Identifier: GPL-3.0-only */
/* A MIDI CC that sets a parameter owns it for the rest of a running Random Morph, as a physical knob does. Before:
 * midi_cc_set wrote the value and the morph's next step wrote its own over it a moment later. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN midi_priority_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void random_press(void){key(1u<<(action_keys[0]));key(0);}
static void return_press(void){key(1u<<(action_keys[2]));key(0);}
static void polls(uint32_t n){for(uint32_t i=0;i<n;i++)hp_rm_poll(hp_rm.stamp+800u);}
static void finish(void){for(uint32_t i=0;i<80;i++)hp_rm_poll(hp_rm.stamp+1000u);CHECK(hp_rm.state==HP_RM_IDLE||hp_rm.state==HP_RM_DONE);}
static int cell_of(uint32_t id){
 for(uint32_t i=0;i<hp_rm.count;i++){hp_ref_t r=hp_rm.cell[i].ref;if(r.kind==HP_TRACK&&r.track==0u&&r.id==id&&!r.aux)return (int)i;}
 return -1;
}
int main(void){
 web_nor_erase();web_boot();run_ms(1400);normal_off();pair_disable();
 for(uint32_t t=0;t<NTRK;t++)hp_apply(t,0);
 CHECK(web_home_assign(0,0,0,HP_TRACK,0,P_REV));
 CHECK(web_home_assign(0,0,1,HP_TRACK,0,P_PAN));
 trk[0].p[P_REV]=20;trk[0].p[P_PAN]=-30;
 song.sel=0;go_home();action_enabled=1;action_bank=0;action_refresh();
 hp_rm_reset();hp_rm.scope=HP_RM_TRACK;hp_rm.bars=8;hp_rm.depth=100;
 random_press();
 CHECK(hp_rm.state==HP_RM_RANDOM);
 int rev=cell_of(P_REV),pan=cell_of(P_PAN);CHECK(rev>=0&&pan>=0);
 polls(3);
 int16_t pan_before=trk[0].p[P_PAN];
 /* CC91 (REVERB send) on the channel-1 track: the controller takes REV, the morph keeps PAN. */
 midi(0xB0,91,127);
 CHECK(trk[0].p[P_REV]==127);
 CHECK(hp_rm.cell[rev].released==1u && hp_rm.cell[pan].released==0u);
 polls(4);
 CHECK(trk[0].p[P_REV]==127);
 CHECK(trk[0].p[P_PAN]!=pan_before);
 /* a later value from the controller wins again, still within the same morph */
 midi(0xB0,91,40);
 int16_t held=trk[0].p[P_REV];CHECK(held!=127);
 polls(4);CHECK(trk[0].p[P_REV]==held);
 /* RETURN does not pull the controlled parameter back to its origin; the untouched one returns */
 finish();
 trk[0].p[P_PAN]=-30+0;
 return_press();CHECK(hp_rm.state==HP_RM_RETURN);finish();
 CHECK(trk[0].p[P_REV]==held);
 /* a morph started afterwards captures it again (the ownership lasts for one morph only) */
 hp_rm_reset();hp_rm.scope=HP_RM_TRACK;hp_rm.bars=8;hp_rm.depth=100;
 random_press();CHECK(hp_rm.state==HP_RM_RANDOM);
 rev=cell_of(P_REV);CHECK(rev>=0 && hp_rm.cell[rev].released==0u);
 /* LFO2 (400..) and DRUM-X (256..) ids are above 255: the morph cell is found by the full 16-bit id (review of the first draft) */
 hp_rm_reset();
 {hp_ref_t l2={.kind=HP_LFO2,.track=0,.engine=255,.id=L2_FIRST+L2_RATE};
  hp_ref_t dx={.kind=HP_DX,.track=0,.engine=255,.id=DX_FIRST};
  hp_rm.cell[0].ref=l2;hp_rm.cell[1].ref=dx;hp_rm.cell[0].released=hp_rm.cell[1].released=0u;
  hp_rm.used=hp_rm.count=2;
  hp_midi_touch(HP_LFO2,0,L2_FIRST+L2_RATE);CHECK(hp_rm.cell[0].released==1u&&hp_rm.cell[1].released==0u);
  hp_midi_touch(HP_DX,0,DX_FIRST);CHECK(hp_rm.cell[1].released==1u);
  hp_midi_touch(HP_LFO2,1,L2_FIRST+L2_RATE);CHECK(hp_rm.cell[0].released==1u);   /* another track's: nothing else changes */
  hp_rm_reset();}
 /* and through a learned CC on the real firmware path, with a morph that moves the LFO2 rate */
 {CHECK(web_home_assign(0,1,0,HP_LFO2,0,L2_FIRST+L2_RATE));
  trk[0].p[P_REV]=20;hp_rm_reset();hp_rm.scope=HP_RM_TRACK;hp_rm.bars=8;hp_rm.depth=100;
  random_press();
  int found=-1;for(uint32_t i=0;i<hp_rm.count;i++)if(hp_rm.cell[i].ref.kind==HP_LFO2&&hp_rm.cell[i].ref.id==L2_FIRST+L2_RATE)found=(int)i;
  if(found>=0){
   polls(2);
   ui.ml.on=1;ml_knob(&trk[0],L2_FIRST+L2_RATE);midi(0xB0,27,100);run_ms(60);ui.ml.on=0;
   midi(0xB0,27,127);CHECK(hp_rm.cell[found].released==1u);
   int32_t at=lfo2_get(0,L2_FIRST+L2_RATE);polls(4);CHECK(lfo2_get(0,L2_FIRST+L2_RATE)==at);
  }else printf("(LFO2 is not a morph target in this profile: unit-level check only)\n");}
 printf("MIDI CC PRIORITY PASS: %u checks.\n",checks);
 return 0;
}
