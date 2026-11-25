/* SPDX-License-Identifier: GPL-3.0-only
 * Actual stereo FX PCM with a continuous 110 Hz input. A smooth source makes
 * isolated sample discontinuities distinguishable from legitimate transients.
 * Build with CARAVEL_REVERB_MINIMAL=1 and CARAVEL_STASIS_EXPAND=1.
 */
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main
static int more_slot_ok(uint32_t id) { (void)id; return 1; }

static uint32_t clock_sample, worst_curve, worst_jump;
static int32_t prev[2], slope[2];
static const char *stage;
static const char *worst_stage;
static int failed, scenario_failed;
/* Curvature = change in adjacent-sample slope, before the master limiter.
 * A 10% input-peak bound allows ordinary grain/filter motion on this sine,
 * while rejecting the much larger overwrite-seam/capture-start impulses. */
#define MAX_CURVATURE 4000u

#if CARAVEL_STASIS2 && defined(CARAVEL_STASIS_PCM_TRACE)
/* The host report needs 64 samples before and after the actual impulse.
 * Production DSP and firmware builds never compile this diagnostic. */
#define STPCM_CONTEXT_N 64u
typedef struct {
    uint32_t n, writer, phase, head_l[2], head_r[2];
    uint32_t read_l[2], read_r[2];
    int32_t left, right, gain;
    uint32_t xfade;
} st_pcm_context_t;
static st_pcm_context_t st_pcm_context[STPCM_CONTEXT_N];
static uint32_t st_pcm_context_w, st_pcm_context_n, st_pcm_context_after;
static void st_pcm_context_print(const char *when, const st_pcm_context_t *p)
{
    fprintf(stderr, "STASIS2 PCM %s: sample=%u left=%d right=%d "
            "writer=%u phase=%u gain=%d xfade=%u headL=%u/%u headR=%u/%u "
            "readL=%u/%u readR=%u/%u\n",
            when, (unsigned)p->n, (int)p->left, (int)p->right,
            (unsigned)p->writer, (unsigned)p->phase, (int)p->gain,
            (unsigned)p->xfade, (unsigned)p->head_l[0],
            (unsigned)p->head_l[1], (unsigned)p->head_r[0],
            (unsigned)p->head_r[1], (unsigned)p->read_l[0],
            (unsigned)p->read_l[1], (unsigned)p->read_r[0],
            (unsigned)p->read_r[1]);
}
#endif

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
#if CARAVEL_STASIS2 && defined(CARAVEL_STASIS_PCM_TRACE)
            uint32_t not_failed_before= !scenario_failed;
            st_pcm_context_t current = {0};
            current.n=clock_sample-CTL+i;
            current.left=sample[0];current.right=sample[1];
            current.writer=st_pcm_trace[i].w;
            current.phase=st_pcm_trace[i].phase;
            current.gain=st_pcm_trace[i].gain;
            current.xfade=st_pcm_trace[i].xfade;
            current.head_l[0]=st_pcm_trace[i].gr_l[0];
            current.head_l[1]=st_pcm_trace[i].gr_l[1];
            current.head_r[0]=st_pcm_trace[i].gr_r[0];
            current.head_r[1]=st_pcm_trace[i].gr_r[1];
            current.read_l[0]=st_pcm_trace[i].read_l[0];
            current.read_l[1]=st_pcm_trace[i].read_l[1];
            current.read_r[0]=st_pcm_trace[i].read_r[0];
            current.read_r[1]=st_pcm_trace[i].read_r[1];
