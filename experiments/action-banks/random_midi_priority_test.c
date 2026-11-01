/* SPDX-License-Identifier: GPL-3.0-only */
/* A MIDI CC that sets a parameter owns it for the rest of a running Random Morph, as a physical knob does. Before:
 * midi_cc_set wrote the value and the morph's next step wrote its own over it a moment later. */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN midi_priority_baseline_main
#include "../engine-pair-morph/pair_test.c"
static void random_press(void){key(1u<<(ACT_KEY_FIRST+1u));key(0);}
static void return_press(void){key(1u<<(ACT_KEY_FIRST+3u));key(0);}
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
 printf("MIDI CC PRIORITY PASS: %u checks.\n",checks);
 return 0;
}
