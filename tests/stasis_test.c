/* SPDX-License-Identifier: GPL-3.0-only
 * STASIS V0.6 host smoke: real DSP source, without target hardware.
 */
#if STASIS_FULL_PROFILE
/* Exercise real HOME/PAIR/UI/storage too; PAIR shifts the appended MOD IDs. */
#define FELUCCA_HOME_PERF 1
#define FELUCCA_PERF_HW 1
#define PAIR_TEST_MAIN stasis_pair_baseline_main
#include "../experiments/engine-pair-morph/pair_test.c"
#else
#define main hostsim_main
#include "hostsim.c"
#undef main
/* This standalone host harness includes params.c but not ui.c. Its test of
 * the new unified page needs the UI's unrelated MORE-page predicate stub. */
static int more_slot_ok(uint32_t id) { (void)id; return 1; }
#endif

static int failed;
static void check_st(const char *name, int ok) {
    printf("stasis: %-63s %s\n", name, ok ? "ok" : "FAIL");
    failed += !ok;
}
static void reset_st(void) {
    host_tracks_init();
    memset(&fx, 0, sizeof fx);
    memset(&hl, 0, sizeof hl);
    memset(&st, 0, sizeof st);
    memset(dly_buf, 0, sizeof dly_buf);
    rev_clear();
    song.g[G_RTYPE] = 3;
    song.g[G_RSIZE] = 75;
    song.g[G_RDAMP] = 35;
    song.g[G_DRLVL] = 0;
    song.g[G_DRREV] = 0;
    fx.rtype = 2;
}
static uint64_t run_st(uint32_t blocks, uint32_t start, int has_input) {
    int32_t rev[CTL], zero[CTL], out[CTL];
    uint32_t i, b;
    uint64_t energy = 0u;
    for(b = 0; b < blocks; ++b) {
        for(i = 0; i < CTL; ++i) {
            uint32_t sample = (start + b) * CTL + i;
            rev[i] = has_input ? (osc_sine(sample * 25000000u) * 3) : 0;
            out[i] = zero[i] = 0;
        }
        fx_buses(zero, zero, rev, out, CTL);
        for(i=0;i<CTL;++i) {
            int64_t a = (int64_t)out[i] + rev_side[i], c = (int64_t)out[i] - rev_side[i];
            if(a > 500000 || a < -500000 || c > 500000 || c < -500000)
                failed++;
            energy += a < 0 ? (uint64_t)-a : (uint64_t)a;
            energy += c < 0 ? (uint64_t)-c : (uint64_t)c;
        }
    }
    return energy;
}
typedef struct {
    uint64_t brightness;         /* summed |high-pass of the rendered stereo output| */
    uint32_t max_jump;          /* worst adjacent-sample discontinuity */
} st_probe_t;
static st_probe_t st_probe(uint32_t blocks, uint32_t start, int input) {
    int32_t rev[CTL], zero[CTL], out[CTL], prev_l = 0, prev_r = 0;
    uint32_t i, b;
    st_probe_t p = {0,0};
    for (b = 0; b < blocks; ++b) {
        for (i = 0; i < CTL; ++i) {
            uint32_t t = (start + b) * CTL + i;
            /* Bright overtone-rich input; output must remain textured on HOLD. */
            rev[i] = input ? osc_sine(t * 25000000u) * 2 +
                osc_sine(t * 193000000u) : 0;
            out[i] = zero[i] = 0;
        }
        fx_buses(zero,zero,rev,out,CTL);
        for (i = 0; i < CTL; ++i) {
            int32_t l = out[i] + rev_side[i], r = out[i] - rev_side[i];
            uint32_t dl = (uint32_t)(l >= prev_l ? l-prev_l : prev_l-l);
            uint32_t dr = (uint32_t)(r >= prev_r ? r-prev_r : prev_r-r);
            if (dl > p.max_jump) p.max_jump = dl;
            if (dr > p.max_jump) p.max_jump = dr;
            p.brightness += dl + dr;
            prev_l = l; prev_r = r;
        }
    }
    return p;
}
static uint64_t legacy_hash(int model) {
    int32_t rev[CTL], zero[CTL], out[CTL];
    uint32_t b,i;
    uint64_t hash=1469598103934665603ULL;
    reset_st();
    song.g[G_RTYPE] = model;
    fx.rtype = 2;
    for(b=0;b<70;++b) {
        for(i=0;i<CTL;++i) {
            rev[i]=(b<5) ? osc_sine((b*CTL+i)*29000000u)*3 : 0;
            out[i]=zero[i]=0;
        }
        fx_buses(zero,zero,rev,out,CTL);
        for(i=0;i<CTL;++i) {
            hash=(hash ^ (uint32_t)out[i])*1099511628211ULL;
            hash=(hash ^ (uint32_t)rev_side[i])*1099511628211ULL;
        }
    }
    return hash;
}
int main(void) {
#if STASIS_FULL_PROFILE
    web_nor_erase(); web_boot(); run_ms(1200);
#endif
    uint64_t a,b,tail,low,high;
    uint32_t w,ph,ev0,ev1;
    reset_st();
    song.g[G_RSIZE] = 92; /* STR */
    song.g[G_DRREV] = 45; /* MOTN */
    a = run_st(1050,0,1);
    check_st("LIVE captures and plays foreground stereo texture",
             a>300000u && st.valid >= ST_MIN_VALID && st.w > ST_MIN_VALID);
    w=st.w;
    song.g[G_DRLVL]=1; /* CAPT HOLD */
    b=run_st(160,1050,0);
    check_st("CAPT HOLD immediately freezes previously recorded sound",
             st.w==w && st.auto_hold && !st.cap_pending && b>10000u);
    song.g[G_DRREV]=0; /* MOTN=0: glides down rather than jumping */
    ph=st.drift_ph;
    ev0=st.evolve_q8;
    run_st(100,1210,0);
    ev1=st.evolve_q8;
    check_st("MOTN zero smoothly approaches target without a head jump",
             ev1 < ev0 && ev1 > 0 && st.w==w);
    song.g[G_DRREV]=127;
    a=run_st(100,1310,0);
    check_st("MOTN high moves stereo scanners without instant teleport",
             st.evolve_q8 > ev1 && st.evolve_q8 < (127u << 8u) &&
             st.drift_ph!=ph && a>10000u && st.w==w);
    song.g[G_RSIZE]=0; /* texture off, dark HALL retained */
    low=run_st(160,1410,0);
    song.g[G_RSIZE]=127; /* foreground texture */
    high=run_st(160,1570,0);
    printf("stasis-level: STR 0=%llu STR 127=%llu\\n",
           (unsigned long long)low,(unsigned long long)high);
    check_st("STR dramatically raises frozen texture above normal HALL",
             high>low+(low>>1) && high>10000u && st.w==w);
    song.g[G_DRREV]=127;
    a=run_st(120,1730,0);
    check_st("MOTN combines slow filter and echo taps without stopping CAPT",
             a>10000u && st.w==w && st.gr_ready);
    {
        /* Fixed-pitch grains must not teleport across a rapid knob change:
         * head 0 cannot change its source tap except at its silent boundary. */
        uint32_t left = st.gr_l[0], phase = st.phase & (ST_GRAIN_LEN-1u);
        uint32_t max_block = (ST_GRAIN_LEN - phase) / CTL;
        if (max_block > 30u) max_block = 30u;
        if (!max_block) max_block = 1u;
        song.g[G_DRREV]=0;
        run_st(max_block,1850,0);
        check_st("Active grain tap stays fixed during MOTN jumps",
                 st.gr_l[0] == left || phase + max_block*CTL >= ST_GRAIN_LEN);
    }
    song.g[G_DRLVL]=0; /* CAPT LIVE */
    a=run_st(64,1850,1);
    check_st("HOLD->LIVE still plays the old texture while fading",
             st.w==w && st.auto_hold && st.releasing &&
             st.release_gain>0 && st.release_gain<32768 && a>10000);
    b=run_st(1750,1914,1);
    check_st("Only after the old texture fades does LIVE resume recording",
             st.w!=w && !st.auto_hold && !st.releasing && b>10000);
#if CARAVEL_REVERB_MINIMAL
    /* MOD destination contract: each source lives on a TRACK, whereas
     * STASIS parameters are shared. No change to saved song.g is allowed. */
    reset_st();
    song.g[G_RSIZE]=18; song.g[G_DRREV]=16; song.g[G_DRLVL]=0;
    trk[0].mw=127;
    trk[0].p[P_M1SRC]=MS_MODW;
    trk[0].p[P_M1DST]=MD_STSTR;
    trk[0].p[P_M1AMT]=63;
    trk[0].p[P_M2SRC]=MS_MODW;
    trk[0].p[P_M2DST]=MD_STMOTN;
    trk[0].p[P_M2AMT]=63;
    trk[0].p[P_M3SRC]=MS_MODW;
    trk[0].p[P_M3DST]=MD_STCAP;
    trk[0].p[P_M3AMT]=63;
    run_st(1030,0,1);
    check_st("MOD STR and MOTN apply to global STASIS DSP without changing project",
             mod_stasis.strength>115 && mod_stasis.motion>110 &&
             song.g[G_RSIZE]==18 && song.g[G_DRREV]==16);
    check_st("MOD CAPT holds after threshold with manual CAPT still LIVE",
             mod_stasis.capt && st.auto_hold && song.g[G_DRLVL]==0);
    w=st.w;
    trk[0].mw=62; /* beneath ON (78) but above OFF (49): hysteresis */
    run_st(16,1030,0);
    check_st("MOD CAPT hysteresis prevents chatter",mod_stasis.capt && st.w==w);
    trk[0].mw=0;
    run_st(16,1046,0);
    check_st("MOD CAPT release fades just like manual CAPT",
             !mod_stasis.capt && st.releasing && st.w==w && song.g[G_DRLVL]==0);
    trk[0].p[P_M3SRC]=0;
    run_st(1,1062,0);
    check_st("Unassigned MOD CAPT returns to saved switch setting",
             !mod_stasis.capt && !mod_stasis.capt_hyst);
#endif
    /* Regression: HOLD should not abruptly turn a sustained bright source
     * into a near-silent, low-passed cloud. MOTN knob changes must not
     * teleport playback heads into unrelated captured samples. */
    {
        st_probe_t live, held, steady, moving;
        uint32_t phase_prev, phase_next;
        reset_st();
        song.g[G_RSIZE] = 100; song.g[G_DRREV] = 50;
        st_probe(1100, 0, 1);
        live = st_probe(140, 1100, 1);
        song.g[G_DRLVL] = 1;
        held = st_probe(140, 1240, 1);
        printf("stasis-hold-brightness: live=%llu hold=%llu\\n",
               (unsigned long long)live.brightness,
               (unsigned long long)held.brightness);
        check_st("HOLD retains substantial bright stereo activity",
                 held.brightness > live.brightness / 4u && held.brightness > 10000u);
        song.g[G_DRREV] = 0;
        st_probe(700, 1380, 0); /* let existing MOTN settle */
        steady = st_probe(128, 2080, 0);
        song.g[G_DRREV] = 127;
        phase_prev = st.evolve_q8;
        moving = st_probe(128, 2208, 0);
        phase_next = st.evolve_q8;
        printf("stasis-evolve-click: steady=%u change=%u glide=%u..%u\\n",
               steady.max_jump, moving.max_jump, phase_prev, phase_next);
        check_st("MOTN automation bounds clicks versus frozen baseline",
                 moving.max_jump < steady.max_jump * 3u + 4096u);
        check_st("MOTN knob moves smoothly without full-range position jump",
                 phase_next > phase_prev && phase_next < (127u << 8u));
    }
    /* A STOP while held must release CAPT and fade grains without a hard
     * reverb-clear, even though the sequence transport is already stopped. */
    reset_st();
    song.g[G_RSIZE]=100;
    song.g[G_DRREV]=65;
    run_st(1080,0,1);
    song.g[G_DRLVL]=1;
    run_st(64,1080,0);
    check_st("CAPT HOLD engaged before STOP",st.auto_hold && st.gr_ready);
    w=st.w;
    song.playing=0;
    seq_stop();
    check_st("STOP releases CAPT HOLD and initiates musical fade",
             song.g[G_DRLVL]==0 && !st.auto_hold && st.stopping &&
             st.stop_gain==32768);
    a=run_st(256,1144,0);
    check_st("STOP stops capture and fades grains but leaves hall tail",
             st.w==w && st.stop_gain<32768 && a>10000u);
    b=run_st(2500,1400,0);
    check_st("STOP cloud eventually fades to silence without retrigger",
             st.stop_gain<200 && st.w==w);
    seq_start();
    check_st("PLAY clears stop fade for fresh capture",
             !st.stopping && st.stop_gain==32768 && st.valid==0);
        /* A pluck must leave a true long tail even before CAPT HOLD. */
    reset_st();
    run_st(24,0,1);
    tail=run_st(600,24,0);
    check_st("Long reverberant wash remains after a note-off",tail>10000u);
#if CARAVEL_REVERB_MINIMAL
    check_st("Legacy ROOM=0 renders HALL=2",legacy_hash(0)==legacy_hash(2));
    check_st("Legacy SPRING=1 renders HALL=2",legacy_hash(1)==legacy_hash(2));
    check_st("Knob skips retired ROOM/SPRING",
        param_turn(&GP[G_RTYPE],2,-1)==2 &&
        param_turn(&GP[G_RTYPE],2,1)==3 &&
        param_turn(&GP[G_RTYPE],3,-1)==2);
    check_st("CAPT, STR and MOTN controls exposed",
        GP[G_DRLVL].max==1 && GP[G_DRREV].max==127 &&
        ST_STR_DESC.max==127 && ST_MOTION_DESC.max==127 &&
        strcmp(GP[G_RTYPE].names[3],"STASIS")==0);
    {
        const page_t *pg = 0;
        int n_rev=0, n_st=0;
        int16_t *val;
        const param_desc_t *d;
        for (uint32_t k=0;k<NPAGES;k++) {
            if (!strcmp(PAGES[k].title,"REVERB")) {pg=&PAGES[k];n_rev++;}
            if (!strcmp(PAGES[k].title,"STASIS")) n_st++;
        }
        check_st("A single REVERB page replaces duplicate STASIS page",
                 n_rev==1 && n_st==0);
        if (pg) {
            song.g[G_RTYPE]=3;
            d=page_desc(pg,1,&val);
            check_st("STASIS knob 2 controls CAPT",d==&GP[G_DRLVL] && val==&song.g[G_DRLVL]);
            d=page_desc(pg,2,&val);
            check_st("STASIS knob 3 controls STR",d==&ST_STR_DESC && val==&song.g[G_RSIZE]);
            d=page_desc(pg,3,&val);
            check_st("STASIS knob 4 controls MOTN",d==&ST_MOTION_DESC && val==&song.g[G_DRREV]);
            song.g[G_RTYPE]=2;
            d=page_desc(pg,1,&val);
            check_st("HALL knob 2 still controls SIZE",d==&GP[G_RSIZE] && val==&song.g[G_RSIZE]);
            d=page_desc(pg,2,&val);
            check_st("HALL knob 3 still controls DAMP",d==&GP[G_RDAMP] && val==&song.g[G_RDAMP]);
            d=page_desc(pg,3,&val);
            check_st("HALL knob 4 is unused",d==0 && val==0);
        }
    }
#endif
    printf("stasis: %s\n", failed ? "FAILED" : "PASSED");
    return failed != 0;
}
