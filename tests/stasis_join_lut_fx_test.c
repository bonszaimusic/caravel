/* SPDX-License-Identifier: GPL-3.0-only
 * Validate the shared-LUT join against the LITERAL firmware grain envelope,
 * not only an independently synthesized smoothstep approximation.
 * Requires full private checkout, engines and build/gen tables.
 */
#ifndef CARAVEL_STASIS2
#error "FX LUT test requires CARAVEL_STASIS2=1"
#endif
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "stasis_join.h"
#include "stasis_join_lut.h"
int main(void)
{
    uint32_t max_weight_error=0u,max_pcm_error=0u;
    int32_t values[]={-32768,-30000,-16000,-1,0,1,16000,30000,32767};
    for(uint32_t k=1u;k<=256u;k++)
        assert(stasis_grain_env[k]>=stasis_grain_env[k-1u]);
    assert(stasis_grain_env[0]==0u);
    assert(stasis_grain_env[128]==16384u);
    assert(stasis_grain_env[256]==32768u);
    for(uint32_t left=0u;left<=512u;left++){
        uint32_t t=512u-left;
        uint32_t x=t<<6u,x2=(x*x)>>15u,x3=(x2*x)>>15u;
        uint32_t exact=3u*x2-2u*x3;
        uint32_t k=t>>1u;
        uint32_t approx=stasis_grain_env[k];
        if(t&1u)approx=(approx+stasis_grain_env[k+1u]+1u)>>1u;
        uint32_t d=exact>approx?exact-approx:approx-exact;
        if(d>max_weight_error)max_weight_error=d;
        for(uint32_t a=0;a<sizeof values/sizeof values[0];a++)
        for(uint32_t b=0;b<sizeof values/sizeof values[0];b++){
            int32_t old=stasis_source_join(values[a],values[b],left);
            int32_t now=stasis_source_join_lut(values[a],values[b],left,
                                                 stasis_grain_env);
            uint32_t err=(uint32_t)(now>old?now-old:old-now);
            if(err>max_pcm_error)max_pcm_error=err;
            assert(now>=-32768 && now<=32767);
            assert(err<=4u);
        }
    }
    assert(max_weight_error<=2u);
    printf("STASIS actual FX shared LUT: 513 Q15 weights, "
           "max weight difference %u; PCM16 max difference %u PASS\n",
           max_weight_error,max_pcm_error);
    return 0;
}
