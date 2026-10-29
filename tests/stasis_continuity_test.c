/* SPDX-License-Identifier: GPL-3.0-only
 * Actual stereo FX PCM with a continuous 110 Hz input. A smooth source makes
 * isolated sample discontinuities distinguishable from legitimate transients.
 * Build with CARAVEL_REVERB_MINIMAL=1 and CARAVEL_STASIS_EXPAND=1.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main
static int more_slot_ok(uint32_t id) { (void)id; return 1; }

static uint32_t clock_sample, worst_curve, worst_jump;
static int32_t prev[2], slope[2];
static const char *stage;
static const char *worst_stage;
static int failed;
/* Curvature = change in adjacent-sample slope, before the master limiter.
 * A 10% input-peak bound allows ordinary grain/filter motion on this sine,
 * while rejecting the much larger overwrite-seam/capture-start impulses. */
#define MAX_CURVATURE 4000u

static void render_audio(uint32_t frames)
{
    /* HALL operates on whole CTL blocks. Include block and stage boundaries. */
    for (uint32_t at=0; at<frames; at+=CTL) {
        int32_t input[CTL], zero[CTL]={0}, wet_pcm[CTL]={0};
        for (uint32_t i=0;i<CTL;i++,clock_sample++)
            input[i]=(int32_t)(40000.0*sin(clock_sample*2.0*3.141592653589793*110.0/FS));
        fx_buses(zero,zero,input,wet_pcm,CTL);
        for(uint32_t i=0;i<CTL;i++) {
            int32_t sample[2]={wet_pcm[i]+rev_side[i],wet_pcm[i]-rev_side[i]};
            for(uint32_t ch=0;ch<2;ch++) {
                int32_t delta=sample[ch]-prev[ch];
                uint32_t jump=(uint32_t)abs(delta),curve=(uint32_t)abs(delta-slope[ch]);
                if(jump>worst_jump)worst_jump=jump;
                if(curve>worst_curve){worst_curve=curve;worst_stage=stage;}
                if(curve>MAX_CURVATURE && !failed) {
                    fprintf(stderr,"STASIS PCM discontinuity: %s ch=%u sample=%u curvature=%u\n",
                            stage,ch,clock_sample-CTL+i,curve);
                    failed=1;
                }
                prev[ch]=sample[ch];slope[ch]=delta;
            }
        }
    }
}

static void scenario(uint32_t grain,uint32_t quality,uint32_t reverse,uint32_t scan)
{
    host_tracks_init();
    memset(&fx,0,sizeof fx);memset(&hl,0,sizeof hl);memset(&st,0,sizeof st);
    memset(dly_buf,0,sizeof dly_buf);stasis_layer_reset();rev_clear();
    clock_sample=worst_curve=worst_jump=0;
    memset(prev,0,sizeof prev);memset(slope,0,sizeof slope);
    st_expand.grain=grain;st_expand.quality=quality;st_expand.reverse=reverse;st_expand.scan=scan;
    song.g[G_RTYPE]=3;song.g[G_RSIZE]=100;song.g[G_DRREV]=0;
    stage="LIVE prefill";render_audio(FS*2u);
    song.g[G_DRLVL]=1;stage="HOLD";render_audio(FS*3u);
    song.g[G_STMODE]=0;song.g[G_STAMT]=64;
    if(!stasis_layer_command(0))exit(2);
    stage="ADD and STORE";render_audio(FS*2u);
    song.g[G_STMODE]=1;song.g[G_STTIME]=1;
    if(!stasis_layer_command(0))exit(2);
    stage="BLEND and STORE";render_audio(FS*2u);
    st_expand.scan=127u-scan;st_expand.reverse=!reverse;
    stage="SCAN and REVERSE";render_audio(FS);
    int32_t next=(int32_t)((grain+1u)%3u);
    if(stasis_exp_turn(0,next-(int32_t)grain))exit(2);
    stage="GRAIN size";render_audio(FS);
    if(stasis_exp_turn(0,(int32_t)grain-next))exit(2);
    render_audio(FS);
    song.g[G_DRLVL]=0;stage="HOLD to LIVE";render_audio(FS*2u);
    st_expand.quality=(quality+1u)%3u;
    stage="WINDOW and new prefill";render_audio(FS*2u);
    printf("STASIS stereo PCM g=%u q=%u rev=%u scan=%u: jump=%u curvature=%u (%s)\n",
           grain,quality,reverse,scan,worst_jump,worst_curve,worst_stage);
}

int main(void)
{
    /* An entire reverse window must survive every write until its last
     * playback sample. Exercise the actual per-head tap selection too. */
    for(uint32_t g=0;g<3;g++)for(uint32_t q=0;q<3;q++){
        memset(&st,0,sizeof st);
        st.w=ST_RING_LEN*3u;st.valid=ST_RING_LEN;
        st.active_grain=g;st.active_quality=q;
        st_expand.reverse=1;mod_stasis.scan=127;
        st_grain_start(0,3812u>>q,4233u>>q);
        uint32_t window=stasis_grain_samples()>>q;
        uint32_t l=(st.w-st.gr_l[0])&ST_RING_MASK;
        uint32_t r=(st.w-st.gr_r[0])&ST_RING_MASK;
        if(l<window || r<window || l+window>ST_RING_LEN || r+window>ST_RING_LEN){
            fprintf(stderr,"Reverse LIVE overwrite horizon unsafe g=%u q=%u: %u,%u + %u\n",g,q,l,r,window);
            failed=1;
        }
    }
    for(uint32_t g=0;g<3;g++)for(uint32_t q=0;q<3;q++)
        for(uint32_t r=0;r<2;r++)for(uint32_t s=0;s<2;s++)
            scenario(g,q,r,s?127u:0u);
    puts(failed?"STASIS stereo continuity FAILED":"STASIS stereo continuity PASS: 36 combinations, LIVE/HOLD/ADD/BLEND/SCAN/REVERSE/GRAIN/WINDOW");
    return failed?1:0;
}
