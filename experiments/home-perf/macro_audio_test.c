/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_HOME_PERF 1
#define PAIR_TEST_MAIN pair_contract_main
#include "../engine-pair-morph/pair_test.c"
int main(void)
{
 static const uint8_t ids[16]={P_ATK,P_DEC,P_SUS,P_REL,P_LEVEL,P_PAN,P_DIST,P_CHOR,P_DLY,P_REV,P_E1,P_E2,P_E3,P_E4,P_E5,P_E6};
 web_nor_erase();web_boot();run_ms(1400);hp_init();
 host_preset(&trk[0],ENGI_HADES,0);host_preset(&trk[1],ENGI_DRONE,5);
 track_t*t=&trk[0];int16_t before[P_COUNT];
 for(unsigned i=0;i<16;i++){const param_desc_t*d=track_desc(t,ids[i]);t->p[ids[i]]=(d->min+d->max)/2;CHECK(web_home_macro_route(0,i/4,i%4,ids[i],63));}
 memcpy(before,t->p,sizeof before);for(unsigned i=0;i<4;i++)web_home_macro_set(0,i,127);
 hp_begin(t);for(unsigned i=0;i<16;i++)CHECK(t->p[ids[i]]==track_desc(t,ids[i])->max);
 hp_end(t);CHECK(!memcmp(before,t->p,sizeof before));
 /* Duplicate targets sum before clamping. */
 memset(hp.macro[0],0,sizeof hp.macro[0]);
 CHECK(web_home_macro_route(0,0,0,P_E4,63));CHECK(web_home_macro_route(0,1,0,P_E4,-63));
 web_home_macro_set(0,0,127);web_home_macro_set(0,1,127);
 hp_begin(t);CHECK(!memcmp(before,t->p,sizeof before));hp_end(t);
 /* Inert stale engine references and zero knobs, independent tracks. */
 hp.macro[0][0].route[0].ref.engine=ENGI_DRONE;hp.macro[0][1].route[0].ref.engine=ENGI_DRONE;
 hp_begin(t);CHECK(!memcmp(before,t->p,sizeof before));hp_end(t);
 for(unsigned i=0;i<4;i++)web_home_macro_set(0,i,0);
 for(unsigned i=0;i<1000;i++){hp_begin(t);hp_begin(&trk[1]);hp_end(t);hp_end(&trk[1]);CHECK(!memcmp(before,t->p,sizeof before));}
 /* Live automation values take precedence while playing. */
 hp.macro[0][0].route[0].ref.engine=ENGI_HADES;song.playing=1;hp_macro_live[0][0]=127;
 hp_begin(t);CHECK(t->p[P_E4]==track_desc(t,P_E4)->max);hp_end(t);CHECK(!memcmp(before,t->p,sizeof before));song.playing=0;
 printf("MACRO AUDIO: %u checks passed; sixteen targets, cancellation, engine guards, zero knobs, track isolation and automation.\n",checks);return 0;
}