#endif
            for(uint32_t ch=0;ch<2;ch++) {
                int32_t delta=sample[ch]-prev[ch];
                uint32_t jump=(uint32_t)abs(delta),curve=(uint32_t)abs(delta-slope[ch]);
                if(jump>worst_jump)worst_jump=jump;
                if(curve>worst_curve){worst_curve=curve;worst_stage=stage;}
                if(curve>MAX_CURVATURE && !scenario_failed) {
                    fprintf(stderr,"STASIS PCM discontinuity: %s ch=%u sample=%u curvature=%u\n",
                            stage,ch,clock_sample-CTL+i,curve);
#if CARAVEL_STASIS2
#if defined(CARAVEL_STASIS_PCM_TRACE)
                    /* A4: this row is sampled inside the audio loop, at the
                     * exact PCM sample offset, unlike the block-end snapshot. */
                    fprintf(stderr,"STASIS2 exact-sample: grain=%u quality=%u reverse=%u scan=%u "
                            "held=%u releasing=%u release_gain=%d gain=%d gr_ready=%u xfade=%u "
                            "writer=%u valid=%u phase=%u region=%u young=%u length=%u "
                            "smooth=%d/%d headL=%u/%u headR=%u/%u readL=%u/%u readR=%u/%u\n",
                            (unsigned)st.active_grain,(unsigned)st.active_quality,
                            (unsigned)st_expand.reverse,(unsigned)st_expand.scan,
                            (unsigned)st_pcm_trace[i].auto_hold,
                            (unsigned)st_pcm_trace[i].releasing,
                            (int)st_pcm_trace[i].release_gain,
                            (int)st_pcm_trace[i].gain,
                            (unsigned)st_pcm_trace[i].gr_ready,
                            (unsigned)st_pcm_trace[i].xfade,
                            (unsigned)st_pcm_trace[i].w,
                            (unsigned)st_pcm_trace[i].valid,
                            (unsigned)st_pcm_trace[i].phase,
                            (unsigned)st_pcm_trace[i].region_status,
                            (unsigned)st_pcm_trace[i].region_young,
                            (unsigned)st_pcm_trace[i].region_length,
                            (int)st_pcm_trace[i].smooth_l,
                            (int)st_pcm_trace[i].smooth_r,
                            (unsigned)st_pcm_trace[i].gr_l[0],
                            (unsigned)st_pcm_trace[i].gr_l[1],
                            (unsigned)st_pcm_trace[i].gr_r[0],
                            (unsigned)st_pcm_trace[i].gr_r[1],
                            (unsigned)st_pcm_trace[i].read_l[0],
                            (unsigned)st_pcm_trace[i].read_l[1],
                            (unsigned)st_pcm_trace[i].read_r[0],
                            (unsigned)st_pcm_trace[i].read_r[1]);
#else
                    /* End-of-block fallback when trace is not enabled. */
                    fprintf(stderr,"STASIS2 end-of-block state: grain=%u quality=%u reverse=%u scan=%u "
                            "held=%u releasing=%u release_gain=%d gr_ready=%u xfade=%u "
                            "writer=%u valid=%u phase=%u region=%u young=%u length=%u "
                            "headL=%u/%u headR=%u/%u\n",
                            (unsigned)st.active_grain,(unsigned)st.active_quality,
                            (unsigned)st_expand.reverse,(unsigned)st_expand.scan,
                            (unsigned)st.auto_hold,(unsigned)st.releasing,
                            (int)st.release_gain,(unsigned)st.gr_ready,
                            (unsigned)st.grain_xfade,(unsigned)st.w,(unsigned)st.valid,
                            (unsigned)st.phase,(unsigned)st.region_status,
                            (unsigned)st.region_young,(unsigned)st.region_length,
                            (unsigned)st.gr_l[0],(unsigned)st.gr_l[1],
                            (unsigned)st.gr_r[0],(unsigned)st.gr_r[1]);
#endif /* CARAVEL_STASIS_PCM_TRACE */
#endif /* CARAVEL_STASIS2 */
                    scenario_failed=1; /* one diagnostic per 36-case scenario */
                    failed=1;          /* preserve global nonzero failure status */
                }
                prev[ch]=sample[ch];slope[ch]=delta;
            }
#if CARAVEL_STASIS2 && defined(CARAVEL_STASIS_PCM_TRACE)
            if(not_failed_before && scenario_failed) {
                /* Chronological samples before the threshold violation. */
                for(uint32_t k=st_pcm_context_n;k>0u;k--)
                    st_pcm_context_print("PRE",
                        &st_pcm_context[(st_pcm_context_w-k)&(STPCM_CONTEXT_N-1u)]);
                st_pcm_context_print("HIT",&current);
                st_pcm_context_after=STPCM_CONTEXT_N;
            } else if(st_pcm_context_after) {
                st_pcm_context_print("POST",&current);
                --st_pcm_context_after;
            }
            st_pcm_context[st_pcm_context_w++ & (STPCM_CONTEXT_N-1u)]=current;
            if(st_pcm_context_n<STPCM_CONTEXT_N)st_pcm_context_n++;
#endif
        }
    }
}

