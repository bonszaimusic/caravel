/* SPDX-License-Identifier: GPL-3.0-only */
/* Identical harness compiled against V6 and V7; compare stdout byte for byte. */
#define FELUCCA_HOME_PERF 1
#define FELUCCA_PERF_HW 1
#define PAIR_TEST_MAIN pair_baseline_main
#include "../engine-pair-morph/pair_test.c"

static uint64_t hash;
static void capture(unsigned ms)
{
    while(ms--) {
        web_render(44);
        for(unsigned i=0;i<44;i++) {
            CHECK(isfinite(out_l[i])&&isfinite(out_r[i]));
            uint32_t l,r;memcpy(&l,&out_l[i],4);memcpy(&r,&out_r[i],4);
            hash=(hash^l)*1099511628211ull;hash=(hash^r)*1099511628211ull;
        }
    }
}
int main(void)
{
    web_nor_erase();web_boot();run_ms(1400);unsigned cases=0;
    for(unsigned partner=0;partner<2;partner++)for(unsigned topology=0;topology<6;topology++)
    for(unsigned reverse=0;reverse<2;reverse++)for(unsigned motion=0;motion<2;motion++) {
        web_keys(0);normal_off();
        unsigned engine=partner?ENGI_HADES:ENGI_SEM;
        CHECK(web_pair_prepare(0,2,reverse?ENGI_DRONE:engine,reverse?engine:ENGI_DRONE));
        unsigned cells=reverse?0:2;trk[cells].p[P_E3]=topology;ep.motion=motion;
        trk[0].p[P_DLY]=24;trk[2].p[P_REV]=24;run_ms(20);
        hash=1469598103934665603ull;
        web_pair_select(2);web_keys((1u<<0)|(1u<<4)|(1u<<7));capture(80);
        ep.base=0;capture(550);ep.base=32767;capture(550);
        ep.mute=1;capture(5);ep.mute=0;
        trk[0].p[P_MUTE]=1;capture(15);trk[0].p[P_MUTE]=0;capture(15);
        ep.base=16384;capture(450);
        web_keys((1u<<0)|(1u<<4));capture(20);
        for(unsigned i=0;i<40;i++){ep.base=(i*7919)%32768;capture(3);}
        ep.base=motion?32767:0;capture(500);
        pair_disable();capture(50);web_keys(0);capture(100);
        printf("partner=%u topology=%u reverse=%u motion=%u hash=%016llx\n",partner,topology,reverse,motion,(unsigned long long)hash);
        cases++;
    }
    fprintf(stderr,"PAIR AUDIO: %u cases, %u sample/lifecycle checks.\n",cases,checks);
    return 0;
}
