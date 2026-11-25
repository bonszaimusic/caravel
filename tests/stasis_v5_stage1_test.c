/* SPDX-License-Identifier: GPL-3.0-only
 * STASIS2 stage-1 host probe: test real FX DSP with the full PCM ring.
 * Build only with -DCARAVEL_REVERB_MINIMAL=1 -DCARAVEL_STASIS_EXPAND=1
 * -DCARAVEL_STASIS2=1 and the regular hostsim include flags.
 * This is a target-independent test; it does not measure FM-1 ISR time. */
#ifndef CARAVEL_STASIS2
#error "STASIS2 probe must be built with CARAVEL_STASIS2=1"
#endif
#if !CARAVEL_STASIS2 || !CARAVEL_STASIS_EXPAND || !CARAVEL_REVERB_MINIMAL
#error "STASIS2 probe requires minimal reverb and expanded capture"
#endif
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

_Static_assert(ST_RING_LEN == DLY_LEN, "V5 must occupy the complete PCM16 delay ring");
_Static_assert(ST_RING_MASK == DLY_LEN - 1u, "full ring wrapping must stay power-of-two");
_Static_assert(sizeof dly_buf == DLY_LEN * sizeof(int16_t),
               "V5 cannot expand the original 128 KiB static delay storage");

static void stage1_render(uint32_t frames, int32_t sample)
{
    int32_t zero[CTL] = {0};
    int32_t bus[CTL], wet[CTL];
    for (uint32_t n = 0; n < frames; n += CTL) {
        for (uint32_t j = 0; j < CTL; j++) {
            bus[j] = sample;
            wet[j] = 0;
        }
        fx_buses(zero, zero, bus, wet, CTL);
    }
}

