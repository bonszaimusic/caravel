/* SPDX-License-Identifier: GPL-3.0-only */
#define FELUCCA_HOME_PERF 1
#define FELUCCA_PERF_HW 1
#define PAIR_TEST_MAIN pair_baseline_main
#include "../engine-pair-morph/pair_test.c"
#include <limits.h>

static uint32_t endpoint_rng=1;
static int32_t sample(void) {endpoint_rng^=endpoint_rng<<13;endpoint_rng^=endpoint_rng>>17;endpoint_rng^=endpoint_rng<<5;return (int32_t)endpoint_rng;}

static void compare_gain(uint32_t n)
{
    for(unsigned member=0;member<2;member++)for(unsigned mute=0;mute<2;mute++) {
        int32_t actual[CTL],expect[CTL];
        for(unsigned i=0;i<n;i++) {
            int32_t x=i==0?INT32_MIN:i==1?INT32_MAX:i==2?-1:i==3?1:sample();
            actual[i]=x;expect[i]=mute?0:(int32_t)((int64_t)x*ep.weight[member][i]/32767);
        }
        ep.mute=mute;pair_apply_gain(&trk[member?ep.b:ep.a],actual,n);
        CHECK(!memcmp(actual,expect,n*sizeof actual[0]));
    }
    ep.mute=0;
}

int main(void)
{
    web_nor_erase();web_boot();run_ms(1400);prepare(ENGI_DRONE);
    static const uint16_t positions[]={0,1,74,75,76,16384,32691,32692,32766,32767};
    static const uint8_t sizes[]={1,7,16,CTL};
    unsigned modes=0;
    for(unsigned start=0;start<NELEM(positions);start++)
    for(unsigned target=0;target<NELEM(positions);target++)
    for(unsigned motion=0;motion<4;motion++)for(unsigned size=0;size<NELEM(sizes);size++) {
        unsigned n=sizes[size];ep.effective=positions[start];ep.base=positions[target];
        ep.motion=motion&1;ep.motion_mix=motion&2?32767:0;
        pair_mix_begin(n);CHECK(ep.endpoint<=2);
        for(unsigned i=0;i<n;i++) {
            CHECK(ep.weight[0][i]+ep.weight[1][i]==32767);
            if(ep.endpoint) {
                CHECK(ep.weight[0][i]==(ep.endpoint==1?32767:0));
                CHECK(ep.weight[1][i]==(ep.endpoint==2?32767:0));
            }
        }
        compare_gain(n);modes++;
    }
    /* Every possible linear weight still uses the original exact arithmetic. */
    ep.endpoint=0;
    for(unsigned w=0;w<32768;w++) {
        for(unsigned i=0;i<CTL;i++){ep.weight[0][i]=32767-w;ep.weight[1][i]=w;}
        compare_gain(CTL);
    }
    /* Fade metadata is derived from both actual weights, including pre-render silence. */
    for(unsigned scenario=0;scenario<4;scenario++) {
        normal_off();prepare(ENGI_DRONE);
        int a=scenario==0?32767:scenario==1?0:scenario==2?16383:0;
        int b=scenario==0?0:scenario==1?32767:scenario==2?16384:0;
        for(unsigned i=0;i<CTL;i++){ep.weight[0][i]=a;ep.weight[1][i]=b;}
        pair_disable();CHECK(ep.endpoint==(scenario<2?scenario+1:0));compare_gain(CTL);
    }
    printf("PAIR ENDPOINTS: %u checks passed; %u slew/motion/block cases, all 32768 weights, full int32 range samples and unpair fade.\n",checks,modes);
    return 0;
}
