/* SPDX-License-Identifier: GPL-3.0-only
 * Standalone arithmetic regression. Generated Q15 LUT is verified against
 * firmware's literal stasis_grain_env[257] separately at source review.
 */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "stasis_join.h"
#include "stasis_join_lut.h"
static uint16_t env[257];
int main(void)
{
    int32_t values[]={-32768,-30000,-16384,-4096,-1,0,1,4096,16384,30000,32767};
    uint32_t max_q15_error=0u,max_pcm_error=0u;
    for(uint32_t k=0u;k<=256u;k++){
        double u=(double)k/256.0;
        env[k]=(uint16_t)llround(32768.0*(3*u*u-2*u*u*u));
        if(k)assert(env[k]>=env[k-1u]);
    }
    assert(env[0]==0u && env[128]==16384u && env[256]==32768u);
    for(uint32_t left=0u;left<=512u;left++){
        uint32_t x=(512u-left)<<6u,x2=(x*x)>>15u,x3=(x2*x)>>15u;
        uint32_t exact=3u*x2-2u*x3;
        uint32_t k=(512u-left)>>1u;
        uint32_t lut=env[k];
        if((512u-left)&1u)lut=(lut+env[k+1u]+1u)>>1u;
        uint32_t err=exact>lut?exact-lut:lut-exact;
        if(err>max_q15_error)max_q15_error=err;
        for(uint32_t a=0;a<sizeof(values)/sizeof(values[0]);a++)
        for(uint32_t b=0;b<sizeof(values)/sizeof(values[0]);b++){
            int32_t got=stasis_source_join_lut(values[a],values[b],left,env);
            int32_t expected=stasis_source_join(values[a],values[b],left);
            uint32_t delta=(uint32_t)(got>expected?got-expected:expected-got);
            if(delta>max_pcm_error)max_pcm_error=delta;
            assert(got>=-32768 && got<=32767);
            assert(delta<=4u);
            if(left==512u)assert(got==values[a]);
            if(left==0u)assert(got==values[b]);
        }
    }
    assert(max_q15_error<=2u && max_pcm_error<=4u);
    printf("STASIS HQ join LUT: 513 phases * 121 PCM pairs; Q15 max error %u, PCM16 max error %u PASS\n",
           max_q15_error,max_pcm_error);
    return 0;
}