static void stage1_quality(uint32_t quality)
{
    host_tracks_init();
    memset(&fx, 0, sizeof fx);
    memset(&hl, 0, sizeof hl);
    memset(&st, 0, sizeof st);
    memset(dly_buf, 0, sizeof dly_buf);
    rev_clear();

    st_expand.grain = 1;
    st_expand.quality = (int16_t)quality;
    st_expand.scan = 0;
    st_expand.reverse = 0;
    st_expand.length = 64;
    song.g[G_RTYPE] = 3;
    song.g[G_DRLVL] = 0;
    song.g[G_RSIZE] = 96;

    const uint32_t factor = 1u << quality;
    stage1_render(ST_RING_LEN * factor + CTL * 4u, 16000);
    assert(st.active_quality == quality);
    assert(st.w >= ST_RING_LEN);
    assert(st.valid == ST_RING_LEN);
    assert(dly_buf[ST_RING_LEN - 256u] != 0);
    assert(stasis_read(ST_RING_LEN + 27u) == dly_buf[27u]);
    assert(stasis_read(ST_RING_LEN * 3u + 513u) == dly_buf[513u]);
#if CARAVEL_STASIS2 && defined(CARAVEL_STASIS_SCOPE_V6) && CARAVEL_STASIS_SCOPE_V6
    /* V6 bins must be peaks of recorded PCM16, not fabricated MIDI notes.
     * Actual FX routing can scale the test input; compare against samples
     * in the real PCM ring instead of assuming a hardcoded amplitude. */
    for(uint32_t bin=0u;bin<32u;bin++){
        uint32_t max_pcm=0u;
        for(uint32_t offset=0u;offset<2048u;offset++){
            int32_t pcm=dly_buf[bin*2048u+offset];
            uint32_t level=stasis_scope_amp(pcm);
            if(level>max_pcm)max_pcm=level;
        }
        assert(st_scope_peak[bin]<=255u && st_scope_peak[bin]>0u);
        /* One extra partial sector was written; all other 31 sectors have
         * complete envelopes, so their source and UI peaks must match. */
        if(bin!=0u)
            assert(st_scope_peak[bin]==(uint8_t)(max_pcm>255u?255u:max_pcm));
    }
    uint8_t captured_peaks[32];
    for(uint32_t bin=0u;bin<32u;bin++)
        captured_peaks[bin]=st_scope_peak[bin];
#endif

    song.g[G_DRLVL] = 1; /* hold the populated source, no second writer */
    stage1_render(CTL * 2u, 0);
    assert(st.auto_hold);
    const uint32_t w = st.w;
    stage1_render(CTL * 16u, 16000);
    assert(st.w == w);
    assert(st.valid == ST_RING_LEN);
#if CARAVEL_STASIS2 && defined(CARAVEL_STASIS_SCOPE_V6) && CARAVEL_STASIS_SCOPE_V6
    for(uint32_t bin=0u;bin<32u;bin++)
        assert(st_scope_peak[bin]==captured_peaks[bin]);
#endif

    /* REGION must be a bounded, actual selected subset, not an unchecked
     * old SCAN offset or imaginary wrap-safe history. */
    st_expand.scan=127;st_expand.length=0;
    stage1_render(CTL*2u,0);
    assert(st.region_status>=STREG_FIT);
    assert(st.region_length>0u && st.region_length<ST_RING_LEN);

    /* FREEZE OFF resumes the existing writer/history. It must not clear
     * valid frames (the old firmware performed a destructive retake). */
    song.g[G_DRLVL]=0;
    stage1_render(FS*2u,16000);
    assert(!st.auto_hold && !st.releasing);
    assert(st.valid==ST_RING_LEN);
    assert(st.w>w);

    /* Explicit NEW SOURCE is destructive, but only after an audible fade;
     * keep HALL outside the source-ring reset. */
    stasis_new_source_request();
    stage1_render(CTL,16000);
    assert(st.new_source && st.valid==ST_RING_LEN);
    /* The actual fade takes ~0.385 s, leaving >65536 HQ frames when
     * rendering two full seconds: a correctly refilled full ring was
     * incorrectly rejected. Inspect the reset boundary before refill can
     * saturate, then independently verify that recording resumes. */
    uint32_t fade_samples=0u;
    while(st.new_source && fade_samples<FS) {
        stage1_render(CTL,16000);
        fade_samples+=CTL;
    }
    assert(!st.new_source && !st.releasing);
    assert(fade_samples>CTL && fade_samples<FS);
    assert(st.valid<=CTL && st.w==st.valid); /* genuine writer/history reset */
#if CARAVEL_STASIS2 && defined(CARAVEL_STASIS_SCOPE_V6) && CARAVEL_STASIS_SCOPE_V6
    /* NEW SOURCE clears the older 31 source-ring envelopes, while the
     * first active bin may already contain real resumed input. */
    for(uint32_t bin=1u;bin<32u;bin++)assert(st_scope_peak[bin]==0u);
#endif
    stage1_render((FS/4u+CTL-1u)&~(CTL-1u),16000);
    assert(st.valid>CTL && st.valid<ST_RING_LEN);
    assert(st.w==st.valid); /* source writer resumed, without second writer */

    printf("stasis2 full-ring + REGION FREEZE resume/NEW q=%u factor=%u PASS\n",
           quality, factor);
}

int main(void)
{
    /* Retired global wire IDs retain old valid value ranges; no repurposing. */
    assert(G_STMODE == G_SYNC && G_STAMT == G_INFO && G_STTIME == G_ENGGO);
    assert(GP[G_SYNC].min == 0 && GP[G_SYNC].max == 1);
    assert(GP[G_INFO].min == 0 && GP[G_INFO].max == 127);
    assert(GP[G_ENGGO].min == 1 && GP[G_ENGGO].max == 30);
    for(uint32_t quality = 0; quality < 3u; quality++)
        stage1_quality(quality);
    puts("STASIS2 stage-0+1+2 host probe PASS (no FM-1 hardware claim)");
    return 0;
}