static void scenario(uint32_t grain,uint32_t quality,uint32_t reverse,uint32_t scan)
{
    host_tracks_init();
    memset(&fx,0,sizeof fx);memset(&hl,0,sizeof hl);memset(&st,0,sizeof st);
    memset(dly_buf,0,sizeof dly_buf);
#if !CARAVEL_STASIS2
    stasis_layer_reset();
#endif
    rev_clear();
    clock_sample=worst_curve=worst_jump=0;
    scenario_failed=0;
#if CARAVEL_STASIS2 && defined(CARAVEL_STASIS_PCM_TRACE)
    st_pcm_context_w=st_pcm_context_n=st_pcm_context_after=0u;
#endif
    memset(prev,0,sizeof prev);memset(slope,0,sizeof slope);
    st_expand.grain=grain;st_expand.quality=quality;st_expand.reverse=reverse;st_expand.scan=scan;
#if CARAVEL_STASIS2
    st_expand.length=64;
#endif
    song.g[G_RTYPE]=3;song.g[G_RSIZE]=100;song.g[G_DRREV]=0;
    stage="LIVE prefill";render_audio(FS*2u);
    song.g[G_DRLVL]=1;stage="HOLD";render_audio(FS*3u);
#if CARAVEL_STASIS2
    /* Retired layer has no second source/write head; keep elapsed time
     * comparable without scheduling obsolete ADD/BLEND operations. */
    stage="FULL RING HOLD";render_audio(FS*4u);
#else
    song.g[G_STMODE]=0;song.g[G_STAMT]=64;
    if(!stasis_layer_command(0))exit(2);
    stage="ADD and STORE";render_audio(FS*2u);
    song.g[G_STMODE]=1;song.g[G_STTIME]=1;
    if(!stasis_layer_command(0))exit(2);
    stage="BLEND and STORE";render_audio(FS*2u);
#endif
    st_expand.scan=127u-scan;st_expand.reverse=!reverse;
#if CARAVEL_STASIS2
    st_expand.length=0u;
    stage="REGION POS/LENGTH/REVERSE";render_audio(FS);
    st_expand.length=127u;
    stage="REGION LIMIT/FIT";render_audio(FS);
#else
    stage="SCAN and REVERSE";render_audio(FS);
#endif
    int32_t next=(int32_t)((grain+1u)%(uint32_t)(ST_GRAIN_MAX+1));
    if(stasis_exp_turn(0,next-(int32_t)grain))exit(2);
    stage="GRAIN size";render_audio(FS);
    if(stasis_exp_turn(0,(int32_t)grain-next))exit(2);
    render_audio(FS);
    song.g[G_DRLVL]=0;stage="HOLD to LIVE";render_audio(FS*2u);
#if CARAVEL_STASIS2
    stasis_new_source_request();
    stage="EXPLICIT NEW SOURCE";render_audio(FS*2u);
#endif
    st_expand.quality=(quality+1u)%3u;
    stage="WINDOW and new prefill";render_audio(FS*2u);
    printf("STASIS stereo PCM g=%u q=%u rev=%u scan=%u: jump=%u curvature=%u (%s)\n",
           grain,quality,reverse,scan,worst_jump,worst_curve,worst_stage);
}

int main(void)
{
    /* An entire reverse window must survive every write until its last
     * playback sample. Exercise the actual per-head tap selection too. */
    for(uint32_t g=0;g<(uint32_t)(ST_GRAIN_MAX+1);g++)for(uint32_t q=0;q<3;q++){
        memset(&st,0,sizeof st);
        st.w=ST_RING_LEN*3u;st.valid=ST_RING_LEN;
        st.active_grain=g;st.active_quality=q;
        st_expand.reverse=1;mod_stasis.scan=127;
#if CARAVEL_STASIS2
        st_expand.length=64;
        stasis_region_t region=stasis_region_plan(st.valid,ST_RING_LEN,
            stasis_grain_samples(),stasis_record_factor(),127u,64u,0u);
        assert(region.status>=STREG_FIT);
        st_grain_start(0,3812u>>q,4233u>>q,&region);
#else
        st_grain_start(0,3812u>>q,4233u>>q);
#endif
        uint32_t window=stasis_grain_samples()>>q;
        uint32_t l=(st.w-st.gr_l[0])&ST_RING_MASK;
        uint32_t r=(st.w-st.gr_r[0])&ST_RING_MASK;
        if(l<window || r<window || l+window>ST_RING_LEN || r+window>ST_RING_LEN){
            fprintf(stderr,"Reverse LIVE overwrite horizon unsafe g=%u q=%u: %u,%u + %u\n",g,q,l,r,window);
            failed=1;
        }
    }
    for(uint32_t g=0;g<(uint32_t)(ST_GRAIN_MAX+1);g++)for(uint32_t q=0;q<3;q++)
        for(uint32_t r=0;r<2;r++)for(uint32_t s=0;s<2;s++)
            scenario(g,q,r,s?127u:0u);
#if CARAVEL_STASIS2
    printf("STASIS2 stereo continuity %s: %u combinations, "
           "LIVE/HOLD/REGION/FIT/LIMIT/NEW/REVERSE/GRAIN/WINDOW\n",
           failed?"FAILED":"PASS",12u*(uint32_t)(ST_GRAIN_MAX+1u));
#else
    puts(failed?"STASIS stereo continuity FAILED":"STASIS stereo continuity PASS: 36 combinations, LIVE/HOLD/ADD/BLEND/SCAN/REVERSE/GRAIN/WINDOW");
#endif
    return failed?1:0;
}
