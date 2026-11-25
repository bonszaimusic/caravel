/* SPDX-License-Identifier: GPL-3.0-only
 * Host-only A/B audition of the REAL STASIS2 + HALL stereo FX pipeline.
 * This produces an unnormalized PCM16 WAV, not a target/ISR benchmark.
 * Compile once without the experimental join, once with it, using the same
 * checkout and flags. tools/stasis2_join_audition.sh drives both builds.
 *
 * Usage: stasis_join_audition_test OUT.wav QUALITY(0/1/2)
 * Timeline: 2s LIVE prefill, ~6.14s FROZEN, then HOLD->LIVE through 12s.
 * A polyphonic pad with continuously changing phases deliberately puts
 * dissimilar audio history on the two sides of the resumed source seam.
 */
#ifndef CARAVEL_STASIS2
#error "STASIS join audition requires CARAVEL_STASIS2=1"
#endif
#if !CARAVEL_STASIS2 || !CARAVEL_STASIS_EXPAND || !CARAVEL_REVERB_MINIMAL
#error "STASIS join audition requires minimal reverb, expanded STASIS2"
#endif
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

enum { AB_SECONDS=12u };
/* Audition-only attenuation: previous four-tone input saturated the 16-bit
 * FX WAV for ~160k samples. This does not modify firmware or the A4 input.
 * Fixed compile-time value is identical for default/cubic/LUT runs.
 * If a render still clips, rebuild all three modes with a larger divisor. */
#ifndef STASIS_AB_INPUT_DIV
#define STASIS_AB_INPUT_DIV 16
#endif
#if STASIS_AB_INPUT_DIV < 1
#error "STASIS_AB_INPUT_DIV must be a positive integer"
#endif
/* Fixed block boundaries, identical for default and experimental builds. */
static uint32_t block_before(uint32_t n) { return n & ~(CTL-1u); }
static int32_t ab_pad(uint32_t sample)
{
    double t=(double)sample/(double)FS;
    double amp=0.78+0.22*sin(2.0*3.141592653589793*t*0.37);
    double x=19000.0*sin(2.0*3.141592653589793*110.0*t)
       + 9500.0*sin(2.0*3.141592653589793*164.8138*t+0.9)
       + 6000.0*sin(2.0*3.141592653589793*220.0*t+1.7)
       + 4000.0*sin(2.0*3.141592653589793*329.6276*t+2.6);
    return (int32_t)((amp*x)/(double)STASIS_AB_INPUT_DIV);
}

int main(int argc,char **argv)
{
    if(argc!=3) {
        fprintf(stderr,"usage: %s output.wav quality(0=1.5s,1=3.0s,2=5.9s)\n",argv[0]);
        return 2;
    }
    char *end=0;
    unsigned long quality=strtoul(argv[2],&end,10);
    if(!end || *end || quality>2u) {
        fprintf(stderr,"invalid quality: %s\n",argv[2]);
        return 2;
    }
    FILE *out=fopen(argv[1],"wb");
    if(!out) { perror(argv[1]); return 2; }

    const uint32_t frames=block_before(FS*AB_SECONDS);
    const uint32_t freeze_at=block_before(FS*2u);
    const uint32_t resume_at=block_before(FS*8u+FS/7u);
    host_tracks_init();
    memset(&fx,0,sizeof fx);memset(&hl,0,sizeof hl);memset(&st,0,sizeof st);
    memset(dly_buf,0,sizeof dly_buf);
    rev_clear();
    st_expand.grain=2u;           /* current 186-ms default */
    st_expand.quality=(uint8_t)quality;
    st_expand.reverse=0u;st_expand.scan=0u;st_expand.length=64u;
    song.g[G_RTYPE]=3u;song.g[G_RSIZE]=100u;song.g[G_DRREV]=0u;
    song.g[G_DRLVL]=0u;
    wav_hdr(out,frames);
    uint32_t overs=0u, max_pcm=0u;
    for(uint32_t at=0u;at<frames;at+=CTL) {
        int32_t input[CTL],zero[CTL]={0},wet[CTL]={0};
        if(at==freeze_at) song.g[G_DRLVL]=1u;
        if(at==resume_at) song.g[G_DRLVL]=0u;
        for(uint32_t i=0u;i<CTL;i++)
            input[i]=ab_pad(at+i);
        fx_buses(zero,zero,input,wet,CTL);
        for(uint32_t i=0u;i<CTL;i++) {
            int32_t left=wet[i]+rev_side[i],right=wet[i]-rev_side[i];
            /* Inspect the pre-PCM16-wav_put values: that helper clamps,
             * otherwise masking how far the host FX exceeds full scale. */
            int64_t l=left,r=right;
            uint32_t la=(uint32_t)(l<0?-l:l);
            uint32_t ra=(uint32_t)(r<0?-r:r);
            if(la>max_pcm)max_pcm=la;
            if(ra>max_pcm)max_pcm=ra;
            if(left>32767 || left<-32768)overs++;
            if(right>32767 || right<-32768)overs++;
            wav_put(out,left,right);
        }
    }
    if(fclose(out)) { perror("WAV close"); return 2; }
    printf("STASIS audition pre-PCM16: input_div=%d, max_abs=%u, "
           "out_of_range_samples=%u\n",
           STASIS_AB_INPUT_DIV,(unsigned)max_pcm,(unsigned)overs);
    printf("STASIS actual FX A/B: %s, quality=%lu, 44.1kHz stereo, "
           "frames=%u, HOLD=%u, RESUME=%u, join=%s\n",
           argv[1],quality,frames,freeze_at,resume_at,
#if defined(CARAVEL_STASIS_SOURCE_JOIN_EXPERIMENT) && CARAVEL_STASIS_SOURCE_JOIN_EXPERIMENT
           "EXPERIMENT"
#else
           "OFF"
#endif
           );
    return 0;
}
